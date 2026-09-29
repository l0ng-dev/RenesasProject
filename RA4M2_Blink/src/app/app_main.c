#include "app_main.h"

#include "app_config.h"
#include "bemfa_payload.h"
#include "da16200.h"
#include "dht11.h"
#include "i2c_bus.h"
#include "mpu6050.h"
#include "oled.h"
#include "potentiometer.h"
#include "hal_data.h"

#include <stdbool.h>
#include <stdio.h>

volatile uint8_t g_attitude_state = APP_ATTITUDE_STATE_UNKNOWN;
volatile app_telemetry_t g_app_telemetry;
volatile uint8_t  g_bemfa_mqtt_publish_result;
volatile uint32_t g_bemfa_mqtt_publish_attempt_count;
volatile uint32_t g_bemfa_mqtt_publish_success_count;
volatile uint32_t g_bemfa_mqtt_publish_error_count;
volatile uint32_t g_bemfa_mqtt_last_published_sequence;

static bool     g_pot_display_cache_valid;
static uint16_t g_oled_last_display_raw;
static uint16_t g_oled_last_display_percent_x10;
static uint32_t g_sensor_refresh_elapsed_ms;
static uint32_t g_mpu6050_refresh_elapsed_ms;
static uint32_t g_dht11_refresh_elapsed_ms;
static uint32_t g_oled_page_elapsed_ms;
static uint32_t g_oled_attitude_refresh_elapsed_ms;
static uint32_t g_wifi_check_elapsed_ms;
static uint32_t g_telemetry_refresh_elapsed_ms;
static uint32_t g_bemfa_publish_elapsed_ms;
static uint32_t g_app_uptime_ms;
static uint32_t g_tilt_alarm_elapsed_ms;
static uint32_t g_tilt_recovery_elapsed_ms;
static uint32_t g_bemfa_pending_sequence;
static bool     g_oled_attitude_page;
static bool     g_bemfa_publish_pending;

static float app_abs_float (float value)
{
    return (value < 0.0F) ? -value : value;
}

static void app_reset_attitude_monitor (void)
{
    g_attitude_state = APP_ATTITUDE_STATE_UNKNOWN;
    g_tilt_alarm_elapsed_ms = 0U;
    g_tilt_recovery_elapsed_ms = 0U;
}

static void app_update_attitude_monitor (void)
{
    float const roll_abs_deg = app_abs_float(g_mpu6050_roll_deg);
    float const pitch_abs_deg = app_abs_float(g_mpu6050_pitch_deg);

    if (APP_ATTITUDE_STATE_UNKNOWN == g_attitude_state)
    {
        g_attitude_state = APP_ATTITUDE_STATE_NORMAL;
    }

    if (APP_ATTITUDE_STATE_ALERT != g_attitude_state)
    {
        g_tilt_recovery_elapsed_ms = 0U;
        if ((roll_abs_deg >= APP_TILT_ALARM_ENTER_DEG) ||
            (pitch_abs_deg >= APP_TILT_ALARM_ENTER_DEG))
        {
            g_tilt_alarm_elapsed_ms += APP_MPU6050_REFRESH_INTERVAL_MS;
            if (g_tilt_alarm_elapsed_ms >= APP_TILT_ALARM_CONFIRM_MS)
            {
                g_attitude_state = APP_ATTITUDE_STATE_ALERT;
                g_tilt_alarm_elapsed_ms = 0U;
            }
        }
        else
        {
            g_tilt_alarm_elapsed_ms = 0U;
        }
    }
    else
    {
        g_tilt_alarm_elapsed_ms = 0U;
        if ((roll_abs_deg <= APP_TILT_ALARM_EXIT_DEG) &&
            (pitch_abs_deg <= APP_TILT_ALARM_EXIT_DEG))
        {
            g_tilt_recovery_elapsed_ms += APP_MPU6050_REFRESH_INTERVAL_MS;
            if (g_tilt_recovery_elapsed_ms >= APP_TILT_RECOVERY_CONFIRM_MS)
            {
                g_attitude_state = APP_ATTITUDE_STATE_NORMAL;
                g_tilt_recovery_elapsed_ms = 0U;
            }
        }
        else
        {
            g_tilt_recovery_elapsed_ms = 0U;
        }
    }
}

static int32_t app_float_to_tenths (float value)
{
    return (int32_t) ((value >= 0.0F) ? ((value * 10.0F) + 0.5F) : ((value * 10.0F) - 0.5F));
}

static void app_telemetry_init (void)
{
    g_app_uptime_ms = 0U;
    g_telemetry_refresh_elapsed_ms = 0U;
    g_bemfa_publish_elapsed_ms = 0U;
    g_bemfa_mqtt_publish_result = DA16200_MQTT_PUBLISH_NOT_ATTEMPTED;
    g_bemfa_mqtt_publish_attempt_count = 0U;
    g_bemfa_mqtt_publish_success_count = 0U;
    g_bemfa_mqtt_publish_error_count = 0U;
    g_bemfa_mqtt_last_published_sequence = 0U;
    g_bemfa_pending_sequence = 0U;
    g_bemfa_publish_pending = false;

    g_app_telemetry.sequence = 0U;
    g_app_telemetry.uptime_ms = 0U;
    g_app_telemetry.air_temperature_x10 = 0;
    g_app_telemetry.air_humidity_x10 = 0U;
    g_app_telemetry.adc_raw = 0U;
    g_app_telemetry.adc_percent_x10 = 0U;
    g_app_telemetry.imu_temperature_x10 = 0;
    g_app_telemetry.roll_angle_x10 = 0;
    g_app_telemetry.pitch_angle_x10 = 0;
    g_app_telemetry.tilt_state = APP_ATTITUDE_STATE_UNKNOWN;
    g_app_telemetry.dht11_valid = 0U;
    g_app_telemetry.adc_valid = 0U;
    g_app_telemetry.imu_valid = 0U;
}

static void app_update_telemetry (void)
{
    bool const dht11_valid = (DHT11_STATUS_READ_OK == g_dht11_status);
    bool const adc_valid = ((1U == g_pot_adc_result) &&
                            (g_pot_sample_count > 0U) &&
                            ((uint32_t) FSP_SUCCESS == g_pot_last_fsp_error));
    bool const imu_valid = ((MPU6050_STATUS_READ_OK == g_mpu6050_status) &&
                            (0U != g_mpu6050_calibrated) &&
                            (g_mpu6050_read_count > 0U));

    g_app_telemetry.uptime_ms = g_app_uptime_ms;
    g_app_telemetry.air_temperature_x10 =
        (int16_t) (((uint16_t) g_dht11_temperature_integer * 10U) +
                   (uint16_t) g_dht11_temperature_decimal);
    g_app_telemetry.air_humidity_x10 =
        (uint16_t) (((uint16_t) g_dht11_humidity_integer * 10U) +
                    (uint16_t) g_dht11_humidity_decimal);
    g_app_telemetry.adc_raw = g_pot_raw;
    g_app_telemetry.adc_percent_x10 = g_pot_percent_x10;
    g_app_telemetry.imu_temperature_x10 =
        (int16_t) app_float_to_tenths(g_mpu6050_data.temperature_c);
    g_app_telemetry.roll_angle_x10 =
        (int16_t) app_float_to_tenths(g_mpu6050_roll_deg);
    g_app_telemetry.pitch_angle_x10 =
        (int16_t) app_float_to_tenths(g_mpu6050_pitch_deg);
    g_app_telemetry.tilt_state = imu_valid ?
                                 g_attitude_state : APP_ATTITUDE_STATE_UNKNOWN;
    g_app_telemetry.dht11_valid = dht11_valid ? 1U : 0U;
    g_app_telemetry.adc_valid = adc_valid ? 1U : 0U;
    g_app_telemetry.imu_valid = imu_valid ? 1U : 0U;
    g_app_telemetry.sequence++;
    BemfaPayload_Update(&g_app_telemetry);
}

static void app_format_signed_tenths (char * p_line,
                                      size_t line_size,
                                      char const * p_label,
                                      int32_t value_x10,
                                      char const * p_suffix)
{
    char sign = '+';
    uint32_t magnitude;

    if (value_x10 < 0)
    {
        sign = '-';
        magnitude = (uint32_t) (-(value_x10 + 1)) + 1U;
    }
    else
    {
        magnitude = (uint32_t) value_x10;
    }

    (void) snprintf(p_line,
                    line_size,
                    "%-6s%c%3lu.%1lu%-4s",
                    p_label,
                    sign,
                    (unsigned long) (magnitude / 10U),
                    (unsigned long) (magnitude % 10U),
                    p_suffix);
}

static void app_display_dht11 (void)
{
    char temperature_line[17];
    char humidity_line[17];
    fsp_err_t err;

    if (OLED_STATUS_READY != g_oled_status)
    {
        return;
    }

    if (DHT11_STATUS_READ_OK == g_dht11_status)
    {
        (void) snprintf(temperature_line,
                        sizeof(temperature_line),
                        "TEMP:%2u.%1uC     ",
                        (unsigned int) g_dht11_temperature_integer,
                        (unsigned int) g_dht11_temperature_decimal);
        (void) snprintf(humidity_line,
                        sizeof(humidity_line),
                        "HUMI:%2u.%1u%%    ",
                        (unsigned int) g_dht11_humidity_integer,
                        (unsigned int) g_dht11_humidity_decimal);
    }
    else if (DHT11_STATUS_READY == g_dht11_status)
    {
        (void) snprintf(temperature_line, sizeof(temperature_line), "DHT11 WAIT      ");
        (void) snprintf(humidity_line, sizeof(humidity_line), "NO SAMPLE       ");
    }
    else
    {
        (void) snprintf(temperature_line,
                        sizeof(temperature_line),
                        "DHT11 ERR:%u     ",
                        (unsigned int) g_dht11_status);
        (void) snprintf(humidity_line, sizeof(humidity_line), "CHECK WATCH     ");
    }

    err = OLED_ShowString(1U, 1U, temperature_line);
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(2U, 1U, humidity_line);
    }
}

static void app_display_potentiometer (void)
{
    char adc_line[17];
    char percent_line[17];
    fsp_err_t err;

    if (OLED_STATUS_READY != g_oled_status)
    {
        return;
    }

    if (g_pot_display_cache_valid &&
        (g_oled_last_display_raw == g_pot_raw) &&
        (g_oled_last_display_percent_x10 == g_pot_percent_x10))
    {
        return;
    }

    (void) snprintf(adc_line, sizeof(adc_line), "ADC:%04u        ", (unsigned int) g_pot_raw);
    (void) snprintf(percent_line,
                    sizeof(percent_line),
                    "PCT:%3u.%1u%%      ",
                    (unsigned int) (g_pot_percent_x10 / 10U),
                    (unsigned int) (g_pot_percent_x10 % 10U));

    err = OLED_ShowString(3U, 1U, adc_line);
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(4U, 1U, percent_line);
    }

    if (FSP_SUCCESS == err)
    {
        g_oled_last_display_raw = g_pot_raw;
        g_oled_last_display_percent_x10 = g_pot_percent_x10;
        g_pot_display_cache_valid = true;
    }
    else
    {
        g_pot_display_cache_valid = false;
    }
}

static void app_display_attitude (void)
{
    char roll_line[17];
    char pitch_line[17];
    char temperature_line[17];
    fsp_err_t err;

    if (OLED_STATUS_READY != g_oled_status)
    {
        return;
    }

    if ((MPU6050_STATUS_READY != g_mpu6050_status) &&
        (MPU6050_STATUS_READ_OK != g_mpu6050_status))
    {
        (void) OLED_ShowString(1U, 1U, "ROLL :---       ");
        (void) OLED_ShowString(2U, 1U, "PITCH:---       ");
        (void) OLED_ShowString(3U, 1U, "IMU DATA ERROR  ");
        (void) OLED_ShowString(4U, 1U, "CHECK WATCH     ");
        return;
    }

    app_format_signed_tenths(roll_line,
                             sizeof(roll_line),
                             "ROLL:",
                             app_float_to_tenths(g_mpu6050_roll_deg),
                             "");
    app_format_signed_tenths(pitch_line,
                             sizeof(pitch_line),
                             "PITCH:",
                             app_float_to_tenths(g_mpu6050_pitch_deg),
                             "");
    app_format_signed_tenths(temperature_line,
                             sizeof(temperature_line),
                             "IMUT:",
                             app_float_to_tenths(g_mpu6050_data.temperature_c),
                             "C");

    err = OLED_ShowString(1U, 1U, roll_line);
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(2U, 1U, pitch_line);
    }
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(3U, 1U, temperature_line);
    }
    if (FSP_SUCCESS == err)
    {
        if (APP_ATTITUDE_STATE_ALERT == g_attitude_state)
        {
            (void) OLED_ShowString(4U, 1U, "TILT:ALERT      ");
        }
        else if (APP_ATTITUDE_STATE_NORMAL == g_attitude_state)
        {
            (void) OLED_ShowString(4U, 1U, "TILT:NORMAL     ");
        }
        else
        {
            (void) OLED_ShowString(4U, 1U, "TILT:UNKNOWN    ");
        }
    }
}

static void app_display_current_page (void)
{
    if (g_oled_attitude_page)
    {
        app_display_attitude();
    }
    else
    {
        g_pot_display_cache_valid = false;
        app_display_dht11();
        app_display_potentiometer();
    }
}

static void app_service_delay (uint32_t delay_ms)
{
    while (delay_ms-- > 0U)
    {
        uint8_t mqtt_result;

        DA16200_ServiceDelay(1U);
        if (g_bemfa_publish_pending && DA16200_MQTT_TakeResult(&mqtt_result))
        {
            g_bemfa_mqtt_publish_result = mqtt_result;
            g_bemfa_publish_pending = false;
            if (DA16200_MQTT_PUBLISH_OK == mqtt_result)
            {
                g_bemfa_mqtt_publish_success_count++;
                g_bemfa_mqtt_last_published_sequence = g_bemfa_pending_sequence;
            }
            else
            {
                g_bemfa_mqtt_publish_error_count++;
            }
        }
        g_sensor_refresh_elapsed_ms++;
        g_mpu6050_refresh_elapsed_ms++;
        g_dht11_refresh_elapsed_ms++;
        g_oled_page_elapsed_ms++;
        g_oled_attitude_refresh_elapsed_ms++;
        g_wifi_check_elapsed_ms++;
        g_telemetry_refresh_elapsed_ms++;
        if (g_bemfa_publish_elapsed_ms < APP_BEMFA_PUBLISH_INTERVAL_MS)
        {
            g_bemfa_publish_elapsed_ms++;
        }
        g_app_uptime_ms++;
        if (g_mpu6050_refresh_elapsed_ms >= APP_MPU6050_REFRESH_INTERVAL_MS)
        {
            g_mpu6050_refresh_elapsed_ms = 0U;
            if ((MPU6050_STATUS_READY == g_mpu6050_status) ||
                (MPU6050_STATUS_READ_OK == g_mpu6050_status))
            {
                fsp_err_t const err =
                    MPU6050_UpdateAttitude((float) APP_MPU6050_REFRESH_INTERVAL_MS / 1000.0F);
                if (FSP_SUCCESS == err)
                {
                    app_update_attitude_monitor();
                }
                else
                {
                    app_reset_attitude_monitor();
                }
            }
            else
            {
                app_reset_attitude_monitor();
            }
        }
        if (g_sensor_refresh_elapsed_ms >= APP_SENSOR_REFRESH_INTERVAL_MS)
        {
            g_sensor_refresh_elapsed_ms = 0U;
            Potentiometer_Sample();
            if (!g_oled_attitude_page)
            {
                app_display_potentiometer();
            }
        }
        if (g_oled_attitude_page &&
            (g_oled_attitude_refresh_elapsed_ms >= APP_OLED_ATTITUDE_REFRESH_INTERVAL_MS))
        {
            g_oled_attitude_refresh_elapsed_ms = 0U;
            app_display_attitude();
        }
        if (g_dht11_refresh_elapsed_ms >= APP_DHT11_REFRESH_INTERVAL_MS)
        {
            g_dht11_refresh_elapsed_ms = 0U;
            (void) DHT11_Read();
            if (!g_oled_attitude_page)
            {
                app_display_dht11();
            }
        }
        if (g_oled_page_elapsed_ms >= APP_OLED_PAGE_INTERVAL_MS)
        {
            g_oled_page_elapsed_ms = 0U;
            g_oled_attitude_refresh_elapsed_ms = 0U;
            g_oled_attitude_page = !g_oled_attitude_page;
            app_display_current_page();
        }
        if (g_telemetry_refresh_elapsed_ms >= APP_TELEMETRY_REFRESH_INTERVAL_MS)
        {
            g_telemetry_refresh_elapsed_ms = 0U;
            app_update_telemetry();
        }
    }
}

static void app_publish_bemfa_if_due (void)
{
    uint32_t payload_sequence;

    if ((g_bemfa_publish_elapsed_ms < APP_BEMFA_PUBLISH_INTERVAL_MS) ||
        (BEMFA_PAYLOAD_RESULT_OK != g_bemfa_payload_result) ||
        g_bemfa_publish_pending || DA16200_MQTT_IsBusy() ||
        DA16200_ConnectIsBusy())
    {
        return;
    }

    payload_sequence = g_bemfa_payload_sequence;
    if (payload_sequence == g_bemfa_mqtt_last_published_sequence)
    {
        return;
    }

    if (DA16200_MQTT_RequestPublish(g_bemfa_payload))
    {
        g_bemfa_publish_elapsed_ms = 0U;
        g_bemfa_mqtt_publish_attempt_count++;
        g_bemfa_mqtt_publish_result = DA16200_MQTT_PUBLISH_NOT_ATTEMPTED;
        g_bemfa_pending_sequence = payload_sequence;
        g_bemfa_publish_pending = true;
    }
}

static void app_oled_init (void)
{
    fsp_err_t err;

    g_pot_display_cache_valid = false;
    g_oled_last_display_raw = 0U;
    g_oled_last_display_percent_x10 = 0U;
    g_sensor_refresh_elapsed_ms = 0U;
    g_mpu6050_refresh_elapsed_ms = 0U;
    g_dht11_refresh_elapsed_ms = 0U;
    g_oled_page_elapsed_ms = 0U;
    g_oled_attitude_refresh_elapsed_ms = 0U;
    g_wifi_check_elapsed_ms = 0U;
    g_telemetry_refresh_elapsed_ms = 0U;
    g_tilt_alarm_elapsed_ms = 0U;
    g_tilt_recovery_elapsed_ms = 0U;
    g_oled_attitude_page = false;
    g_attitude_state = APP_ATTITUDE_STATE_UNKNOWN;

    err = OLED_Init();
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(1U, 1U, "DHT11 WAIT");
    }
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(2U, 1U, "NO SAMPLE");
    }
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(3U, 1U, "ADC:");
    }
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(4U, 1U, "PCT:");
    }
    if (FSP_SUCCESS == err)
    {
        app_display_potentiometer();
    }
}

void App_Main (void)
{
    bool communication_ready;
    fsp_err_t mpu6050_err;

    app_telemetry_init();
    BemfaPayload_Init();
    Potentiometer_Init();
    Potentiometer_Sample();
    (void) DHT11_Init();
    (void) I2C_Bus_Init();
    app_oled_init();
    mpu6050_err = MPU6050_Init();
    if (FSP_SUCCESS == mpu6050_err)
    {
        mpu6050_err = MPU6050_CalibrateGyro();
    }
    if (FSP_SUCCESS == mpu6050_err)
    {
        mpu6050_err = MPU6050_UpdateAttitude((float) APP_MPU6050_REFRESH_INTERVAL_MS / 1000.0F);
    }
    if (FSP_SUCCESS == mpu6050_err)
    {
        app_update_attitude_monitor();
    }
    else
    {
        app_reset_attitude_monitor();
    }

    communication_ready = false;
    (void) DA16200_RequestConnect();
    (void) DHT11_Read();
    app_update_telemetry();
    app_display_dht11();

    while (1)
    {
        uint32_t blink_interval_ms = communication_ready ?
                                     APP_STATUS_LED_OK_INTERVAL_MS :
                                     APP_STATUS_LED_ERROR_INTERVAL_MS;

        (void) R_IOPORT_PinWrite(&g_ioport_ctrl,
                                 BSP_IO_PORT_01_PIN_03,
                                 BSP_IO_LEVEL_HIGH);
        app_service_delay(blink_interval_ms);

        (void) R_IOPORT_PinWrite(&g_ioport_ctrl,
                                 BSP_IO_PORT_01_PIN_03,
                                 BSP_IO_LEVEL_LOW);
        app_service_delay(blink_interval_ms);

        communication_ready = DA16200_IsReady();
        uint32_t const wifi_check_interval_ms = communication_ready ?
                                                 DA16200_STATUS_CHECK_INTERVAL_MS :
                                                 DA16200_RETRY_INTERVAL_MS;

        if ((g_wifi_check_elapsed_ms >= wifi_check_interval_ms) &&
            !DA16200_MQTT_IsBusy() && !DA16200_ConnectIsBusy())
        {
            if (DA16200_RequestConnect())
            {
                g_wifi_check_elapsed_ms = 0U;
            }
        }

        communication_ready = DA16200_IsReady();
        if (communication_ready)
        {
            app_publish_bemfa_if_due();
            communication_ready = DA16200_IsReady();
        }
    }
}

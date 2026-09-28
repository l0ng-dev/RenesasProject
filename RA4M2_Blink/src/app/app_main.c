#include "app_main.h"

#include "app_config.h"
#include "da16200.h"
#include "dht11.h"
#include "oled.h"
#include "potentiometer.h"
#include "hal_data.h"

#include <stdbool.h>
#include <stdio.h>

static bool     g_pot_display_cache_valid;
static uint16_t g_oled_last_display_raw;
static uint16_t g_oled_last_display_percent_x10;
static uint32_t g_sensor_refresh_elapsed_ms;
static uint32_t g_dht11_refresh_elapsed_ms;
static uint32_t g_wifi_check_elapsed_ms;

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
    char adc_line[9];
    char percent_line[11];
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

    (void) snprintf(adc_line, sizeof(adc_line), "ADC:%04u", (unsigned int) g_pot_raw);
    (void) snprintf(percent_line,
                    sizeof(percent_line),
                    "PCT:%3u.%1u%%",
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

static void app_service_delay (uint32_t delay_ms)
{
    while (delay_ms-- > 0U)
    {
        DA16200_ServiceDelay(1U);
        g_sensor_refresh_elapsed_ms++;
        g_dht11_refresh_elapsed_ms++;
        g_wifi_check_elapsed_ms++;
        if (g_sensor_refresh_elapsed_ms >= APP_SENSOR_REFRESH_INTERVAL_MS)
        {
            g_sensor_refresh_elapsed_ms = 0U;
            Potentiometer_Sample();
            app_display_potentiometer();
        }
        if (g_dht11_refresh_elapsed_ms >= APP_DHT11_REFRESH_INTERVAL_MS)
        {
            g_dht11_refresh_elapsed_ms = 0U;
            (void) DHT11_Read();
            app_display_dht11();
        }
    }
}

static void app_oled_init (void)
{
    fsp_err_t err;

    g_pot_display_cache_valid = false;
    g_oled_last_display_raw = 0U;
    g_oled_last_display_percent_x10 = 0U;
    g_sensor_refresh_elapsed_ms = 0U;
    g_dht11_refresh_elapsed_ms = 0U;
    g_wifi_check_elapsed_ms = 0U;

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

    Potentiometer_Init();
    Potentiometer_Sample();
    (void) DHT11_Init();
    app_oled_init();

    communication_ready = DA16200_Connect();
    (void) DHT11_Read();
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

        uint32_t const wifi_check_interval_ms = communication_ready ?
                                                 DA16200_STATUS_CHECK_INTERVAL_MS :
                                                 DA16200_RETRY_INTERVAL_MS;

        if (g_wifi_check_elapsed_ms >= wifi_check_interval_ms)
        {
            g_wifi_check_elapsed_ms = 0U;
            communication_ready = DA16200_Connect();
        }
        else
        {
            communication_ready = DA16200_IsReady();
        }
    }
}

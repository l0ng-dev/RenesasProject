#include "app_main.h"

#include "app_config.h"
#include "da16200.h"
#include "oled.h"
#include "potentiometer.h"
#include "hal_data.h"

#include <stdio.h>

/* OLED application result: 0 not initialized, 1 display updated,
 * 2 initialization failed, 3 display update failed. */
volatile uint8_t  g_oled_display_result;
volatile uint16_t g_oled_last_display_raw;
volatile uint16_t g_oled_last_display_percent_x10;
static uint32_t g_sensor_refresh_elapsed_ms;
static uint32_t g_wifi_check_elapsed_ms;

static void app_display_potentiometer (void)
{
    char adc_line[9];
    char percent_line[11];
    fsp_err_t err;

    if (OLED_STATUS_READY != g_oled_status)
    {
        return;
    }

    if ((1U == g_oled_display_result) &&
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

    err = OLED_ShowString(2U, 1U, adc_line);
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(3U, 1U, percent_line);
    }

    if (FSP_SUCCESS == err)
    {
        g_oled_last_display_raw = g_pot_raw;
        g_oled_last_display_percent_x10 = g_pot_percent_x10;
        g_oled_display_result = 1U;
    }
    else
    {
        g_oled_display_result = 3U;
    }
}

static void app_service_delay (uint32_t delay_ms)
{
    while (delay_ms-- > 0U)
    {
        DA16200_ServiceDelay(1U);
        g_sensor_refresh_elapsed_ms++;
        g_wifi_check_elapsed_ms++;
        if (g_sensor_refresh_elapsed_ms >= APP_SENSOR_REFRESH_INTERVAL_MS)
        {
            g_sensor_refresh_elapsed_ms = 0U;
            Potentiometer_Sample();
            app_display_potentiometer();
        }
    }
}

static void app_oled_init (void)
{
    fsp_err_t err;

    g_oled_display_result = 0U;
    g_oled_last_display_raw = 0U;
    g_oled_last_display_percent_x10 = 0U;
    g_sensor_refresh_elapsed_ms = 0U;
    g_wifi_check_elapsed_ms = 0U;

    err = OLED_Init();
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(1U, 1U, "RA4M2 SENSOR");
    }
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(2U, 1U, "ADC:");
    }
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(3U, 1U, "PCT:");
    }
    if (FSP_SUCCESS == err)
    {
        err = OLED_ShowString(4U, 1U, "I2C OLED OK");
    }
    if (FSP_SUCCESS == err)
    {
        app_display_potentiometer();
    }
    else
    {
        g_oled_display_result = 2U;
    }
}

void App_Main (void)
{
    bool communication_ready;

    Potentiometer_Init();
    Potentiometer_Sample();
    app_oled_init();

    communication_ready = DA16200_Connect();

    while (1)
    {
        uint32_t blink_interval_ms = communication_ready ?
                                     APP_STATUS_LED_OK_INTERVAL_MS :
                                     APP_STATUS_LED_ERROR_INTERVAL_MS;

        Potentiometer_Sample();
        app_display_potentiometer();

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

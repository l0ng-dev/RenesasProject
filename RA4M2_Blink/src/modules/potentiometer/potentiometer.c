#include "potentiometer.h"
#include "app_config.h"

volatile uint8_t  g_pot_adc_result;
volatile uint32_t g_pot_last_fsp_error;
volatile uint16_t g_pot_raw;
volatile uint16_t g_pot_min;
volatile uint16_t g_pot_max;
volatile uint16_t g_pot_percent_x10;
volatile uint32_t g_pot_sample_count;

void Potentiometer_Init (void)
{
    fsp_err_t err;

    g_pot_adc_result = 0U;
    g_pot_last_fsp_error = (uint32_t) FSP_SUCCESS;
    g_pot_raw = 0U;
    g_pot_min = 0U;
    g_pot_max = 0U;
    g_pot_percent_x10 = 0U;
    g_pot_sample_count = 0U;

    err = g_adc0.p_api->open(g_adc0.p_ctrl, g_adc0.p_cfg);
    if (FSP_SUCCESS != err)
    {
        g_pot_adc_result = 2U;
        g_pot_last_fsp_error = (uint32_t) err;
        return;
    }

    err = g_adc0.p_api->scanCfg(g_adc0.p_ctrl, g_adc0.p_channel_cfg);
    if (FSP_SUCCESS != err)
    {
        g_pot_adc_result = 3U;
        g_pot_last_fsp_error = (uint32_t) err;
        (void) g_adc0.p_api->close(g_adc0.p_ctrl);
        return;
    }

    g_pot_adc_result = 1U;
}

void Potentiometer_Sample (void)
{
    adc_status_t status = {0};
    uint16_t raw = 0U;
    fsp_err_t err;
    uint32_t remaining = POTENTIOMETER_ADC_POLL_LIMIT;

    if (1U != g_pot_adc_result)
    {
        return;
    }

    err = g_adc0.p_api->scanStart(g_adc0.p_ctrl);
    if (FSP_SUCCESS != err)
    {
        g_pot_adc_result = 4U;
        g_pot_last_fsp_error = (uint32_t) err;
        return;
    }

    do
    {
        err = g_adc0.p_api->scanStatusGet(g_adc0.p_ctrl, &status);
        if (FSP_SUCCESS != err)
        {
            g_pot_adc_result = 5U;
            g_pot_last_fsp_error = (uint32_t) err;
            return;
        }
        remaining--;
    } while ((ADC_STATE_SCAN_IN_PROGRESS == status.state) && (0U != remaining));

    if (ADC_STATE_SCAN_IN_PROGRESS == status.state)
    {
        g_pot_adc_result = 5U;
        g_pot_last_fsp_error = (uint32_t) FSP_ERR_TIMEOUT;
        return;
    }

    err = g_adc0.p_api->read(g_adc0.p_ctrl, ADC_CHANNEL_11, &raw);
    if (FSP_SUCCESS != err)
    {
        g_pot_adc_result = 6U;
        g_pot_last_fsp_error = (uint32_t) err;
        return;
    }

    g_pot_raw = raw;
    if (0U == g_pot_sample_count)
    {
        g_pot_min = raw;
        g_pot_max = raw;
    }
    else
    {
        if (raw < g_pot_min)
        {
            g_pot_min = raw;
        }
        if (raw > g_pot_max)
        {
            g_pot_max = raw;
        }
    }

    g_pot_percent_x10 = (uint16_t) ((((uint32_t) raw * 1000U) +
                                      (POTENTIOMETER_ADC_MAX_CODE / 2U)) /
                                     POTENTIOMETER_ADC_MAX_CODE);
    g_pot_sample_count++;
    g_pot_last_fsp_error = (uint32_t) FSP_SUCCESS;
}

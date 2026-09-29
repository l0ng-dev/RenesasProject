#include "bemfa_payload.h"

#include <stddef.h>
#include <stdio.h>

char              g_bemfa_payload[BEMFA_PAYLOAD_BUFFER_SIZE];
volatile uint16_t g_bemfa_payload_length;
volatile uint8_t  g_bemfa_payload_result;
volatile uint8_t  g_bemfa_payload_valid_mask;
volatile uint32_t g_bemfa_payload_sequence;
volatile uint32_t g_bemfa_payload_format_count;
volatile uint32_t g_bemfa_payload_error_count;

static uint32_t bemfa_abs_tenths (int16_t value)
{
    int32_t const wide_value = (int32_t) value;

    if (wide_value < 0)
    {
        return (uint32_t) (-(wide_value + 1)) + 1U;
    }

    return (uint32_t) wide_value;
}

static char const * bemfa_sign (int16_t value)
{
    return (value < 0) ? "-" : "";
}

void BemfaPayload_Init (void)
{
    g_bemfa_payload[0] = '\0';
    g_bemfa_payload_length = 0U;
    g_bemfa_payload_result = BEMFA_PAYLOAD_RESULT_NOT_READY;
    g_bemfa_payload_valid_mask = 0U;
    g_bemfa_payload_sequence = 0U;
    g_bemfa_payload_format_count = 0U;
    g_bemfa_payload_error_count = 0U;
}

void BemfaPayload_Update (volatile app_telemetry_t const * p_telemetry)
{
    uint8_t valid_mask = 0U;
    int written;

    if (NULL == p_telemetry)
    {
        g_bemfa_payload[0] = '\0';
        g_bemfa_payload_length = 0U;
        g_bemfa_payload_result = BEMFA_PAYLOAD_RESULT_FORMAT_ERROR;
        g_bemfa_payload_error_count++;
        return;
    }

    if (0U != p_telemetry->dht11_valid)
    {
        valid_mask |= BEMFA_PAYLOAD_VALID_DHT11;
    }
    if (0U != p_telemetry->adc_valid)
    {
        valid_mask |= BEMFA_PAYLOAD_VALID_ADC;
    }
    if (0U != p_telemetry->imu_valid)
    {
        valid_mask |= BEMFA_PAYLOAD_VALID_IMU;
    }

    written = snprintf(g_bemfa_payload,
                       sizeof(g_bemfa_payload),
                       "{\"seq\":%lu,\"up\":%lu,\"t\":%s%lu.%lu,\"h\":%lu.%lu,"
                       "\"adc\":%u,\"pct\":%lu.%lu,\"roll\":%s%lu.%lu,"
                       "\"pitch\":%s%lu.%lu,\"imut\":%s%lu.%lu,\"tilt\":%u,\"valid\":%u}",
                       (unsigned long) p_telemetry->sequence,
                       (unsigned long) p_telemetry->uptime_ms,
                       bemfa_sign(p_telemetry->air_temperature_x10),
                       (unsigned long) (bemfa_abs_tenths(p_telemetry->air_temperature_x10) / 10U),
                       (unsigned long) (bemfa_abs_tenths(p_telemetry->air_temperature_x10) % 10U),
                       (unsigned long) (p_telemetry->air_humidity_x10 / 10U),
                       (unsigned long) (p_telemetry->air_humidity_x10 % 10U),
                       (unsigned int) p_telemetry->adc_raw,
                       (unsigned long) (p_telemetry->adc_percent_x10 / 10U),
                       (unsigned long) (p_telemetry->adc_percent_x10 % 10U),
                       bemfa_sign(p_telemetry->roll_angle_x10),
                       (unsigned long) (bemfa_abs_tenths(p_telemetry->roll_angle_x10) / 10U),
                       (unsigned long) (bemfa_abs_tenths(p_telemetry->roll_angle_x10) % 10U),
                       bemfa_sign(p_telemetry->pitch_angle_x10),
                       (unsigned long) (bemfa_abs_tenths(p_telemetry->pitch_angle_x10) / 10U),
                       (unsigned long) (bemfa_abs_tenths(p_telemetry->pitch_angle_x10) % 10U),
                       bemfa_sign(p_telemetry->imu_temperature_x10),
                       (unsigned long) (bemfa_abs_tenths(p_telemetry->imu_temperature_x10) / 10U),
                       (unsigned long) (bemfa_abs_tenths(p_telemetry->imu_temperature_x10) % 10U),
                       (unsigned int) p_telemetry->tilt_state,
                       (unsigned int) valid_mask);

    g_bemfa_payload_valid_mask = valid_mask;
    g_bemfa_payload_sequence = p_telemetry->sequence;
    g_bemfa_payload_format_count++;

    if (written < 0)
    {
        g_bemfa_payload[0] = '\0';
        g_bemfa_payload_length = 0U;
        g_bemfa_payload_result = BEMFA_PAYLOAD_RESULT_FORMAT_ERROR;
        g_bemfa_payload_error_count++;
    }
    else if ((size_t) written >= sizeof(g_bemfa_payload))
    {
        g_bemfa_payload_length = (uint16_t) (sizeof(g_bemfa_payload) - 1U);
        g_bemfa_payload_result = BEMFA_PAYLOAD_RESULT_TRUNCATED;
        g_bemfa_payload_error_count++;
    }
    else
    {
        g_bemfa_payload_length = (uint16_t) written;
        g_bemfa_payload_result = BEMFA_PAYLOAD_RESULT_OK;
    }
}

#ifndef BEMFA_PAYLOAD_H
#define BEMFA_PAYLOAD_H

#include "app_main.h"

#include <stdint.h>

#define BEMFA_PAYLOAD_BUFFER_SIZE          (192U)

#define BEMFA_PAYLOAD_RESULT_NOT_READY     (0U)
#define BEMFA_PAYLOAD_RESULT_OK            (1U)
#define BEMFA_PAYLOAD_RESULT_TRUNCATED     (2U)
#define BEMFA_PAYLOAD_RESULT_FORMAT_ERROR  (3U)

#define BEMFA_PAYLOAD_VALID_DHT11          (1U << 0)
#define BEMFA_PAYLOAD_VALID_ADC            (1U << 1)
#define BEMFA_PAYLOAD_VALID_IMU            (1U << 2)

extern char              g_bemfa_payload[BEMFA_PAYLOAD_BUFFER_SIZE];
extern volatile uint16_t g_bemfa_payload_length;
extern volatile uint8_t  g_bemfa_payload_result;
extern volatile uint8_t  g_bemfa_payload_valid_mask;
extern volatile uint32_t g_bemfa_payload_sequence;
extern volatile uint32_t g_bemfa_payload_format_count;
extern volatile uint32_t g_bemfa_payload_error_count;

void BemfaPayload_Init (void);
void BemfaPayload_Update (volatile app_telemetry_t const * p_telemetry);

#endif

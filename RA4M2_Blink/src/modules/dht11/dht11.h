#ifndef DHT11_H
#define DHT11_H

#include "hal_data.h"
#include <stdint.h>

#define DHT11_STATUS_NOT_INITIALIZED       (0U)
#define DHT11_STATUS_READY                 (1U)
#define DHT11_STATUS_READ_OK               (2U)
#define DHT11_STATUS_START_FAILED          (3U)
#define DHT11_STATUS_RESPONSE_TIMEOUT      (4U)
#define DHT11_STATUS_DATA_TIMEOUT          (5U)
#define DHT11_STATUS_CHECKSUM_FAILED       (6U)

extern volatile uint8_t  g_dht11_status;
extern volatile uint32_t g_dht11_last_fsp_error;
extern volatile uint8_t  g_dht11_humidity_integer;
extern volatile uint8_t  g_dht11_humidity_decimal;
extern volatile uint8_t  g_dht11_temperature_integer;
extern volatile uint8_t  g_dht11_temperature_decimal;
extern volatile uint8_t  g_dht11_checksum;
extern volatile uint8_t  g_dht11_raw_data[5];
extern volatile uint8_t  g_dht11_stage;
extern volatile uint8_t  g_dht11_idle_level;
extern volatile uint8_t  g_dht11_release_level;
extern volatile uint8_t  g_dht11_last_level;
extern volatile uint32_t g_dht11_read_count;
extern volatile uint32_t g_dht11_timeout_count;
extern volatile uint32_t g_dht11_checksum_error_count;

fsp_err_t DHT11_Init (void);
fsp_err_t DHT11_Read (void);

#endif

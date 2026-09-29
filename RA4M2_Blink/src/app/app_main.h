#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>

typedef struct st_app_telemetry
{
    uint32_t sequence;
    uint32_t uptime_ms;
    int16_t  air_temperature_x10;
    uint16_t air_humidity_x10;
    uint16_t adc_raw;
    uint16_t adc_percent_x10;
    int16_t  imu_temperature_x10;
    int16_t  roll_angle_x10;
    int16_t  pitch_angle_x10;
    uint8_t  tilt_state;
    uint8_t  dht11_valid;
    uint8_t  adc_valid;
    uint8_t  imu_valid;
} app_telemetry_t;

extern volatile app_telemetry_t g_app_telemetry;
extern volatile uint8_t  g_bemfa_mqtt_publish_result;
extern volatile uint32_t g_bemfa_mqtt_publish_attempt_count;
extern volatile uint32_t g_bemfa_mqtt_publish_success_count;
extern volatile uint32_t g_bemfa_mqtt_publish_error_count;
extern volatile uint32_t g_bemfa_mqtt_last_published_sequence;

void App_Main (void);

#endif

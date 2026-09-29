#ifndef DA16200_H
#define DA16200_H

#include "hal_data.h"
#include <stdbool.h>
#include <stdint.h>

#define DA16200_MQTT_PUBLISH_NOT_ATTEMPTED       (0U)
#define DA16200_MQTT_PUBLISH_OK                  (1U)
#define DA16200_MQTT_PUBLISH_INVALID_PAYLOAD     (2U)
#define DA16200_MQTT_PUBLISH_NOT_READY           (3U)
#define DA16200_MQTT_PUBLISH_SESSION_FAILED      (4U)
#define DA16200_MQTT_PUBLISH_WIFI_DISCONNECTED   (5U)
#define DA16200_MQTT_PUBLISH_MQTT_DISCONNECTED   (6U)
#define DA16200_MQTT_PUBLISH_COMMAND_FAILED      (7U)
#define DA16200_MQTT_PUBLISH_ACK_TIMEOUT         (8U)
#define DA16200_MQTT_PUBLISH_BROKER_REJECTED     (9U)
#define DA16200_MQTT_PUBLISH_DPM_RESTORE_FAILED  (10U)
#define DA16200_MQTT_PUBLISH_CONFIG_INVALID      (11U)
#define DA16200_MQTT_PUBLISH_CONFIG_FAILED       (12U)

bool DA16200_Connect (void);
bool DA16200_RequestConnect (void);
void DA16200_ConnectService (void);
bool DA16200_ConnectIsBusy (void);
bool DA16200_IsReady (void);
uint8_t DA16200_MQTT_Publish (char const * p_payload);
bool DA16200_MQTT_RequestPublish (char const * p_payload);
void DA16200_MQTT_Service (void);
bool DA16200_MQTT_IsBusy (void);
bool DA16200_MQTT_TakeResult (uint8_t * p_result);
void DA16200_ServiceDelay (uint32_t delay_ms);

void user_uart_callback (uart_callback_args_t * p_args);

#endif

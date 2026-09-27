#ifndef STAGE2_CONFIG_H
#define STAGE2_CONFIG_H

/*
 * Stage 2 is deliberately disabled in the shared build. Enabling it causes
 * the firmware to publish the fixed historical replay set after a successful
 * DPM wake handshake. Applying MQTT configuration also writes DA16200 NVRAM.
 * Keep secrets in the ignored stage2_local_config.h file only.
 */
#if defined(__has_include)
#if __has_include("stage2_local_config.h")
#include "stage2_local_config.h"
#endif
#endif

#ifndef DA16200_STAGE2_ENABLE
#define DA16200_STAGE2_ENABLE                 (0U)
#endif

#ifndef DA16200_STAGE2_APPLY_MQTT_CONFIG
#define DA16200_STAGE2_APPLY_MQTT_CONFIG      (0U)
#endif

#ifndef DA16200_MQTT_BROKER_HOST
#define DA16200_MQTT_BROKER_HOST              "bemfa.com"
#endif

#ifndef DA16200_MQTT_BROKER_PORT
#define DA16200_MQTT_BROKER_PORT              (9501U)
#endif

#ifndef DA16200_MQTT_SUB_TOPIC
#define DA16200_MQTT_SUB_TOPIC                ""
#endif

#ifndef DA16200_MQTT_PUB_TOPIC
#define DA16200_MQTT_PUB_TOPIC                ""
#endif

#ifndef DA16200_MQTT_CLIENT_ID
#define DA16200_MQTT_CLIENT_ID                ""
#endif

#endif

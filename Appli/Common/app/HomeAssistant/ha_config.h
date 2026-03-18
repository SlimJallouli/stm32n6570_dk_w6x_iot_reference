#pragma once

/* Standard includes. */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* MQTT includes. */
#include "core_mqtt.h"
#include "core_mqtt_agent.h"

/* OTA version types. */
#include "ota_appversion32.h"

/*-----------------------------------------------------------*/
/* Common configuration constants (pulled from the original file). */

/** @brief Maximum length for MQTT topic strings used by Home Assistant. */
#define HA_CONFIG_MAX_TOPIC_LENGTH                  ( 128 )

/** @brief Delay between MQTT publish operations to avoid flooding the agent. */
#define HA_MQTT_PUBLISH_TIME_BETWEEN_MS             ( 10 )

/** @brief Max time to block waiting to queue a command to the MQTT agent. */
#define HA_CONFIG_MAX_COMMAND_SEND_BLOCK_TIME_MS    ( 500 )

/** @brief Size of the payload buffer used for MQTT message bodies. */
#define HA_CONFIG_PAYLOAD_BUFFER_LENGTH             ( 1024 )

/** @brief Milliseconds in one hour. */
#define HA_MS_PER_HOUR                              ( 60UL * 60UL * 1000UL )

/** @brief Base timeout used for jittered waits in the HA task. */
#define HA_BASE_TIMEOUT_MS                          ( 24UL * HA_MS_PER_HOUR )

/** @brief Jitter range applied to the base timeout. */
#define HA_JITTER_RANGE_MS                          ( 1UL * HA_MS_PER_HOUR )

/*-----------------------------------------------------------*/
/* Shared, reusable topic buffer. Allocated at runtime by the main task. */
extern char * pcPublishTopic;

/* Keep a compatible alias used in the original code. */
#define configPUBLISH_TOPIC pcPublishTopic

#include "ha_entities_imu.h"

#include <stdio.h>
#include <string.h>

#ifndef BOARD
    #define BOARD "UNKNOWN"
#endif

/* Home Assistant device automation entity type */
static const char * entity = "device_automation";

/* IMU event descriptors */
typedef struct IMUDescriptor_t
{
    const char * name;        /* Friendly name */
    const char * type;        /* HA trigger type */
    const char * subtype;     /* HA trigger subtype */
    BaseType_t enabled;
} IMUDescriptor_t;

/* These must match your IMU task payloads */
static const IMUDescriptor_t xIMUEvents[] =
{
    { "free_fall",   "free_fall",   "imu", pdTRUE },
    { "double_tap",  "double_tap",  "imu", pdTRUE },
    { "single_tap",  "single_tap",  "imu", pdTRUE },
    { "wake_up",     "wake_up",     "imu", pdTRUE }
};

#define ARRAY_SIZE(arr) ( sizeof(arr) / sizeof(arr[0]) )
#define MQTT_PUBLISH_TOPIC_EVENT          "imu/event"

/* -------------------------------------------------------------------------- */
/* Clear IMU Discovery Config                                                 */
/* -------------------------------------------------------------------------- */

void HA_IMU_ClearConfig( const char * pcThingName )
{
    configASSERT( pcThingName != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    for( int i = 0; i < (int) ARRAY_SIZE( xIMUEvents ); i++ )
    {
        const IMUDescriptor_t * pxIMU = &xIMUEvents[i];

        /* Build discovery topic */
        (void) snprintf( configPUBLISH_TOPIC,
                         HA_CONFIG_MAX_TOPIC_LENGTH,
                         "homeassistant/%s/%s_%s/config",
                         entity,
                         pcThingName,
                         pxIMU->name );

        /* Clear retained config */
        (void) HA_ClearRetainedTopic( configPUBLISH_TOPIC );

        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }
}

/* -------------------------------------------------------------------------- */
/* Publish IMU Discovery Config                                               */
/* -------------------------------------------------------------------------- */

void HA_IMU_PublishConfig( const char * pcThingName,
                           char * pcPayloadBuffer )
{
#if (DEMO_MOTION_IMU == 1)
    configASSERT( pcThingName != NULL );
    configASSERT( pcPayloadBuffer != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    for( int i = 0; i < (int) ARRAY_SIZE( xIMUEvents ); i++ )
    {
        const IMUDescriptor_t * pxIMU = &xIMUEvents[i];

        /* Build discovery topic */
        (void) snprintf( configPUBLISH_TOPIC,
                         HA_CONFIG_MAX_TOPIC_LENGTH,
                         "homeassistant/%s/%s_%s/config",
                         entity,
                         pcThingName,
                         pxIMU->name );

        if( pxIMU->enabled == pdTRUE )
        {
            /* Build payload */
          size_t xPayloadLength = snprintf(
              pcPayloadBuffer,
              HA_CONFIG_PAYLOAD_BUFFER_LENGTH,
              "{"
                "\"automation_type\": \"trigger\","
                "\"topic\": \"%s/%s\","
                "\"type\": \"%s\","
                "\"subtype\": \"%s\","
                "\"payload\": \"%s\","
                "\"device\": {"
                  "\"identifiers\": [\"%s\"],"
                  "\"manufacturer\": \"STMicroelectronics\","
                  "\"model\": \"%s\","
                  "\"name\": \"%s\""
                "}"
              "}",
              pcThingName,
              MQTT_PUBLISH_TOPIC_EVENT,
              pxIMU->type,
              pxIMU->subtype,
              pxIMU->type,
              pcThingName,
              BOARD,
              pcThingName
          );


            if( xPayloadLength < HA_CONFIG_PAYLOAD_BUFFER_LENGTH )
            {
                (void) HA_PublishToTopic( MQTTQoS0,
                                          true,
                                          configPUBLISH_TOPIC,
                                          (const uint8_t *) pcPayloadBuffer,
                                          xPayloadLength );
            }
            else
            {
                LogError( ( "IMU payload truncated for %s", pxIMU->name ) );
            }
        }
        else
        {
            (void) HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        }

        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }
#else
    (void) pcThingName;
    (void) pcPayloadBuffer;
#endif
}

#include "ha_entities_cover.h"

#include <stdio.h>
#include <string.h>

#ifndef BOARD
    #define BOARD "UNKNOWN"
#endif

static char * entity = "cover";

typedef struct CoverDescriptor_t
{
    const char * name;
    BaseType_t enabled;
} CoverDescriptor_t;


#if (NUM_COVERS >= 1)
    #define GARAGE_DOOR_1_ENABLED pdTRUE
#else
    #define GARAGE_DOOR_1_ENABLED pdFALSE
#endif

#if (NUM_COVERS >= 2)
    #define GARAGE_DOOR_2_ENABLED pdTRUE
#else
    #define GARAGE_DOOR_2_ENABLED pdFALSE
#endif

#if (NUM_COVERS >= 3)
    #define GARAGE_DOOR_3_ENABLED pdTRUE
#else
    #define GARAGE_DOOR_3_ENABLED pdFALSE
#endif


/* Three garage doors */
static const CoverDescriptor_t xCovers[] = {
     { "GARAGE_DOOR_1", GARAGE_DOOR_1_ENABLED },
     { "GARAGE_DOOR_2", GARAGE_DOOR_2_ENABLED },
     { "GARAGE_DOOR_3", GARAGE_DOOR_3_ENABLED }
};

#define ARRAY_SIZE(arr) ( sizeof((arr)) / sizeof((arr)[0]) )

/*-----------------------------------------------------------*/

void HA_COVER_ClearConfig( const char * pcThingName )
{
    configASSERT( pcThingName != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    for( int i = 0; i < (int)ARRAY_SIZE( xCovers ); i++ )
    {
        snprintf( configPUBLISH_TOPIC,
                  HA_CONFIG_MAX_TOPIC_LENGTH,
                  "homeassistant/%s/%s_%s/config",
                  entity,
                  pcThingName,
                  xCovers[i].name );

        HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }
}

/*-----------------------------------------------------------*/

void HA_COVER_PublishConfig( const char * pcThingName,
                             char * pcPayloadBuffer )
{
#if (DEMO_COVER == 1)
    configASSERT( pcThingName != NULL );
    configASSERT( pcPayloadBuffer != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    char * commandTopic = pvPortMalloc( HA_CONFIG_MAX_TOPIC_LENGTH );
    char * stateTopic   = pvPortMalloc( HA_CONFIG_MAX_TOPIC_LENGTH );

    configASSERT( commandTopic != NULL );
    configASSERT( stateTopic != NULL );

    for( int i = 0; i < (int)ARRAY_SIZE( xCovers ); i++ )
    {
        const CoverDescriptor_t * pxCover = &xCovers[i];

        snprintf( configPUBLISH_TOPIC,
                  HA_CONFIG_MAX_TOPIC_LENGTH,
                  "homeassistant/%s/%s_%s/config",
                  entity,
                  pcThingName,
                  pxCover->name );

        if( pxCover->enabled == pdTRUE )
        {
            /* Per‑cover topics */
            snprintf( commandTopic,
                      HA_CONFIG_MAX_TOPIC_LENGTH,
                      "%s/cover/%s/desired",
                      pcThingName,
                      pxCover->name );

            snprintf( stateTopic,
                      HA_CONFIG_MAX_TOPIC_LENGTH,
                      "%s/cover/%s/state",
                      pcThingName,
                      pxCover->name );

            /* HA discovery JSON */
            size_t xPayloadLength = snprintf(
                pcPayloadBuffer,
                HA_CONFIG_PAYLOAD_BUFFER_LENGTH,
                "{"
                  "\"platform\": \"cover\","
                  "\"name\": \"%s\","
                  "\"unique_id\": \"%s_%s\","
                  "\"command_topic\": \"%s\","
                  "\"state_topic\": \"%s\","
                  "\"payload_open\": \"OPEN\","
                  "\"payload_close\": \"CLOSE\","
                  "\"payload_stop\": \"STOP\","
                  "\"state_open\": \"open\","
                  "\"state_opening\": \"opening\","
                  "\"state_closed\": \"closed\","
                  "\"state_closing\": \"closing\","
                  "\"availability_topic\": \"%s/status/availability\","
                  "\"payload_available\": \"online\","
                  "\"payload_not_available\": \"offline\","
                  "\"retain\": false,"
                  "\"device\": {"
                    "\"identifiers\": [\"%s\"],"
                    "\"manufacturer\": \"STMicroelectronics\","
                    "\"model\": \"%s\","
                    "\"name\": \"%s\""
                  "}"
                "}",
                pxCover->name,
                pcThingName,
                pxCover->name,
                commandTopic,
                stateTopic,
                pcThingName,
                pcThingName,
                BOARD,
                pcThingName );

            if( xPayloadLength < HA_CONFIG_PAYLOAD_BUFFER_LENGTH )
            {
                HA_PublishToTopic( MQTTQoS0,
                                   true,
                                   configPUBLISH_TOPIC,
                                   (const uint8_t *)pcPayloadBuffer,
                                   xPayloadLength );
            }
            else
            {
                LogError( ("Cover %s payload truncated", pxCover->name) );
            }
        }
        else
        {
            HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        }

        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }

    vPortFree( commandTopic );
    vPortFree( stateTopic );
#else
    (void)pcThingName;
    (void)pcPayloadBuffer;
#endif
}

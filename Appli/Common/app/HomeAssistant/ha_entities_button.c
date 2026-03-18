#include "ha_entities_button.h"

#include <stdio.h>
#include <string.h>

#ifndef BOARD
    #define BOARD "UNKNOWN"
#endif

static char * entity = "binary_sensor";

typedef struct BUTTONDescriptor_t
{
    const char * name;
    BaseType_t enabled;
} BUTTONDescriptor_t;

static const BUTTONDescriptor_t xBUTTONs[] = {
    { "USER_Button", pdTRUE }
};

#define ARRAY_SIZE(arr) ( sizeof( (arr) ) / sizeof( (arr)[0] ) )

/*-----------------------------------------------------------*/

void HA_Button_ClearConfig( const char * pcThingName )
{
    configASSERT( pcThingName != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    for( int i = 0; i < ( int ) ARRAY_SIZE( xBUTTONs ); i++ )
    {
        ( void ) snprintf( configPUBLISH_TOPIC,
                           HA_CONFIG_MAX_TOPIC_LENGTH,
                           "homeassistant/%s/%s_%s/config",
                           entity,
                           pcThingName,
                           xBUTTONs[ i ].name );

        ( void ) HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }
}

/*-----------------------------------------------------------*/

void HA_Button_PublishConfig( const char * pcThingName,
                             char * pcPayloadBuffer )
{
#if (DEMO_BUTTON == 1)
    configASSERT( pcThingName != NULL );
    configASSERT( pcPayloadBuffer != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    for( int i = 0; i < ( int ) ARRAY_SIZE( xBUTTONs ); i++ )
    {
        const BUTTONDescriptor_t * pxButton = &xBUTTONs[ i ];

        ( void ) snprintf( configPUBLISH_TOPIC,
                           HA_CONFIG_MAX_TOPIC_LENGTH,
                           "homeassistant/%s/%s_%s/config",
                           entity,
                           pcThingName,
                           pxButton->name );

        if( pxButton->enabled == pdTRUE )
        {
            size_t xPayloadLength = ( size_t ) snprintf(
                pcPayloadBuffer,
                HA_CONFIG_PAYLOAD_BUFFER_LENGTH,
                "{"
                  "\"name\": \"%s\","
                  "\"unique_id\": \"%s_%s\","
                  "\"state_topic\": \"%s/sensor/button/reported\"," /* Should match the < publish_topic > used in the button task */
                  "\"value_template\": \"{{ value_json.buttonStatus.%s.reported }}\","
                  "\"payload_on\": \"ON\","
                  "\"payload_off\": \"OFF\","
                  "\"device_class\": \"occupancy\","
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
                pxButton->name,
                pcThingName,
                pxButton->name,
                pcThingName,
                pxButton->name,
                pcThingName,
                pcThingName,
                BOARD,
                pcThingName );

            if( xPayloadLength < HA_CONFIG_PAYLOAD_BUFFER_LENGTH )
            {
                ( void ) HA_PublishToTopic( MQTTQoS0,
                                            true,
                                            configPUBLISH_TOPIC,
                                            ( const uint8_t * ) pcPayloadBuffer,
                                            xPayloadLength );
            }
            else
            {
                LogError( ( "Button %s payload truncated", pxButton->name ) );
            }
        }
        else
        {
            ( void ) HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        }

        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }
#else
    ( void ) pcThingName;
    ( void ) pcPayloadBuffer;
#endif
}

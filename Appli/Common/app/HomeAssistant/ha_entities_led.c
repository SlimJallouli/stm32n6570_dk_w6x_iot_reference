#include "ha_entities_led.h"

#include <stdio.h>
#include <string.h>

#ifndef BOARD
    #define BOARD "UNKNOWN"
#endif

static char * entity = "switch";

typedef struct LEDDescriptor_t
{
    const char * name;
    BaseType_t enabled;
} LEDDescriptor_t;

static const LEDDescriptor_t xLEDs[] = {
    { "LED_GREEN", pdTRUE },
    { "LED_RED",   pdFALSE }
};

#define ARRAY_SIZE(arr) ( sizeof( (arr) ) / sizeof( (arr)[0] ) )

/*-----------------------------------------------------------*/

void HA_LED_ClearConfig( const char * pcThingName )
{
    configASSERT( pcThingName != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    for( int i = 0; i < ( int ) ARRAY_SIZE( xLEDs ); i++ )
    {
        ( void ) snprintf( configPUBLISH_TOPIC,
                           HA_CONFIG_MAX_TOPIC_LENGTH,
                           "homeassistant/%s/%s_%s/config",
                           entity,
                           pcThingName,
                           xLEDs[ i ].name );

        ( void ) HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }
}

/*-----------------------------------------------------------*/
void HA_LED_PublishConfig( const char * pcThingName,
                           char * pcPayloadBuffer )
{
#if (DEMO_LED == 1)
    configASSERT( pcThingName != NULL );
    configASSERT( pcPayloadBuffer != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    char * commandTopic = ( char * ) pvPortMalloc( HA_CONFIG_MAX_TOPIC_LENGTH );
    char * stateTopic   = ( char * ) pvPortMalloc( HA_CONFIG_MAX_TOPIC_LENGTH );
    char payloadOn[ 32 ];
    char payloadOff[ 32 ];

    configASSERT( commandTopic != NULL );
    configASSERT( stateTopic != NULL );

    for( int i = 0; i < ( int ) ARRAY_SIZE( xLEDs ); i++ )
    {
        const LEDDescriptor_t * pxLed = &xLEDs[ i ];

        ( void ) snprintf( configPUBLISH_TOPIC,
                           HA_CONFIG_MAX_TOPIC_LENGTH,
                           "homeassistant/%s/%s_%s/config",
                           entity,
                           pcThingName,
                           pxLed->name );

        if( pxLed->enabled == pdTRUE )
        {
            ( void ) snprintf( commandTopic,
                               HA_CONFIG_MAX_TOPIC_LENGTH,
                               "%s/led/desired",
                               pcThingName ); /* Should match the < subscribe_topic > used in the LED task */

            ( void ) snprintf( stateTopic,
                               HA_CONFIG_MAX_TOPIC_LENGTH,
                               "%s/led/reported",
                               pcThingName );/* Should match the < publish_topic > used in the LED task */

            ( void ) snprintf( payloadOn, sizeof( payloadOn ), "%s_ON", pxLed->name );
            ( void ) snprintf( payloadOff, sizeof( payloadOff ), "%s_OFF", pxLed->name );

            size_t xPayloadLength = ( size_t ) snprintf(
                pcPayloadBuffer,
                HA_CONFIG_PAYLOAD_BUFFER_LENGTH,
                "{"
                  "\"name\": \"%s\","
                  "\"unique_id\": \"%s_%s\","
                  "\"command_topic\": \"%s\","
                  "\"state_topic\": \"%s\","
                  "\"value_template\": \"{{ value_json.ledStatus.%s.reported }}\","
                  "\"payload_on\": \"%s\","
                  "\"payload_off\": \"%s\","
                  "\"state_on\": \"ON\","
                  "\"state_off\": \"OFF\","
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
                pxLed->name,
                pcThingName,
                pxLed->name,
                commandTopic,
                stateTopic,
                pxLed->name,
                payloadOn,
                payloadOff,
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
                LogError( ( "LED %s payload truncated", pxLed->name ) );
            }
        }
        else
        {
            ( void ) HA_ClearRetainedTopic( configPUBLISH_TOPIC );
        }

        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    }

    vPortFree( commandTopic );
    vPortFree( stateTopic );
#else
    ( void ) pcThingName;
    ( void ) pcPayloadBuffer;
#endif
}


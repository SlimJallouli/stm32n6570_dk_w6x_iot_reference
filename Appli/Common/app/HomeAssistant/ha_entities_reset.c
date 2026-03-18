#include "ha_entities_reset.h"

#include <string.h>
#include <stdio.h>

#ifndef BOARD
    #define BOARD "UNKNOWN"
#endif

static char * entity = "button";

void HA_Reset_HandleRebootCommand( void * pxSubscriptionContext, MQTTPublishInfo_t * pPublishInfo )
{
    ( void ) pxSubscriptionContext;

    if( ( pPublishInfo == NULL ) || ( pPublishInfo->pPayload == NULL ) || ( pPublishInfo->payloadLength == 0 ) )
    {
        return;
    }

    const char * payload = ( const char * ) pPublishInfo->pPayload;

    /* Ensure payload is null-terminated for comparison. */
    char tempPayload[ 32 ] = { 0 };
    size_t copyLen = ( pPublishInfo->payloadLength < ( sizeof( tempPayload ) - 1 ) ) ?
                     pPublishInfo->payloadLength :
                     ( sizeof( tempPayload ) - 1 );

    ( void ) memcpy( tempPayload, payload, copyLen );
    tempPayload[ copyLen ] = '\0';

    /* Accept either raw string "reboot" or JSON-style {"action":"reboot"}. */
    if( ( strcmp( tempPayload, "reboot" ) == 0 ) || ( strstr( tempPayload, "\"reboot\"" ) != NULL ) )
    {
        LogInfo( "Reboot command received. Setting EVT_COMMAND_RESET flag." );
        xEventGroupSetBits( xHAEventGroup, EVT_COMMAND_RESET );
    }
    else
    {
        LogWarn( "Received unknown reboot command: %s", tempPayload );
    }
}

void HA_Reset_PublishConfig( const char * pcThingName, char * pcPayloadBuffer )
{
    configASSERT( pcThingName != NULL );
    configASSERT( pcPayloadBuffer != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    snprintf( configPUBLISH_TOPIC,
              HA_CONFIG_MAX_TOPIC_LENGTH,
              "homeassistant/%s/%s_reboot/config",
              entity,
              pcThingName );

    size_t xPayloadLength = snprintf(
        pcPayloadBuffer,
        HA_CONFIG_PAYLOAD_BUFFER_LENGTH,
        "{"
          "\"name\": \"Reboot\","
          "\"unique_id\": \"%s_reboot\","
          "\"command_topic\": \"%s/cmd/action\","
          "\"payload_press\": \"{\\\"action\\\":\\\"reboot\\\"}\","
          "\"retain\": false,"
          "\"availability_topic\": \"%s/status/availability\","
          "\"payload_available\": \"online\","
          "\"payload_not_available\": \"offline\","
          "\"device\": {"
            "\"identifiers\": [\"%s\"],"
            "\"manufacturer\": \"STMicroelectronics\","
            "\"model\": \"%s\","
            "\"name\": \"%s\","
            "\"serial_number\": \"%s\""
          "}"
        "}",
        pcThingName,   /* unique_id */
        pcThingName,   /* command_topic */
        pcThingName,   /* availability_topic */
        pcThingName,   /* identifiers */
        BOARD,         /* model */
        pcThingName,   /* device name */
        pcThingName    /* serial number */
    );

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
        LogError( ( "Reboot button payload truncated" ) );
    }

    vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
}


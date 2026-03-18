#include "ha_entities_ota.h"

#include "queue.h"   /* For QueueHandle_t / xQueueReceive */

#include <string.h>
#include <stdio.h>

/* BOARD is used in discovery payloads. Provided by the platform build. */
#ifndef BOARD
    #define BOARD "UNKNOWN"
#endif

static char * entity = "update";

#if (DEMO_AWS_OTA == 1)
volatile AppVersion32_t newAppFirmwareVersion;
#endif

/* Queue created in otaPal_EarlyInit (ota_pal.c) to report OTA blocks remaining. */
extern QueueHandle_t xOtaBlocksRemainingQueue;

/*-----------------------------------------------------------*/

void HA_OTA_ClearConfig( const char * pcThingName )
{
    configASSERT( pcThingName != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    /* homeassistant/update/<thing>_fw/config */
    ( void ) snprintf( configPUBLISH_TOPIC,
                       HA_CONFIG_MAX_TOPIC_LENGTH,
                       "homeassistant/%s/%s_fw/config",
                       entity,
                       pcThingName );

    ( void ) HA_ClearRetainedTopic( configPUBLISH_TOPIC );
    vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
}

/*-----------------------------------------------------------*/

void HA_OTA_PublishConfig( const char * pcThingName,
                           char * pcPayloadBuffer )
{
#if (DEMO_AWS_OTA == 1)
    configASSERT( pcThingName != NULL );
    configASSERT( pcPayloadBuffer != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    /* Compose firmware version string (major.minor.build). */
    char pcFwVersionString[ 17 ] = { 0 };

    snprintf( pcFwVersionString,
              sizeof( pcFwVersionString ),
              "%u.%u.%u",
              (unsigned) appFirmwareVersion.u.x.major,
              (unsigned) appFirmwareVersion.u.x.minor,
              (unsigned) appFirmwareVersion.u.x.build );

    snprintf( configPUBLISH_TOPIC,
              HA_CONFIG_MAX_TOPIC_LENGTH,
              "homeassistant/%s/%s_fw/config",
              entity,
              pcThingName );

    size_t xPayloadLength = snprintf(
        pcPayloadBuffer,
        HA_CONFIG_PAYLOAD_BUFFER_LENGTH,
        "{"
          "\"name\": \"Firmware\","
          "\"unique_id\": \"%s_fw_update\","
          "\"state_topic\": \"%s/fw/state\","
          "\"value_template\": \"{{ value_json.installed_version }}\","
          "\"latest_version_topic\": \"%s/fw/state\","
          "\"latest_version_template\": \"{{ value_json.latest_version }}\","
          "\"json_attributes_topic\": \"%s/fw/state\","
          "\"json_attributes_template\": \"{{ value_json | tojson }}\","
          "\"command_topic\": \"%s/fw/update\","
          "\"payload_install\": \"start_update\","
          "\"availability_topic\": \"%s/status/availability\","
          "\"payload_available\": \"online\","
          "\"payload_not_available\": \"offline\","
          "\"retain\": false,"
          "\"device_class\": \"firmware\","
          "\"device\": {"
            "\"identifiers\": [\"%s\"],"
            "\"manufacturer\": \"STMicroelectronics\","
            "\"model\": \"%s\","
            "\"name\": \"%s\","
            "\"sw_version\": \"%s\""
          "}"
        "}",
        pcThingName,     /* unique_id */
        pcThingName,     /* state_topic */
        pcThingName,     /* latest_version_topic */
        pcThingName,     /* json_attributes_topic */
        pcThingName,     /* command_topic */
        pcThingName,     /* availability_topic */
        pcThingName,     /* identifiers */
        BOARD,           /* model */
        pcThingName,     /* device name */
        pcFwVersionString );

    if( xPayloadLength < HA_CONFIG_PAYLOAD_BUFFER_LENGTH )
    {
        HA_PublishToTopic( MQTTQoS0,
                           true,
                           configPUBLISH_TOPIC,
                           (const uint8_t *) pcPayloadBuffer,
                           xPayloadLength );
    }
    else
    {
        LogError( ("Firmware update payload truncated") );
    }

    vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
#else
    (void) pcThingName;
    (void) pcPayloadBuffer;
#endif
}


/*-----------------------------------------------------------*/

#if (DEMO_AWS_OTA == 1)

static const char * prvFwUpdateStatusToString( FwUpdateStatus_t xStatus )
{
    switch( xStatus )
    {
        case FW_UPDATE_STATUS_IDLE:      return "idle";
        case FW_UPDATE_STATUS_UPDATING:  return "installing";
        case FW_UPDATE_STATUS_COMPLETED: return "completed";
        default:                         return "unknown";
    }
}

MQTTStatus_t HA_OTA_PublishFirmwareVersionStatus( AppVersion32_t xInstalled,
                                                  AppVersion32_t xLatest,
                                                  const char * pcThingName,
                                                  FwUpdateStatus_t xStatus )
{
    char pcPayloadBuffer[ 128 ];
    char cTopicBuf[ 64 ];

    /* Compose topic: <ThingName>/fw/state */
    int msgLen = snprintf( cTopicBuf, sizeof( cTopicBuf ), "%s/fw/state", pcThingName );

    if( ( msgLen < 0 ) || ( ( size_t ) msgLen >= sizeof( cTopicBuf ) ) )
    {
        return MQTTBadParameter;
    }

    const char * statusStr = prvFwUpdateStatusToString( xStatus );

    msgLen = snprintf( pcPayloadBuffer,
                       sizeof( pcPayloadBuffer ),
                       "{\"installed_version\": \"%u.%u.%u\", \"latest_version\": \"%u.%u.%u\", \"status\": \"%s\"}",
                       ( unsigned ) xInstalled.u.x.major,
                       ( unsigned ) xInstalled.u.x.minor,
                       ( unsigned ) xInstalled.u.x.build,
                       ( unsigned ) xLatest.u.x.major,
                       ( unsigned ) xLatest.u.x.minor,
                       ( unsigned ) xLatest.u.x.build,
                       statusStr );

    if( ( msgLen < 0 ) || ( ( size_t ) msgLen >= sizeof( pcPayloadBuffer ) ) )
    {
        return MQTTBadParameter;
    }

    MQTTStatus_t xMqtt = HA_PublishToTopic( MQTTQoS1,
                                           true,
                                           cTopicBuf,
                                           ( const uint8_t * ) pcPayloadBuffer,
                                           ( size_t ) msgLen );

    vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );

    return xMqtt;
}

/* New: publish OTA progress + blocks_remaining as attributes on the same state topic. */
MQTTStatus_t HA_OTA_PublishFirmwareProgress( AppVersion32_t xInstalled,
                                             AppVersion32_t xLatest,
                                             const char * pcThingName,
                                             uint32_t ulProgress,
                                             uint32_t ulBlocksRemaining )
{
    char pcPayloadBuffer[ 192 ];
    char cTopicBuf[ 64 ];

    /* Compose topic: <ThingName>/fw/state */
    int msgLen = snprintf( cTopicBuf, sizeof( cTopicBuf ), "%s/fw/state", pcThingName );

    if( ( msgLen < 0 ) || ( ( size_t ) msgLen >= sizeof( cTopicBuf ) ) )
    {
        return MQTTBadParameter;
    }

    /* Status is always "updating" when we send progress. */
    msgLen = snprintf( pcPayloadBuffer,
                       sizeof( pcPayloadBuffer ),
                       "{"
                         "\"installed_version\": \"%u.%u.%u\","
                         "\"latest_version\": \"%u.%u.%u\","
                         "\"status\": \"installing\","
                         "\"in_progress\": true,"
                         "\"update_percentage\": %u"
                       "}",
                       xInstalled.u.x.major,
                       xInstalled.u.x.minor,
                       xInstalled.u.x.build,
                       xLatest.u.x.major,
                       xLatest.u.x.minor,
                       xLatest.u.x.build,
                       (int)ulProgress);


    if( ( msgLen < 0 ) || ( ( size_t ) msgLen >= sizeof( pcPayloadBuffer ) ) )
    {
        return MQTTBadParameter;
    }

    MQTTStatus_t xMqtt = HA_PublishToTopic( MQTTQoS1,
                                           true,
                                           cTopicBuf,
                                           ( const uint8_t * ) pcPayloadBuffer,
                                           ( size_t ) msgLen );

    vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );

    return xMqtt;
}

void HA_OTA_HandleFwUpdateCommand( void * pxSubscriptionContext,
                                  MQTTPublishInfo_t * pPublishInfo )
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

    if( strcmp( tempPayload, "start_update" ) == 0 )
    {
        LogInfo( "Starting Firmware update." );
        xEventGroupSetBits( xHAEventGroup, EVT_OTA_UPDATE_START );
    }
}

/*-----------------------------------------------------------*/
/* OTA progress reporting task (separate from HA main task). */
void vOtaProgressTask( void * pvParameters )
{
    const char * pcThingName = ( const char * ) pvParameters;
    uint32_t ulTotalBlocks   = 0;
    uint32_t ulLastProgress  = 0xFFFFFFFFu;

    LogInfo( "OTA Progress Task started" );

    for( ;; )
    {
        EventBits_t uxBits = xEventGroupGetBits( xHAEventGroup );

        if( ( uxBits & EVT_OTA_COMPLETED ) != 0U ||
            ( uxBits & EVT_OTA_UPDATE_ABORT ) != 0U )
        {
            LogInfo( "OTA Progress Task exiting" );
            vTaskDelete( NULL );
        }

        if( xOtaBlocksRemainingQueue != NULL )
        {
            uint32_t ulBlocksRemaining = 0;

            if( xQueueReceive( xOtaBlocksRemainingQueue,
                               &ulBlocksRemaining,
                               0 ) == pdTRUE )
            {
                if( ( ulTotalBlocks == 0U ) && ( ulBlocksRemaining > 0U ) )
                {
                    ulTotalBlocks = ulBlocksRemaining;
                }

                if( ( ulTotalBlocks > 0U ) && ( ulBlocksRemaining <= ulTotalBlocks ) )
                {
                    uint32_t ulProgress =
                        100U - ( ( ulBlocksRemaining * 100U ) / ulTotalBlocks );

                    if( ulProgress != ulLastProgress )
                    {
                        ulLastProgress = ulProgress;

                        LogInfo( "OTA progress: %lu%% (%lu blocks remaining)",
                                 ( unsigned long ) ulProgress,
                                 ( unsigned long ) ulBlocksRemaining );

                        ( void ) HA_OTA_PublishFirmwareProgress( appFirmwareVersion,
                                                                 newAppFirmwareVersion,
                                                                 pcThingName,
                                                                 ulProgress,
                                                                 ulBlocksRemaining );
                    }
                }
            }
        }

        vTaskDelay( pdMS_TO_TICKS( 200 ) );
    }
}
#endif /* DEMO_AWS_OTA */

#include "ha_helpers.h"

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* MQTT agent task API. */
#include "mqtt_agent_task.h"

/*-----------------------------------------------------------*/

/* Reasonable bounds to avoid a permanent hang if publish cannot complete. */
#ifndef HA_MQTT_PUBLISH_MAX_RETRIES
    #define HA_MQTT_PUBLISH_MAX_RETRIES             ( 5U )
#endif

#ifndef HA_MQTT_PUBLISH_NOTIFY_TIMEOUT_MS
    /* Wait up to this long for a PUBACK/complete callback once the command was queued. */
    #define HA_MQTT_PUBLISH_NOTIFY_TIMEOUT_MS       ( 5000U )
#endif

#ifndef HA_MQTT_PUBLISH_RETRY_BACKOFF_MS
    #define HA_MQTT_PUBLISH_RETRY_BACKOFF_MS        ( 250U )
#endif

/*-----------------------------------------------------------*/

struct MQTTAgentCommandContext
{
    TaskHandle_t xTaskToNotify;
    void * pArgs;
};

typedef struct MQTTAgentCommandContext MQTTAgentCommandContext_t;

/* MQTT agent handle shared by HA helpers. */
static MQTTAgentHandle_t xMQTTAgentHandle = NULL;

/*-----------------------------------------------------------*/

static void prvPublishCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                      MQTTAgentReturnInfo_t * pxReturnInfo )
{
    if( ( pxCommandContext != NULL ) && ( pxCommandContext->xTaskToNotify != NULL ) && ( pxReturnInfo != NULL ) )
    {
        ( void ) xTaskNotify( pxCommandContext->xTaskToNotify,
                              pxReturnInfo->returnCode,
                              eSetValueWithOverwrite );
    }
}

/*-----------------------------------------------------------*/

void HAHelpers_SetMqttAgentHandle( MQTTAgentHandle_t xHandle )
{
    xMQTTAgentHandle = xHandle;
}

/*-----------------------------------------------------------*/

TickType_t HA_GetJitteredTimeoutTicks( void )
{
    /* uxRand() returns a 32-bit unsigned random number.
     * Jitter in [-HA_JITTER_RANGE_MS, +HA_JITTER_RANGE_MS]. */
    int32_t iJitter = ( int32_t ) ( uxRand() % ( 2UL * HA_JITTER_RANGE_MS ) ) - ( int32_t ) HA_JITTER_RANGE_MS;

    /* IMPORTANT: perform the addition in signed space to avoid unsigned wrap
     * when iJitter is negative (usual arithmetic conversions). */
    int32_t lFinalTimeoutMs = ( int32_t ) HA_BASE_TIMEOUT_MS + iJitter;

    if( lFinalTimeoutMs < 1 )
    {
        lFinalTimeoutMs = 1;
    }

    return pdMS_TO_TICKS( ( uint32_t ) lFinalTimeoutMs );
}

/*-----------------------------------------------------------*/

MQTTStatus_t HA_PublishToTopic( MQTTQoS_t xQoS,
                               bool xRetain,
                               const char * pcTopic,
                               const uint8_t * pucPayload,
                               size_t xPayloadLength )
{
    configASSERT( pcTopic != NULL );
    configASSERT( xMQTTAgentHandle != NULL );

    MQTTPublishInfo_t xPublishInfo = { 0U };
    MQTTAgentCommandContext_t xCommandContext = { 0 };
    MQTTAgentCommandInfo_t xCommandParams = { 0U };
    MQTTStatus_t xMQTTStatus = MQTTBadParameter;

    /* Create a unique number of the publish that is about to be sent. */
    xTaskNotifyStateClear( NULL );

    xPublishInfo.qos = xQoS;
    xPublishInfo.retain = xRetain;
    xPublishInfo.pTopicName = pcTopic;
    xPublishInfo.topicNameLength = ( uint16_t ) strlen( pcTopic );
    xPublishInfo.pPayload = pucPayload;
    xPublishInfo.payloadLength = xPayloadLength;

    xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

    xCommandParams.blockTimeMs = HA_CONFIG_MAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvPublishCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = &xCommandContext;

    for( uint32_t ulTry = 0; ulTry < HA_MQTT_PUBLISH_MAX_RETRIES; ulTry++ )
    {
        xMQTTStatus = MQTTAgent_Publish( xMQTTAgentHandle, &xPublishInfo, &xCommandParams );

        if( xMQTTStatus == MQTTSuccess )
        {
            uint32_t ulNotifiedValue = 0U;
            BaseType_t xNotifyStatus = xTaskNotifyWait( 0,
                                                        0,
                                                        &ulNotifiedValue,
                                                        pdMS_TO_TICKS( HA_MQTT_PUBLISH_NOTIFY_TIMEOUT_MS ) );

            if( xNotifyStatus == pdTRUE )
            {
                /* Non-zero return code indicates send failure. */
                xMQTTStatus = ( ulNotifiedValue == 0U ) ? MQTTSuccess : MQTTSendFailed;
            }
            else
            {
                xMQTTStatus = MQTTSendFailed;
            }
        }

        if( xMQTTStatus == MQTTSuccess )
        {
            break;
        }

        vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_RETRY_BACKOFF_MS ) );
    }

    return xMQTTStatus;
}

/*-----------------------------------------------------------*/

MQTTStatus_t HA_ClearRetainedTopic( const char * pcTopic )
{
    configASSERT( pcTopic != NULL );

    LogDebug( ( "Clearing retained message on topic: %s", pcTopic ) );

    /* Publish a zero-length retained payload. */
    return HA_PublishToTopic( MQTTQoS0, true, pcTopic, NULL, 0 );
}

/*-----------------------------------------------------------*/

BaseType_t HA_SubscribeToThingTopic( const char * pcThingName,
                                     const char * pcTopicSuffix,
                                     IncomingPubCallback_t xCallback,
                                     const char * pcLogContext )
{
    BaseType_t xResult = pdPASS;

    configASSERT( pcThingName != NULL );
    configASSERT( pcTopicSuffix != NULL );
    configASSERT( pcLogContext != NULL );
    configASSERT( xMQTTAgentHandle != NULL );

    char * pcTopicBuffer = pvPortMalloc( HA_CONFIG_MAX_TOPIC_LENGTH );
    configASSERT( pcTopicBuffer != NULL );

    int iLen = snprintf( pcTopicBuffer,
                         HA_CONFIG_MAX_TOPIC_LENGTH,
                         "%s/%s",
                         pcThingName,
                         pcTopicSuffix );

    if( ( iLen < 0 ) || ( iLen >= ( int ) HA_CONFIG_MAX_TOPIC_LENGTH ) )
    {
        LogError( ( "Topic buffer too small for %s topic (%s/%s).",
                    pcLogContext,
                    pcThingName,
                    pcTopicSuffix ) );
        xResult = pdFAIL;
    }

    if( xResult == pdPASS )
    {
        MQTTStatus_t xMQTTStatus = MqttAgent_SubscribeSync( xMQTTAgentHandle,
                                                           pcTopicBuffer,
                                                           MQTTQoS0,
                                                           xCallback,
                                                           NULL );

        if( xMQTTStatus != MQTTSuccess )
        {
            LogError( "Failed to subscribe to %s topic: %s",
                      pcLogContext,
                      pcTopicBuffer );
            xResult = pdFAIL;
        }
    }

    vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
    vPortFree( pcTopicBuffer );

    return xResult;
}

/*-----------------------------------------------------------*/

BaseType_t HA_SubscribeToTopic( const char * pcTopic,
                                IncomingPubCallback_t xCallback )
{
    BaseType_t xResult = pdPASS;

    configASSERT( pcTopic != NULL );
    configASSERT( xCallback != NULL );
    configASSERT( xMQTTAgentHandle != NULL );

    MQTTStatus_t xMQTTStatus = MqttAgent_SubscribeSync( xMQTTAgentHandle,
                                                       pcTopic,
                                                       MQTTQoS0,
                                                       xCallback,
                                                       NULL );

    if( xMQTTStatus != MQTTSuccess )
    {
        LogError( "Failed to subscribe to topic: %s",
                  pcTopic );
        xResult = pdFAIL;
    }

    vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );

    return xResult;
}

/*-----------------------------------------------------------*/

void HA_PublishAvailabilityStatus( const char * pcThingName,
                                   char * pcPayloadBuffer,
                                   const char * pcAvailability )
{
    configASSERT( pcThingName != NULL );
    configASSERT( pcPayloadBuffer != NULL );
    configASSERT( pcAvailability != NULL );
    configASSERT( configPUBLISH_TOPIC != NULL );

    snprintf( configPUBLISH_TOPIC,
              HA_CONFIG_MAX_TOPIC_LENGTH,
              "%s/status/availability",
              pcThingName );

    /* Copy the provided availability string ("online" or "offline") into the payload buffer. */
    strncpy( pcPayloadBuffer, pcAvailability, HA_CONFIG_PAYLOAD_BUFFER_LENGTH - 1 );
    pcPayloadBuffer[ HA_CONFIG_PAYLOAD_BUFFER_LENGTH - 1 ] = '\0';

    size_t xPayloadLength = strlen( pcPayloadBuffer );

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
        LogError( ( "availability update payload truncated" ) );
    }

    vTaskDelay( pdMS_TO_TICKS( HA_MQTT_PUBLISH_TIME_BETWEEN_MS ) );
}

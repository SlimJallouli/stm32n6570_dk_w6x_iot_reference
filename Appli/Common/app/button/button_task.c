/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

/* MQTT library includes. */
#include "core_mqtt.h"

/* MQTT agent include. */
#include "core_mqtt_agent.h"

/* MQTT agent task API. */
#include "mqtt_agent_task.h"

#include "kvstore.h"

#include "interrupt_handlers.h"

#define MAXT_TOPIC_LENGTH 64
static char publish_topic[ MAXT_TOPIC_LENGTH ];

/* One event bit per button; for now only USER_Button. */
#define BUTTON_USER_EVENT      ( 1 << 0 )

static EventGroupHandle_t xButtonEventGroup = NULL;

/**
 * @brief The maximum amount of time in milliseconds to wait for the commands
 * to be posted to the MQTT agent should the MQTT agent's command queue be full.
 * Tasks wait in the Blocked state, so don't use any CPU time.
 */
#define configMAX_COMMAND_SEND_BLOCK_TIME_MS         ( 500 )

/**
 * @brief Size of statically allocated buffers for holding payloads.
 */
#define configPAYLOAD_BUFFER_LENGTH                  ( 512 )

/**
 * @brief Format of topic used to publish outgoing messages.
 */
#define configPUBLISH_TOPIC                   publish_topic

/**
 * @brief Format of topic used to subscribe to incoming messages.
 *
 * (Not used in button task; kept for consistency with LED task.)
 */
#define configSUBSCRIBE_TOPIC_FORMAT   configPUBLISH_TOPIC_FORMAT

/*-----------------------------------------------------------*/

/* Button descriptor (future-proof for multiple buttons). */
typedef struct BUTTONDescriptor_t
{
    const char    *name;      /* logical name in JSON: "USER_Button", "BUTTON2", ... */
    EventBits_t    eventBit;  /* event bit for this button in xButtonEventGroup    */
    GPIO_TypeDef  *port;
    uint16_t       pin;
    GPIO_PinState  onLevel;   /* GPIO level that means "ON"/pressed                */
    GPIO_PinState  pinState;  /* last sampled GPIO state                           */
} BUTTONDescriptor_t;


/* For now only one button, user button. */
static BUTTONDescriptor_t xBUTTONs[] =
{
    { "USER_Button",
      BUTTON_USER_EVENT,
      USER_Button_GPIO_Port,
      USER_Button_Pin,
      USER_BUTTON_ON,
      USER_BUTTON_ON }
};

#define BUTTON_COUNT   ( ( uint8_t ) ( sizeof( xBUTTONs ) / sizeof( xBUTTONs[ 0 ] ) ) )

/*-----------------------------------------------------------*/

/**
 * @brief Defines the structure to use as the command callback context in this
 * demo.
 */
struct MQTTAgentCommandContext
{
    TaskHandle_t xTaskToNotify;
    void        *pArgs;
};

/*-----------------------------------------------------------*/

static MQTTAgentHandle_t xMQTTAgentHandle = NULL;

/*-----------------------------------------------------------*/

/**
 * @brief Passed into MQTTAgent_Publish() as the callback to execute when the
 * broker ACKs the PUBLISH message.
 */
static void prvPublishCommandCallback( MQTTAgentCommandContext_t *pxCommandContext,
                                       MQTTAgentReturnInfo_t *pxReturnInfo );

/**
 * @brief Publishes the given payload using the given qos to the topic provided.
 */
static MQTTStatus_t prvPublishToTopic( MQTTQoS_t xQoS,
                                       bool xRetain,
                                       char *pcTopic,
                                       uint8_t *pucPayload,
                                       size_t xPayloadLength );

/**
 * @brief Callback function for GPIO rising edge interrupt on the user button.
 */
static void user_button_rising_event( void *pvContext );

/**
 * @brief Callback function for GPIO falling edge interrupt on the user button.
 */
static void user_button_falling_event( void *pvContext );

/**
 * @brief The function that implements the button reporting task.
 */
void vButtonTask( void *pvParameters );

/*-----------------------------------------------------------*/

/**
 * @brief The MQTT agent manages the MQTT contexts.  This set the handle to the
 * context used by this demo.
 */
extern MQTTAgentContext_t xGlobalMqttAgentContext;

/*-----------------------------------------------------------*/

static void prvPublishCommandCallback( MQTTAgentCommandContext_t *pxCommandContext,
                                       MQTTAgentReturnInfo_t *pxReturnInfo )
{
    if( pxCommandContext->xTaskToNotify != NULL )
    {
        xTaskNotify( pxCommandContext->xTaskToNotify,
                     pxReturnInfo->returnCode,
                     eSetValueWithOverwrite );
    }
}

/*-----------------------------------------------------------*/

static MQTTStatus_t prvPublishToTopic( MQTTQoS_t xQoS,
                                       bool xRetain,
                                       char *pcTopic,
                                       uint8_t *pucPayload,
                                       size_t xPayloadLength )
{
    MQTTPublishInfo_t        xPublishInfo   = { 0UL };
    MQTTAgentCommandContext_t xCommandContext = { 0 };
    MQTTStatus_t             xMQTTStatus;
    BaseType_t               xNotifyStatus;
    MQTTAgentCommandInfo_t   xCommandParams = { 0UL };
    uint32_t                 ulNotifiedValue = 0U;

    xTaskNotifyStateClear( NULL );

    xPublishInfo.qos              = xQoS;
    xPublishInfo.retain           = xRetain;
    xPublishInfo.pTopicName       = pcTopic;
    xPublishInfo.topicNameLength  = ( uint16_t ) strlen( pcTopic );
    xPublishInfo.pPayload         = pucPayload;
    xPublishInfo.payloadLength    = xPayloadLength;

    xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

    xCommandParams.blockTimeMs                 = configMAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback        = prvPublishCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = &xCommandContext;

    do
    {
        xMQTTStatus = MQTTAgent_Publish( xMQTTAgentHandle,
                                         &xPublishInfo,
                                         &xCommandParams );

        if( xMQTTStatus == MQTTSuccess )
        {
            xNotifyStatus = xTaskNotifyWait( 0,
                                             0,
                                             &ulNotifiedValue,
                                             portMAX_DELAY );

            if( xNotifyStatus == pdTRUE )
            {
                if( ulNotifiedValue )
                {
                    xMQTTStatus = MQTTSendFailed;
                }
                else
                {
                    xMQTTStatus = MQTTSuccess;
                }
            }
            else
            {
                xMQTTStatus = MQTTSendFailed;
            }
        }
    }
    while( xMQTTStatus != MQTTSuccess );

    return xMQTTStatus;
}

/*-----------------------------------------------------------*/

static void user_button_rising_event( void *pvContext )
{
    ( void ) pvContext;

    if( xButtonEventGroup != NULL )
    {
        xEventGroupSetBitsFromISR( xButtonEventGroup, BUTTON_USER_EVENT, NULL );
    }
}

/*-----------------------------------------------------------*/

static void user_button_falling_event( void *pvContext )
{
    ( void ) pvContext;

    if( xButtonEventGroup != NULL )
    {
        xEventGroupSetBitsFromISR( xButtonEventGroup, BUTTON_USER_EVENT, NULL );
    }
}

/*-----------------------------------------------------------*/

void vButtonTask( void *pvParameters )
{
    char       *cPayloadBuf;
    size_t      xPayloadLength;
    MQTTStatus_t xMQTTStatus;
    MQTTQoS_t   xQoS      = MQTTQoS0;
    bool        xRetain   = pdTRUE;
    char       *pThingName = NULL;
    size_t      uxTempSize = 0;
    EventBits_t uxBits;

    ( void ) pvParameters;

    /* Wait until the MQTT agent is ready */
    vSleepUntilMQTTAgentReady();

    /* Get the MQTT Agent handle */
    xMQTTAgentHandle = xGetMqttAgentHandle();
    configASSERT( xMQTTAgentHandle != NULL );

    /* Wait until we are connected to AWS */
    vSleepUntilMQTTAgentConnected();

    LogInfo( ( "MQTT Agent connected. Starting button reporting task." ) );

    pThingName = KVStore_getStringHeap( CS_CORE_THING_NAME, &uxTempSize );
    configASSERT( pThingName != NULL );

    cPayloadBuf = ( char * ) pvPortMalloc( configPAYLOAD_BUFFER_LENGTH );
    configASSERT( cPayloadBuf != NULL );

    xButtonEventGroup = xEventGroupCreate();
    configASSERT( xButtonEventGroup != NULL );

    /* Single shared reported topic for all buttons */
    snprintf( configPUBLISH_TOPIC,
              MAXT_TOPIC_LENGTH,
              "%s/sensor/button/reported",
              pThingName );

    /* Register GPIO callbacks for all buttons (currently only one). */
    GPIO_EXTI_Register_Rising_Callback( USER_Button_Pin,
                                        user_button_rising_event,
                                        NULL );
    GPIO_EXTI_Register_Falling_Callback( USER_Button_Pin,
                                         user_button_falling_event,
                                         NULL );

    /* Force initial state update for all enabled buttons */
    for( uint8_t i = 0; i < BUTTON_COUNT; i++ )
    {
       xEventGroupSetBits( xButtonEventGroup, xBUTTONs[ i ].eventBit );
    }

    for( ;; )
    {
        /* Wait for any button event bit to be set. */
        EventBits_t allBitsMask = 0;
        for( uint8_t i = 0; i < BUTTON_COUNT; i++ )
        {
            allBitsMask |= xBUTTONs[ i ].eventBit;
        }

        uxBits = xEventGroupWaitBits( xButtonEventGroup,
                                      allBitsMask,
                                      pdTRUE,
                                      pdFALSE,
                                      portMAX_DELAY );

        if( uxBits != 0 )
        {
            /* Sample all buttons and build JSON. */
            int len = snprintf( cPayloadBuf,
                                configPAYLOAD_BUFFER_LENGTH,
                                "{ \"buttonStatus\": { " );
            configASSERT( len >= 0 );

            for( uint8_t i = 0; i < BUTTON_COUNT; i++ )
            {
                BUTTONDescriptor_t *pxBtn = &xBUTTONs[ i ];

                /* Sample GPIO and update pinState. */
                GPIO_PinState pinState = HAL_GPIO_ReadPin( pxBtn->port, pxBtn->pin );
                pxBtn->pinState        = pinState;

                /* Determine logical status. */
                const char *status = ( pinState == pxBtn->onLevel ) ? "ON" : "OFF";

                /* Append JSON fragment: "<name>": { "reported": "<status>" } */
                int partLen = snprintf( cPayloadBuf + len,
                                        configPAYLOAD_BUFFER_LENGTH - ( size_t ) len,
                                        "%s\"%s\": { \"reported\": \"%s\" }",
                                        ( len > ( int ) strlen( "{ \"buttonStatus\": { " ) ) ? ", " : "",
                                        pxBtn->name,
                                        status );
                configASSERT( partLen >= 0 );
                len += partLen;
            }

            /* Close JSON. */
            int closeLen = snprintf( cPayloadBuf + len,
                                     configPAYLOAD_BUFFER_LENGTH - ( size_t ) len,
                                     " } }" );
            configASSERT( closeLen >= 0 );
            len += closeLen;

            xPayloadLength = ( size_t ) len;
            configASSERT( xPayloadLength < configPAYLOAD_BUFFER_LENGTH );

            LogInfo( ( "Publishing button status to: %s, message: %.*s",
                       configPUBLISH_TOPIC,
                       ( int ) xPayloadLength,
                       cPayloadBuf ) );

            xMQTTStatus = prvPublishToTopic( xQoS,
                                             xRetain,
                                             configPUBLISH_TOPIC,
                                             ( uint8_t * ) cPayloadBuf,
                                             xPayloadLength );

            if( xMQTTStatus == MQTTSuccess )
            {
                LogInfo( ( "Successfully published button status JSON" ) );
            }
            else
            {
                LogError( ( "Failed to publish button status to topic: %s",
                            configPUBLISH_TOPIC ) );
            }
        }
    }

    /* Not expected to be reached */
    vPortFree( pThingName );
    vPortFree( cPayloadBuf );
    vEventGroupDelete( xButtonEventGroup );
    vTaskDelete( NULL );
}

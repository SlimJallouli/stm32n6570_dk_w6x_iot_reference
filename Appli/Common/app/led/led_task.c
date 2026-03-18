/*
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 */

/**
 * @brief A test application which loops through subscribing to a topic and publishing message
 * to a topic. This test application can be used with AWS IoT device advisor test suite to
 * verify that an application implemented using MQTT agent follows best practices in connecting
 * to AWS IoT core.
 */
/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

/* MQTT library includes. */
#include "core_mqtt.h"

/* MQTT agent include. */
#include "core_mqtt_agent.h"

/* MQTT agent task API. */
#include "mqtt_agent_task.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

#include "kvstore.h"

/* JSON library includes. */
#include "core_json.h"

#define MAXT_TOPIC_LENGTH 64
static char subscribe_topic[MAXT_TOPIC_LENGTH];
static char publish_topic[MAXT_TOPIC_LENGTH];

/* One reported state per LED (RED, GREEN, BLUE) */
static BaseType_t led_reported_status[2] = { pdFALSE, pdFALSE };
static BaseType_t led_last_status    [2] = { pdTRUE,  pdTRUE  };

static EventGroupHandle_t xLedEventGroup;
#define LED_STATUS_CHANGED_EVENT    ( 1 << 0 )

typedef struct
{
    const char    *name;       /* "LED_RED", "LED_GREEN", "LED_BLUE" */
    GPIO_TypeDef  *port;
    uint16_t       pin;
    GPIO_PinState  onLevel;
    GPIO_PinState  offLevel;
    uint8_t        index;      /* index in led_reported_status[] */
} LedHwDescriptor_t;

#define LED_RED_INDEX   0
#define LED_GREEN_INDEX 1

static const LedHwDescriptor_t xLEDs[] =
{
    { "LED_RED"  , LED_RED_GPIO_Port  , LED_RED_Pin  , LED_RED_ON,   LED_RED_OFF  , LED_RED_INDEX   },
    { "LED_GREEN", LED_GREEN_GPIO_Port, LED_GREEN_Pin, LED_GREEN_ON, LED_GREEN_OFF, LED_GREEN_INDEX }
};

#define LED_COUNT   ( ( uint8_t ) ( sizeof( xLEDs ) / sizeof( xLEDs[ 0 ] ) ) )

/**
 * @brief A test application which loops through subscribing to a topic and publishing message
 * to a topic. This test application can be used with AWS IoT device advisor test suite to
 * verify that an application implemented using MQTT agent follows best practices in connecting
 * to AWS IoT core.
 */
#define configMS_TO_WAIT_FOR_NOTIFICATION            ( 10000 )

/**
 * @brief Delay for the synchronous publisher task between publishes.
 */
#define configDELAY_BETWEEN_PUBLISH_OPERATIONS_MS    ( 2000U )

/**
 * @brief The maximum amount of time in milliseconds to wait for the commands
 * to be posted to the MQTT agent should the MQTT agent's command queue be full.
 * Tasks wait in the Blocked state, so don't use any CPU time.
 */
#define configMAX_COMMAND_SEND_BLOCK_TIME_MS         ( 500 )

/**
 * @brief Size of statically allocated buffers for holding payloads.
 */
#define configPAYLOAD_BUFFER_LENGTH                  ( 128 )

/**
 * @brief Format of topic used to publish outgoing messages.
 */
#define configPUBLISH_TOPIC                   publish_topic
#define configSUBSCRIBE_TOPIC                 subscribe_topic

/**
 * @brief Size of the static buffer to hold the topic name.
 */
#define configPUBLISH_TOPIC_BUFFER_LENGTH            ( sizeof( configPUBLISH_TOPIC_FORMAT ) - 1 )

/**
 * @brief Format of topic used to subscribe to incoming messages.
 *
 */
//#define configSUBSCRIBE_TOPIC_FORMAT           "mqtt_test/incoming"
#define configSUBSCRIBE_TOPIC_FORMAT   configPUBLISH_TOPIC_FORMAT

/**
 * @brief Size of the static buffer to hold the topic name.
 */
#define configSUBSCRIBE_TOPIC_BUFFER_LENGTH    ( sizeof( configSUBSCRIBE_TOPIC_FORMAT ) - 1 15

/*-----------------------------------------------------------*/

/**
 * @brief Defines the structure to use as the command callback context in this
 * demo.
 */
struct MQTTAgentCommandContext
{
  TaskHandle_t xTaskToNotify;
  void *pArgs;
};

/*-----------------------------------------------------------*/

static MQTTAgentHandle_t xMQTTAgentHandle = NULL;

/*-----------------------------------------------------------*/

/**
 * @brief Passed into MQTTAgent_Publish() as the callback to execute when the
 * broker ACKs the PUBLISH message.  Its implementation sends a notification
 * to the task that called MQTTAgent_Publish() to let the task know the
 * PUBLISH operation completed.  It also sets the xReturnStatus of the
 * structure passed in as the command's context to the value of the
 * xReturnStatus parameter - which enables the task to check the status of the
 * operation.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pxCommandContext Context of the initial command.
 * @param[in].xReturnStatus The result of the command.
 */
static void prvPublishCommandCallback(MQTTAgentCommandContext_t *pxCommandContext, MQTTAgentReturnInfo_t *pxReturnInfo);

/**
 * @brief Passed into MQTTAgent_Subscribe() as the callback to execute when
 * there is an incoming publish on the topic being subscribed to.  Its
 * implementation just logs information about the incoming publish including
 * the publish messages source topic and payload.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pvIncomingPublishCallbackContext Context of the initial command.
 * @param[in] pxPublishInfo Deserialized publish.
 */
static void prvIncomingPublishCallback(void *pvIncomingPublishCallbackContext, MQTTPublishInfo_t *pxPublishInfo);

/**
 * @brief Subscribe to the topic the demo task will also publish to - that
 * results in all outgoing publishes being published back to the task
 * (effectively echoed back).
 *
 * @param[in] xQoS The quality of service (QoS) to use.  Can be zero or one
 * for all MQTT brokers.  Can also be QoS2 if supported by the broker.  AWS IoT
 * does not support QoS2.
 */
static MQTTStatus_t prvSubscribeToTopic(MQTTQoS_t xQoS, char *pcTopicFilter);

/**
 * @brief Publishes the given payload using the given qos to the topic provided.
 *
 * Function queues a publish command with the MQTT agent and waits for response. For
 * Qos0 publishes command is successful when the message is sent out of network. For Qos1
 * publishes, the command succeeds once a puback is received. If publish is unsuccessful, the function
 * retries the publish for a configure number of tries.
 *
 * @param[in] xQoS The quality of service (QoS) to use.  Can be zero or one
 * for all MQTT brokers.  Can also be QoS2 if supported by the broker.  AWS IoT
 * does not support QoS2.
 * @param[in] pcTopic NULL terminated topic string to which message is published.
 * @param[in] pucPayload The payload blob to be published.
 * @param[in] xPayloadLength Length of the payload blob to be published.
 */
static MQTTStatus_t prvPublishToTopic(MQTTQoS_t xQoS, bool xRetain, char *pcTopic, uint8_t *pucPayload, size_t xPayloadLength);

/**
 * @brief The function that implements the task demonstrated by this file.
 *
 * @param pvParameters The parameters to the task.
 */
void vLEDTask(void *pvParameters);

/**
 * @brief Parses a JSON message to extract the desired LED state and updates system state accordingly.
 *
 * This function searches for the "ledStatus.LED_RED.desired" field within the provided JSON string.
 * Based on the parsed value ("ON" or "OFF"), it updates the `led_desired_status` and
 * `led_reported_status` variables, controls the physical LED using HAL GPIO calls, and
 * notifies the LED task by setting the corresponding event group bit.
 *
 * @param jsonMessage A pointer to a null-terminated JSON string containing the LED control information.
 */
void parseLedControlMessage(const char *jsonMessage);

/*-----------------------------------------------------------*/

/**
 * @brief The MQTT agent manages the MQTT contexts.  This set the handle to the
 * context used by this demo.
 */
extern MQTTAgentContext_t xGlobalMqttAgentContext;

/*-----------------------------------------------------------*/

/* -------------------------------------------------------------------------- */
/* JSON parsing for LED control                                               */
/* -------------------------------------------------------------------------- */

void parseLedControlMessage( const char *jsonMessage )
{
    JSONStatus_t result;
    char *       desiredValue = NULL;
    size_t       desiredLen   = 0;
    BaseType_t   anyChanged   = pdFALSE;

    /* First, try to parse as JSON: { "ledStatus": { "LED_NAME": { "desired": "ON" } } } */
    for( uint8_t i = 0; i < LED_COUNT; i++ )
    {
        const LedHwDescriptor_t *pxLed = &xLEDs[ i ];
        char keyPath[ 64 ];

        /* Build key path: "ledStatus.<LED_NAME>.desired" */
        int keyLen = snprintf( keyPath,
                               sizeof( keyPath ),
                               "ledStatus.%s.desired",
                               pxLed->name );

        if( keyLen <= 0 || keyLen >= ( int ) sizeof( keyPath ) )
        {
            continue;
        }

        desiredValue = NULL;
        desiredLen   = 0;

        result = JSON_Search( ( char * ) jsonMessage,
                              strlen( jsonMessage ),
                              keyPath,
                              ( size_t ) keyLen,
                              &desiredValue,
                              &desiredLen );

        if( result == JSONSuccess && desiredValue != NULL && desiredLen > 0 )
        {
            /* Compare against "ON" and "OFF" (unquoted JSON strings) */
            if( ( desiredLen == 2 ) && ( strncmp( desiredValue, "ON", desiredLen ) == 0 ) )
            {
                if( led_reported_status[ pxLed->index ] == pdFALSE )
                {
                    led_reported_status[ pxLed->index ] = pdTRUE;
                    anyChanged                          = pdTRUE;
                    HAL_GPIO_WritePin( pxLed->port, pxLed->pin, pxLed->onLevel );
                }
            }
            else if( ( desiredLen == 3 ) && ( strncmp( desiredValue, "OFF", desiredLen ) == 0 ) )
            {
                if( led_reported_status[ pxLed->index ] == pdTRUE )
                {
                    led_reported_status[ pxLed->index ] = pdFALSE;
                    anyChanged                          = pdTRUE;
                    HAL_GPIO_WritePin( pxLed->port, pxLed->pin, pxLed->offLevel );
                }
            }
            /* Other values are ignored */
        }
    }

    /* If JSON-based control did not change anything, support simple raw commands:
         *   "LED_RED_ON", "LED_RED_OFF", "LED_GREEN_ON", "LED_GREEN_OFF"
         */
        if( anyChanged == pdFALSE )
        {
            if( strcmp( jsonMessage, "LED_RED_ON" ) == 0 )
            {
                const LedHwDescriptor_t *pxLed = &xLEDs[ LED_RED_INDEX ];

                if( led_reported_status[ pxLed->index ] == pdFALSE )
                {
                    led_reported_status[ pxLed->index ] = pdTRUE;
                    HAL_GPIO_WritePin( pxLed->port, pxLed->pin, pxLed->onLevel );
                    anyChanged = pdTRUE;
                }
            }
            else if( strcmp( jsonMessage, "LED_RED_OFF" ) == 0 )
            {
                const LedHwDescriptor_t *pxLed = &xLEDs[ LED_RED_INDEX ];

                if( led_reported_status[ pxLed->index ] == pdTRUE )
                {
                    led_reported_status[ pxLed->index ] = pdFALSE;
                    HAL_GPIO_WritePin( pxLed->port, pxLed->pin, pxLed->offLevel );
                    anyChanged = pdTRUE;
                }
            }
            else if( strcmp( jsonMessage, "LED_GREEN_ON" ) == 0 )
            {
                const LedHwDescriptor_t *pxLed = &xLEDs[ LED_GREEN_INDEX ];

                if( led_reported_status[ pxLed->index ] == pdFALSE )
                {
                    led_reported_status[ pxLed->index ] = pdTRUE;
                    HAL_GPIO_WritePin( pxLed->port, pxLed->pin, pxLed->onLevel );
                    anyChanged = pdTRUE;
                }
            }
            else if( strcmp( jsonMessage, "LED_GREEN_OFF" ) == 0 )
            {
                const LedHwDescriptor_t *pxLed = &xLEDs[ LED_GREEN_INDEX ];

                if( led_reported_status[ pxLed->index ] == pdTRUE )
                {
                    led_reported_status[ pxLed->index ] = pdFALSE;
                    HAL_GPIO_WritePin( pxLed->port, pxLed->pin, pxLed->offLevel );
                    anyChanged = pdTRUE;
                }
            }
        }

    if( anyChanged == pdTRUE )
    {
        /* Notify the LED task of the change so it publishes updated reported JSON. */
        xEventGroupSetBits( xLedEventGroup, LED_STATUS_CHANGED_EVENT );
    }
}

static void prvPublishCommandCallback(MQTTAgentCommandContext_t *pxCommandContext, MQTTAgentReturnInfo_t *pxReturnInfo)
{
  if (pxCommandContext->xTaskToNotify != NULL)
  {
    xTaskNotify(pxCommandContext->xTaskToNotify, pxReturnInfo->returnCode, eSetValueWithOverwrite);
  }
}

/*-----------------------------------------------------------*/

/* -------------------------------------------------------------------------- */
/* Incoming publish callback (unchanged except calling new parser)            */
/* -------------------------------------------------------------------------- */

static void prvIncomingPublishCallback( void *pvIncomingPublishCallbackContext, MQTTPublishInfo_t *pxPublishInfo )
{
    static char cTerminatedString[ configPAYLOAD_BUFFER_LENGTH ];

    ( void ) pvIncomingPublishCallbackContext;

    if( pxPublishInfo->payloadLength < configPAYLOAD_BUFFER_LENGTH )
    {
        memcpy( ( void * ) cTerminatedString,
                pxPublishInfo->pPayload,
                pxPublishInfo->payloadLength );
        cTerminatedString[ pxPublishInfo->payloadLength ] = 0x00;
    }
    else
    {
        memcpy( ( void * ) cTerminatedString,
                pxPublishInfo->pPayload,
                configPAYLOAD_BUFFER_LENGTH );
        cTerminatedString[ configPAYLOAD_BUFFER_LENGTH - 1 ] = 0x00;
    }

    LogInfo( ( "Received incoming publish message %s", cTerminatedString ) );

    parseLedControlMessage( cTerminatedString );
}

/*-----------------------------------------------------------*/

static MQTTStatus_t prvSubscribeToTopic(MQTTQoS_t xQoS, char *pcTopicFilter)
{
  MQTTStatus_t xMQTTStatus;

  /* Loop in case the queue used to communicate with the MQTT agent is full and
   * attempts to post to it time out.  The queue will not become full if the
   * priority of the MQTT agent task is higher than the priority of the task
   * calling this function. */
  do
  {
    xMQTTStatus = MqttAgent_SubscribeSync(xMQTTAgentHandle, pcTopicFilter, xQoS, prvIncomingPublishCallback,
    NULL);

    if (xMQTTStatus != MQTTSuccess)
    {
      LogError(( "Failed to SUBSCRIBE to topic with error = %u.", xMQTTStatus ));
    }
    else
    {
      LogInfo(( "Subscribed to topic %.*s.\n\n", strlen( pcTopicFilter ), pcTopicFilter ));
    }
  }
  while (xMQTTStatus != MQTTSuccess);

  return xMQTTStatus;
}

/*-----------------------------------------------------------*/

static MQTTStatus_t prvPublishToTopic(MQTTQoS_t xQoS, bool xRetain, char *pcTopic, uint8_t *pucPayload, size_t xPayloadLength)
{
  MQTTPublishInfo_t xPublishInfo = { 0UL };
  MQTTAgentCommandContext_t xCommandContext = { 0 };
  MQTTStatus_t xMQTTStatus;
  BaseType_t xNotifyStatus;
  MQTTAgentCommandInfo_t xCommandParams = { 0UL };
  uint32_t ulNotifiedValue = 0U;

  /* Create a unique number of the subscribe that is about to be sent.  The number
   * is used as the command context and is sent back to this task as a notification
   * in the callback that executed upon receipt of the subscription acknowledgment.
   * That way this task can match an acknowledgment to a subscription. */
  xTaskNotifyStateClear(NULL);

  /* Configure the publish operation. */
  xPublishInfo.qos = xQoS;
  xPublishInfo.retain = xRetain;
  xPublishInfo.pTopicName = pcTopic;
  xPublishInfo.topicNameLength = (uint16_t) strlen(pcTopic);
  xPublishInfo.pPayload = pucPayload;
  xPublishInfo.payloadLength = xPayloadLength;

  xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

  xCommandParams.blockTimeMs = configMAX_COMMAND_SEND_BLOCK_TIME_MS;
  xCommandParams.cmdCompleteCallback = prvPublishCommandCallback;
  xCommandParams.pCmdCompleteCallbackContext = &xCommandContext;

  /* Loop in case the queue used to communicate with the MQTT agent is full and
   * attempts to post to it time out.  The queue will not become full if the
   * priority of the MQTT agent task is higher than the priority of the task
   * calling this function. */
  do
  {
    xMQTTStatus = MQTTAgent_Publish(xMQTTAgentHandle, &xPublishInfo, &xCommandParams);

    if (xMQTTStatus == MQTTSuccess)
    {
      /* Wait for this task to get notified, passing out the value it gets  notified with. */
      xNotifyStatus = xTaskNotifyWait(0, 0, &ulNotifiedValue, portMAX_DELAY);

      if (xNotifyStatus == pdTRUE)
      {
        if(ulNotifiedValue)
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
  while (xMQTTStatus != MQTTSuccess);

  return xMQTTStatus;
}

/*-----------------------------------------------------------*/

/* -------------------------------------------------------------------------- */
/* LED task                                                                   */
/* -------------------------------------------------------------------------- */

void vLEDTask( void *pvParameters )
{
    char *     cPayloadBuf;
    size_t     xPayloadLength;
    BaseType_t xStatus   = pdPASS;
    MQTTStatus_t xMQTTStatus;
    MQTTQoS_t  xQoS      = MQTTQoS1;
    bool       xRetain   = pdTRUE;
    char *     pThingName = NULL;
    size_t     uxTempSize = 0;
    EventBits_t uxBits;

    ( void ) pvParameters;

    vSleepUntilMQTTAgentReady();

    xMQTTAgentHandle = xGetMqttAgentHandle();
    configASSERT( xMQTTAgentHandle != NULL );

    vSleepUntilMQTTAgentConnected();

    LogInfo( ( "MQTT Agent is connected. Starting the LED publish and subscribe task." ) );

    pThingName = KVStore_getStringHeap( CS_CORE_THING_NAME, &uxTempSize );
    configASSERT( pThingName != NULL );

    cPayloadBuf = ( char * ) pvPortMalloc( configPAYLOAD_BUFFER_LENGTH );
    configASSERT( cPayloadBuf != NULL );

    /* Single shared reported/desired topics for all LEDs */
    snprintf( configPUBLISH_TOPIC,   MAXT_TOPIC_LENGTH, "%s/led/reported", pThingName );
    snprintf( configSUBSCRIBE_TOPIC, MAXT_TOPIC_LENGTH, "%s/led/desired",  pThingName );

    xLedEventGroup = xEventGroupCreate();
    configASSERT( xLedEventGroup != NULL );

    if( xStatus == pdPASS )
    {
        xMQTTStatus = prvSubscribeToTopic( xQoS, configSUBSCRIBE_TOPIC );
        if( xMQTTStatus != MQTTSuccess )
        {
            LogError( ( "Failed to subscribe to topic: %s.", configSUBSCRIBE_TOPIC ) );
            vTaskDelete( NULL );
        }
    }

    /* Force initial state publish */
    xEventGroupSetBits( xLedEventGroup, LED_STATUS_CHANGED_EVENT );

    for( ;; )
    {
        uxBits = xEventGroupWaitBits( xLedEventGroup,
                                      LED_STATUS_CHANGED_EVENT,
                                      pdTRUE,
                                      pdTRUE,
                                      portMAX_DELAY );

        if( ( uxBits & LED_STATUS_CHANGED_EVENT ) != 0 )
        {
            /* Keep a copy if you want to check changes vs previous state */
            for( uint8_t i = 0; i < LED_COUNT; i++ )
            {
                led_last_status[ i ] = led_reported_status[ i ];
            }

            xPayloadLength = snprintf(
                cPayloadBuf,
                configPAYLOAD_BUFFER_LENGTH,
                "{ \"ledStatus\": { "
                "\"LED_RED\":   { \"reported\": \"%s\" }, "
                "\"LED_GREEN\": { \"reported\": \"%s\" } "
                "} }",
                led_reported_status[ xLEDs[ LED_RED_INDEX   ].index ] ? "ON" : "OFF",
                led_reported_status[ xLEDs[ LED_GREEN_INDEX ].index ] ? "ON" : "OFF" );

            configASSERT( xPayloadLength < configPAYLOAD_BUFFER_LENGTH );

            LogInfo( ( "Publishing to topic: %s, message: %.*s",
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
                LogInfo( ( "Successfully published to topic: %s",
                           configPUBLISH_TOPIC ) );
            }
            else
            {
                LogError( ( "Failed to publish to topic: %s",
                            configPUBLISH_TOPIC ) );
            }
        }
    }
}

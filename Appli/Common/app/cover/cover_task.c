/*
 * cover_task.c
 *
 * Home Assistant MQTT cover integration using relay pulse outputs.
 * Refactored with explicit state machine and timing-based motion inference.
 */

#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "sys_evt.h"

#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "mqtt_agent_task.h"
#include "subscription_manager.h"
#include "kvstore.h"
#include "core_json.h"

#include "interrupt_handlers.h"

#if (USE_MAGNETIC_SENSOR)&&(USE_RANGING_SENSOR)
#error please select either USE_MAGNETIC_SENSOR or USE_RANGING_SENSOR
#endif

#if (USE_RANGING_SENSOR) && (NUM_COVERS > 1)
#error only one cover is allowed when using the USE_RANGING_SENSOR
#endif

/* -------------------------------------------------------------------------- */
/* Timing configuration                                                       */
/* -------------------------------------------------------------------------- */

/* Motion inference windows (in ticks) */
#define COVER_OPEN_TIME_TICKS      pdMS_TO_TICKS(14000)       /* ~14s to fully open   */
#define COVER_CLOSE_TIME_TICKS     pdMS_TO_TICKS(11000)       /* ~11s to fully close  */
#define COVER_STOP_MAX_HOLD_TICKS  pdMS_TO_TICKS(60 * 50000)  /* ~5mn MAX STOP        */

/* Fixed hysteresis */
#define DOOR_OPEN_THRESHOLD_MM     500   /* 50 cm */
#define DOOR_CLOSE_THRESHOLD_MM    800   /* 80 cm */

/* -------------------------------------------------------------------------- */
/* Cover model                                                                */
/* -------------------------------------------------------------------------- */

typedef enum
{
    eCOVER_STATE_OPEN,
    eCOVER_STATE_CLOSED,
    eCOVER_STATE_STOPPED,
    eCOVER_STATE_CLOSING,
    eCOVER_STATE_OPENING,
    eCOVER_STATE_UNKNOWN
} eCoverState_t;

typedef enum
{
    eCOVER_COMMAND_OPEN,
    eCOVER_COMMAND_CLOSE,
    eCOVER_COMMAND_STOP,
} eCoverCommand_t;

typedef struct
{
    uint16_t      usPin;
    GPIO_TypeDef *pxPort;
} Relay_t;

#if USE_MAGNETIC_SENSOR
typedef struct
{
    GPIO_TypeDef *pxPort;
    uint16_t      usPin;
    GPIO_PinState xOpenState;
} DoorSensor_t;
#endif

typedef struct Cover_t
{
    const char        *pcName;
    const Relay_t      xRelay;
#if USE_MAGNETIC_SENSOR
    const DoorSensor_t xSensor;
#endif
    eCoverState_t      eStateReported;
    eCoverCommand_t    eLastCommand;
    uint32_t           uLastCommandTime;
} Cover_t;

#if USE_RANGING_SENSOR
extern QueueHandle_t xDistanceQueue;
#endif

/* -------------------------------------------------------------------------- */
/* Static cover table                                                         */
/* -------------------------------------------------------------------------- */

static Cover_t xCovers[] =
{
#if (NUM_COVERS > 0)
    {
        .pcName = "GARAGE_DOOR_1",
        .xRelay = {
            .usPin      = RELAY_1_Pin,
            .pxPort     = RELAY_1_Port
        },
    #if USE_MAGNETIC_SENSOR
        .xSensor = {
            .pxPort     = DOOR_SENSPR_1_Port,
            .usPin      = DOOR_SENSPR_1_Pin,
            .xOpenState = DOOR_SENSPR_1_STATE_OPEN
        },
    #endif
        .eStateReported    = eCOVER_STATE_UNKNOWN,
        .eLastCommand      = eCOVER_COMMAND_STOP,
        .uLastCommandTime  = 0
    }
#endif

#if (NUM_COVERS > 1)
    ,{
        .pcName = "GARAGE_DOOR_2",
        .xRelay = {
            .usPin      = RELAY_2_Pin,
            .pxPort     = RELAY_2_Port
        },
    #if USE_MAGNETIC_SENSOR
        .xSensor = {
            .pxPort     = DOOR_SENSPR_2_Port,
            .usPin      = DOOR_SENSPR_2_Pin,
            .xOpenState = DOOR_SENSPR_2_STATE_OPEN
        },
    #endif
        .eStateReported    = eCOVER_STATE_UNKNOWN,
        .eLastCommand      = eCOVER_COMMAND_STOP,
        .uLastCommandTime  = 0
    }
#endif

#if (NUM_COVERS > 2)
    ,{
        .pcName = "GARAGE_DOOR_3",
        .xRelay = {
            .usPin      = RELAY_3_Pin,
            .pxPort     = RELAY_3_Port
        },
    #if USE_MAGNETIC_SENSOR
        .xSensor = {
            .pxPort     = DOOR_SENSPR_3_Port,
            .usPin      = DOOR_SENSPR_3_Pin,
            .xOpenState = DOOR_SENSPR_3_STATE_OPEN
        },
    #endif
        .eStateReported    = eCOVER_STATE_UNKNOWN,
        .eLastCommand      = eCOVER_COMMAND_STOP,
        .uLastCommandTime  = 0
    }
#endif
};

#define COVER_COUNT ( (uint8_t)( sizeof( xCovers ) / sizeof( xCovers[0] ) ) )

/* -------------------------------------------------------------------------- */
/* MQTT topics                                                                */
/* -------------------------------------------------------------------------- */

#define MAX_TOPIC_LEN 128

static char thingName[64];
static MQTTAgentHandle_t xMQTTAgentHandle = NULL;

/* -------------------------------------------------------------------------- */
/* Command context for synchronous publish                                    */
/* -------------------------------------------------------------------------- */

typedef struct MQTTAgentCommandContext
{
    TaskHandle_t xTaskToNotify;
    void       *pArgs;
} MQTTAgentCommandContext_t;

/* -------------------------------------------------------------------------- */
/* Door sensor + state machine                                                */
/* -------------------------------------------------------------------------- */

/* Read raw physical state from sensor/ranging */
static eCoverState_t prvReadPhysicalDoorState(uint8_t ucIndex)
{
#if USE_MAGNETIC_SENSOR

    const DoorSensor_t *pxSensor = &xCovers[ucIndex].xSensor;
    GPIO_PinState xRaw = HAL_GPIO_ReadPin(pxSensor->pxPort, pxSensor->usPin);

    return (xRaw == pxSensor->xOpenState)
           ? eCOVER_STATE_OPEN
           : eCOVER_STATE_CLOSED;

#elif USE_RANGING_SENSOR

    static uint32_t distance_mm = 0;
    static BaseType_t hasDistance = pdFALSE;

    uint32_t newDistance = 0;

    /* Non-blocking read: update only if a new sample is available */
    if (xQueueReceive(xDistanceQueue, &newDistance, 0) == pdTRUE)
    {
        distance_mm = newDistance;
        hasDistance = pdTRUE;
    }

    /* No distance ever received → cannot infer */
    if (!hasDistance)
    {
        return eCOVER_STATE_UNKNOWN;
    }

    /* Hard thresholds */
    if (distance_mm < DOOR_OPEN_THRESHOLD_MM)
    {
        return eCOVER_STATE_OPEN;
    }

    if (distance_mm > DOOR_CLOSE_THRESHOLD_MM)
    {
        return eCOVER_STATE_CLOSED;
    }

    /* Between thresholds → ambiguous */
    return eCOVER_STATE_UNKNOWN;

#else
    (void) ucIndex;
    return eCOVER_STATE_UNKNOWN;
#endif
}

/* Combine physical state + last command + timing into a richer state */
static eCoverState_t prvReadDoorStateWithInference(uint8_t ucIndex)
{
    Cover_t *pxCover = &xCovers[ucIndex];

    eCoverState_t ePhysical = prvReadPhysicalDoorState(ucIndex);

    uint32_t now     = xTaskGetTickCount();
    uint32_t elapsed = now - pxCover->uLastCommandTime;

    /* 1. CLOSED is the ONLY definitive physical state */
    if (ePhysical == eCOVER_STATE_CLOSED)
    {
        return eCOVER_STATE_CLOSED;
    }

    /* 3. Inference based on last command + timing */
    switch (pxCover->eLastCommand)
    {
        case eCOVER_COMMAND_CLOSE:
            /* Within close window → infer CLOSING */
            if (elapsed < COVER_CLOSE_TIME_TICKS)
            {
                return eCOVER_STATE_CLOSING;
            }

            /* Close window expired, reed not closed → treat as OPEN */
            return eCOVER_STATE_OPEN;
            break;

        case eCOVER_COMMAND_OPEN:
            /* Within open window → infer OPENING */
            if (elapsed < COVER_OPEN_TIME_TICKS)
            {
                return eCOVER_STATE_OPENING;
            }

            /* Open window expired, reed not closed → OPEN */
            return eCOVER_STATE_OPEN;
            break;

        case eCOVER_COMMAND_STOP:
            if (elapsed < COVER_STOP_MAX_HOLD_TICKS)
            {
              return eCOVER_STATE_STOPPED;
            }

            return eCOVER_STATE_OPEN;
            break;

        default:
          return eCOVER_STATE_OPEN;
            break;
    }

    /* 4. No active inference window → rely on sensor */
    return eCOVER_STATE_OPEN;   // reed not closed → OPEN
}

/* -------------------------------------------------------------------------- */
/* Relay pulse and motor actions                                              */
/* -------------------------------------------------------------------------- */

static void prvRelayPulse(const Relay_t *pxRelay)
{
    HAL_GPIO_WritePin(pxRelay->pxPort, pxRelay->usPin, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(1000));
    HAL_GPIO_WritePin(pxRelay->pxPort, pxRelay->usPin, GPIO_PIN_RESET);
}

static void prvCoverMotorOpen(Cover_t *pxCover)
{
    prvRelayPulse(&pxCover->xRelay);
}

static void prvCoverMotorClose(Cover_t *pxCover)
{
    prvRelayPulse(&pxCover->xRelay);
}

static void prvCoverMotorStop(Cover_t *pxCover)
{
    prvRelayPulse(&pxCover->xRelay);
}

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static Cover_t *prvFindCoverByName(const char *pcName)
{
    for(uint8_t i = 0; i < COVER_COUNT; i++)
    {
        if(strcmp(xCovers[i].pcName, pcName) == 0)
            return &xCovers[i];
    }
    return NULL;
}

static const char *prvStateToString(eCoverState_t eState)
{
    switch(eState)
    {
        case eCOVER_STATE_OPEN:    return "open";
        case eCOVER_STATE_CLOSED:  return "closed";
        case eCOVER_STATE_STOPPED: return "stopped";
        case eCOVER_STATE_CLOSING: return "closing";
        case eCOVER_STATE_OPENING: return "opening";
        default:                   return "unknown";
    }
}

/* -------------------------------------------------------------------------- */
/* Publish command callback                                                   */
/* -------------------------------------------------------------------------- */

static void prvPublishCommandCallback(MQTTAgentCommandContext_t *pxCtx,
                                      MQTTAgentReturnInfo_t     *pxReturn)
{
    if(pxCtx->xTaskToNotify != NULL)
    {
        xTaskNotify(pxCtx->xTaskToNotify,
                    pxReturn->returnCode,
                    eSetValueWithOverwrite);
    }
}

/* -------------------------------------------------------------------------- */
/* Synchronous publish wrapper                                                */
/* -------------------------------------------------------------------------- */

static MQTTStatus_t prvPublishToTopic(MQTTQoS_t xQoS,
                                      bool      bRetain,
                                      char     *pcTopic,
                                      uint8_t  *pucPayload,
                                      size_t    xPayloadLen)
{
    MQTTPublishInfo_t         xPubInfo = {0};
    MQTTAgentCommandInfo_t    xCmdInfo = {0};
    MQTTAgentCommandContext_t xCtx     = {0};
    MQTTStatus_t              xStatus;
    uint32_t                  ulNotifyVal = 0;

    xTaskNotifyStateClear(NULL);

    xPubInfo.qos             = xQoS;
    xPubInfo.retain          = bRetain;
    xPubInfo.pTopicName      = pcTopic;
    xPubInfo.topicNameLength = (uint16_t) strlen(pcTopic);
    xPubInfo.pPayload        = pucPayload;
    xPubInfo.payloadLength   = xPayloadLen;

    xCtx.xTaskToNotify = xTaskGetCurrentTaskHandle();

    xCmdInfo.blockTimeMs                 = 500;
    xCmdInfo.cmdCompleteCallback         = prvPublishCommandCallback;
    xCmdInfo.pCmdCompleteCallbackContext = &xCtx;

    do
    {
        xStatus = MQTTAgent_Publish(xMQTTAgentHandle, &xPubInfo, &xCmdInfo);

        if(xStatus == MQTTSuccess)
        {
            if(xTaskNotifyWait(0, 0, &ulNotifyVal, portMAX_DELAY) == pdTRUE)
                xStatus = (ulNotifyVal == 0) ? MQTTSuccess : MQTTSendFailed;
            else
                xStatus = MQTTSendFailed;
        }
    }
    while(xStatus != MQTTSuccess);

    return xStatus;
}

/* -------------------------------------------------------------------------- */
/* State publishing helpers                                                   */
/* -------------------------------------------------------------------------- */

static void prvPublishSingleCoverState(Cover_t *pxCover)
{
    char pcTopic[MAX_TOPIC_LEN];
    char pcPayload[32];

    snprintf(pcTopic, sizeof(pcTopic),
             "%s/cover/%s/state",
             thingName, pxCover->pcName);

    const char *pcState = prvStateToString(pxCover->eStateReported);

    size_t xLen = snprintf(pcPayload, sizeof(pcPayload), "%s", pcState);

    prvPublishToTopic(MQTTQoS1, true,
                      pcTopic,
                      (uint8_t *)pcPayload,
                      xLen);
}

static void prvPublishCoverStates(void)
{
    for(uint8_t i = 0; i < COVER_COUNT; i++)
    {
        Cover_t *pxCover = &xCovers[i];

        /* Initial read + publish */
        pxCover->eStateReported = prvReadDoorStateWithInference(i);
        prvPublishSingleCoverState(pxCover);
    }
}

/* -------------------------------------------------------------------------- */
/* JSON / command parsing                                                     */
/* -------------------------------------------------------------------------- */

static void prvHandleCoverCommand(const char *pcCoverName,
                                  const char *pcCommand)
{
    Cover_t *pxCover = prvFindCoverByName(pcCoverName);

    if(pxCover == NULL)
    {
        LogWarn(("Unknown cover name: %s", pcCoverName));
        return;
    }

    uint32_t now = xTaskGetTickCount();

    if(strcmp(pcCommand, "OPEN") == 0)
    {
        prvCoverMotorOpen(pxCover);

        pxCover->eLastCommand     = eCOVER_COMMAND_OPEN;
        pxCover->uLastCommandTime = now;

        xEventGroupSetBits(xSystemEvents, EVT_DOOR_STATE_CHANGED);
    }
    else if(strcmp(pcCommand, "CLOSE") == 0)
    {
        prvCoverMotorClose(pxCover);

        pxCover->eLastCommand     = eCOVER_COMMAND_CLOSE;
        pxCover->uLastCommandTime = now;

        xEventGroupSetBits(xSystemEvents, EVT_DOOR_STATE_CHANGED);
    }
    else if(strcmp(pcCommand, "STOP") == 0)
    {
        prvCoverMotorStop(pxCover);

        pxCover->eLastCommand     = eCOVER_COMMAND_STOP;
        pxCover->uLastCommandTime = now;

        xEventGroupSetBits(xSystemEvents, EVT_DOOR_STATE_CHANGED);
    }
}

/* -------------------------------------------------------------------------- */
/* Incoming publish callback                                                  */
/* -------------------------------------------------------------------------- */

static void prvIncomingPublishCallback(void *pvCtx,
                                       MQTTPublishInfo_t *pxInfo)
{
    (void) pvCtx;

    char pcTopic[MAX_TOPIC_LEN];
    char pcPayload[64];

    size_t xTopicLen = pxInfo->topicNameLength;
    if(xTopicLen >= sizeof(pcTopic))
        xTopicLen = sizeof(pcTopic) - 1;

    memcpy(pcTopic, pxInfo->pTopicName, xTopicLen);
    pcTopic[xTopicLen] = '\0';

    size_t xPayloadLen = pxInfo->payloadLength;
    if(xPayloadLen >= sizeof(pcPayload))
        xPayloadLen = sizeof(pcPayload) - 1;

    memcpy(pcPayload, pxInfo->pPayload, xPayloadLen);
    pcPayload[xPayloadLen] = '\0';

    const char *pc = strstr(pcTopic, "/cover/");
    if(pc == NULL)
        return;

    pc += strlen("/cover/");
    const char *pcNameStart = pc;
    const char *pcSlash = strchr(pcNameStart, '/');
    if(pcSlash == NULL)
        return;

    char pcCoverName[32];
    size_t xNameLen = (size_t)(pcSlash - pcNameStart);
    if(xNameLen >= sizeof(pcCoverName))
        xNameLen = sizeof(pcCoverName) - 1;

    memcpy(pcCoverName, pcNameStart, xNameLen);
    pcCoverName[xNameLen] = '\0';

    prvHandleCoverCommand(pcCoverName, pcPayload);
}

/* -------------------------------------------------------------------------- */
/* Subscribe                                                                  */
/* -------------------------------------------------------------------------- */

static MQTTStatus_t prvSubscribeToCovers(MQTTQoS_t xQoS)
{
    char pcFilter[MAX_TOPIC_LEN];

    snprintf(pcFilter, sizeof(pcFilter),
             "%s/cover/+/desired", thingName);

    MQTTStatus_t xStatus;

    do
    {
        xStatus = MqttAgent_SubscribeSync(xMQTTAgentHandle,
                                          pcFilter,
                                          xQoS,
                                          prvIncomingPublishCallback,
                                          NULL);

        if(xStatus != MQTTSuccess)
            LogError(("Failed to subscribe: %s", pcFilter));
        else
            LogInfo(("Subscribed: %s", pcFilter));

    } while(xStatus != MQTTSuccess);

    return xStatus;
}

/* -------------------------------------------------------------------------- */
/* Door sensor interrupts (magnetic)                                          */
/* -------------------------------------------------------------------------- */

#if USE_MAGNETIC_SENSOR
static void prvDoorSensorInterrupt(void *pvCtx)
{
    (void) pvCtx;
    xEventGroupSetBits(xSystemEvents, EVT_DOOR_STATE_CHANGED);
}

static void prvRegisterDoorInterrupts(void)
{
    for(uint8_t i = 0; i < COVER_COUNT; i++)
    {
        const DoorSensor_t *pxSensor = &xCovers[i].xSensor;

        GPIO_EXTI_Register_Rising_Callback(pxSensor->usPin,
                                           prvDoorSensorInterrupt,
                                           (void *)(uintptr_t)i);

        GPIO_EXTI_Register_Falling_Callback(pxSensor->usPin,
                                            prvDoorSensorInterrupt,
                                            (void *)(uintptr_t)i);
    }
}
#endif

/* -------------------------------------------------------------------------- */
/* Cover task                                                                 */
/* -------------------------------------------------------------------------- */

void vCoverTask(void *pvParams)
{
    (void) pvParams;

    vSleepUntilMQTTAgentReady();
    xMQTTAgentHandle = xGetMqttAgentHandle();
    configASSERT(xMQTTAgentHandle != NULL);

    vSleepUntilMQTTAgentConnected();

    size_t xThingLen = 0;
    char *pcThing = KVStore_getStringHeap(CS_CORE_THING_NAME, &xThingLen);
    configASSERT(pcThing != NULL);

    memcpy(thingName, pcThing, xThingLen);
    thingName[xThingLen] = '\0';
    vPortFree(pcThing);

    LogInfo(("Cover task starting for thing: %s", thingName));

#if USE_RANGING_SENSOR
    configASSERT(xDistanceQueue != NULL);
#endif

#if USE_MAGNETIC_SENSOR
    prvRegisterDoorInterrupts();
#endif

    prvSubscribeToCovers(MQTTQoS1);

    /* Initial state publish */
    prvPublishCoverStates();

    for (;;)
    {
        xEventGroupWaitBits( xSystemEvents,
                             EVT_DOOR_STATE_CHANGED,
                             pdTRUE,
                             pdFALSE,
                             pdMS_TO_TICKS(1000));   // wake every 1 second

        for (uint8_t i = 0; i < COVER_COUNT; i++)
        {
            eCoverState_t eSensed = prvReadDoorStateWithInference(i);

            if (eSensed != xCovers[i].eStateReported)
            {
                xCovers[i].eStateReported = eSensed;
                prvPublishSingleCoverState(&xCovers[i]);
            }
        }
    }
}

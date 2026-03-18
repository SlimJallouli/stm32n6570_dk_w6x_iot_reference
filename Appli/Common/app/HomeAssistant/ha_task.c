/*
 * Home Assistant MQTT discovery task (refactored from HomeAssistant_Task.c).
 *
 * This file keeps the main control flow/task loop. Entity-specific discovery
 * payloads and command handlers are split into separate modules.
 *
 */

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "sys_evt.h"

/* MQTT agent includes. */
#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "mqtt_agent_task.h"

/* App includes. */
#include "kvstore.h"
#include "ha_config.h"
#include "ha_helpers.h"
#include "ha_globals.h"

/* Entity modules. */
#include "ha_entities_ota.h"
#include "ha_entities_led.h"
#include "ha_entities_button.h"
#include "ha_entities_sensors.h"
#include "ha_entities_reset.h"
#include "ha_entities_cover.h"
#include "ha_entities_imu.h"

/*-----------------------------------------------------------*/
/* Globals (shared across modules). */

/* Reusable topic buffer (shared across modules via ha_config.h). */
char * pcPublishTopic = NULL;

/* Event group used to track HA commands and OTA state. */
EventGroupHandle_t xHAEventGroup;

/*-----------------------------------------------------------*/
/* Home Assistant status handler: triggers discovery republish when HA is online. */

void HA_Handle_LWT( void * pxSubscriptionContext,
                    MQTTPublishInfo_t * pPublishInfo )
{
    ( void ) pxSubscriptionContext;

    if( ( pPublishInfo == NULL ) ||
        ( pPublishInfo->pPayload == NULL ) ||
        ( pPublishInfo->payloadLength == 0 ) )
    {
        return;
    }

    const char * payload = ( const char * ) pPublishInfo->pPayload;

    /* Ensure payload is null-terminated for comparison. */
    char tempPayload[ 16 ] = { 0 };
    size_t copyLen = ( pPublishInfo->payloadLength < ( sizeof( tempPayload ) - 1 ) ) ?
                     pPublishInfo->payloadLength :
                     ( sizeof( tempPayload ) - 1 );

    ( void ) memcpy( tempPayload, payload, copyLen );
    tempPayload[ copyLen ] = '\0';

    if( strcmp( tempPayload, "online" ) == 0 )
    {
        LogInfo( ( "homeassistant/status=online -> republish discovery config." ) );
        xEventGroupSetBits( xHAEventGroup, EVT_COMMAND_REPUBLISH_CONFIG );
    }
    else
    {
        /* Optional: if you want to do anything on offline, do it here. */
        LogDebug( ( "homeassistant/status: %s", tempPayload ) );
    }
}

/*-----------------------------------------------------------*/
/* Republish all Home Assistant discovery configs. */

static void prvPublishAllHAConfigs( const char * pcThingName,
                                   char * pcPayloadBuffer )
{
    configASSERT( pcThingName != NULL );
    configASSERT( pcPayloadBuffer != NULL );

#if ( DEMO_AWS_OTA == 1 )
    HA_OTA_PublishConfig( pcThingName, pcPayloadBuffer );
#else
    HA_OTA_ClearConfig( pcThingName );
#endif

#if ( DEMO_LED == 1 )
    HA_LED_PublishConfig( pcThingName, pcPayloadBuffer );
#else
    HA_LED_ClearConfig( pcThingName );
#endif

#if ( DEMO_BUTTON == 1 )
    HA_Button_PublishConfig( pcThingName, pcPayloadBuffer );
#else
    HA_Button_ClearConfig( pcThingName );
#endif

#if ( DEMO_ENV_SENSOR == 1 )
    HA_EnvSensors_PublishConfig( pcThingName, pcPayloadBuffer );
#else
    HA_EnvSensors_ClearConfig( pcThingName );
#endif

#if ( DEMO_MOTION_SENSOR == 1 )
    HA_MotionSensors_PublishConfig( pcThingName, pcPayloadBuffer );
#else
    HA_MotionSensors_ClearConfig( pcThingName );
#endif

#if ( DEMO_RANGING_SENSOR == 1 )
    HA_RangingSensor_PublishConfig( pcThingName, pcPayloadBuffer );
#else
    HA_RangingSensor_ClearConfig( pcThingName );
#endif

#if ( DEMO_COVER == 1 )
    HA_COVER_PublishConfig( pcThingName, pcPayloadBuffer );
#else
    HA_COVER_ClearConfig( pcThingName );
#endif

#if ( DEMO_MOTION_IMU == 1 )
    HA_IMU_PublishConfig( pcThingName, pcPayloadBuffer );
#else
    HA_IMU_ClearConfig( pcThingName );
#endif

    /* Reset entity (reboot button) discovery. */
    HA_Reset_PublishConfig( pcThingName, pcPayloadBuffer );

    /* Small delay to avoid broker flooding. */
    vTaskDelay( pdMS_TO_TICKS( 1000 ) );

#if ( DEMO_AWS_OTA == 1 )
    /* Publish the firmware version. */
    ( void ) HA_OTA_PublishFirmwareVersionStatus( appFirmwareVersion,
                                                  appFirmwareVersion,
                                                  pcThingName,
                                                  FW_UPDATE_STATUS_COMPLETED );
#endif

    /* Publish the availability message. */
    HA_PublishAvailabilityStatus( pcThingName, pcPayloadBuffer, "online" );

    LogInfo("HomeAssistant discovery publish completed.");
}

/*-----------------------------------------------------------*/
/* Main task. */

void vHAConfigPublishTask(void *pvParameters)
{
  (void) pvParameters;

  size_t uxThingNameLen = 0;

#if ( DEMO_AWS_OTA == 1 )
  BaseType_t xOtaInProgress = pdFALSE;
#endif

  /* Wait until the MQTT agent is ready and connected. */
  vSleepUntilMQTTAgentReady();

  MQTTAgentHandle_t xAgentHandle = xGetMqttAgentHandle();
  configASSERT(xAgentHandle != NULL);

  vSleepUntilMQTTAgentConnected();

  /* Share the handle with helper layer. */
  HAHelpers_SetMqttAgentHandle(xAgentHandle);

  /* Load device identity and allocate shared publish buffers. */
  char *pcThingName = KVStore_getStringHeap(CS_CORE_THING_NAME, &uxThingNameLen);
  configASSERT(pcThingName != NULL);

  char *pcPayloadBuffer = (char*) pvPortMalloc( HA_CONFIG_PAYLOAD_BUFFER_LENGTH);
  configASSERT(pcPayloadBuffer != NULL);

  pcPublishTopic = (char*) pvPortMalloc( HA_CONFIG_MAX_TOPIC_LENGTH);
  configASSERT(pcPublishTopic != NULL);

  LogInfo("Publishing Home Assistant discovery configuration for device: %s", pcThingName);

  xHAEventGroup = xEventGroupCreate();
  configASSERT(xHAEventGroup != NULL);

  /*-------------------------------------------------------*/
#if ( DEMO_AWS_OTA == 1 )
  /* Subscribe to OTA update command topic first (so commands are accepted immediately). */
  (void) HA_SubscribeToThingTopic( pcThingName,
                                  "fw/update",
                                  HA_OTA_HandleFwUpdateCommand,
                                  "FW update" );
#endif

  /* Subscribe to reboot command (so commands are accepted immediately). */
  (void) HA_SubscribeToThingTopic( pcThingName,
                                  "cmd/action",
                                  HA_Reset_HandleRebootCommand,
                                  "reboot command" );

  /* Subscribe to Home Assistant status (birth/LWT). */
  (void) HA_SubscribeToTopic( "homeassistant/status",
                                  HA_Handle_LWT );

  /*-------------------------------------------------------*/

  /* Publish discovery config for all entities. */
  prvPublishAllHAConfigs(pcThingName, pcPayloadBuffer);

  /*-------------------------------------------------------*/
  for (;;)
  {
    EventBits_t uxBits = xEventGroupWaitBits(xHAEventGroup,
#if ( DEMO_AWS_OTA == 1 )
        EVT_OTA_UPDATE_AVAILABLE |
        EVT_OTA_UPDATE_START     |
        EVT_OTA_COMPLETED        |
        EVT_OTA_UPDATE_ABORT     |
#endif
        EVT_COMMAND_RESET        |
        EVT_COMMAND_REPUBLISH_CONFIG,
        pdTRUE,
        pdFALSE, HA_GetJitteredTimeoutTicks());

#if ( DEMO_AWS_OTA == 1 )
    if ((uxBits & EVT_OTA_UPDATE_AVAILABLE) != 0)
    {
      LogInfo("New Firmware available.");
      (void) HA_OTA_PublishFirmwareVersionStatus(appFirmwareVersion, newAppFirmwareVersion, pcThingName, FW_UPDATE_STATUS_IDLE);
    }

    if ((uxBits & EVT_OTA_UPDATE_START) != 0)
    {
      LogInfo("Firmware upload starting...");
      xOtaInProgress = pdTRUE;

      (void) HA_OTA_PublishFirmwareVersionStatus(appFirmwareVersion, newAppFirmwareVersion, pcThingName, FW_UPDATE_STATUS_UPDATING);

      /* Start OTA progress reporting task. */
      (void) xTaskCreate( vOtaProgressTask,
                          "OTA_Progress",
                          1024,
                          ( void * ) pcThingName,
                          tskIDLE_PRIORITY + 2,
                          NULL );
    }

    if ((uxBits & EVT_OTA_COMPLETED) != 0)
    {
      LogInfo("New Firmware uploaded");
      LogInfo("Generating reset to install New Firmware");

      xOtaInProgress = pdFALSE; /* Not strictly needed; we reset, but keeps logic clean. */
      vDoSystemReset();
    }

    if ((uxBits & EVT_OTA_UPDATE_ABORT) != 0)
    {
      LogInfo( "Firmware update aborted..." );
      xOtaInProgress = pdFALSE;

      prvPublishAllHAConfigs(pcThingName, pcPayloadBuffer);
    }
#endif /* DEMO_AWS_OTA */

    if ((uxBits & EVT_COMMAND_RESET) != 0)
    {
#if ( DEMO_AWS_OTA == 1 )
      if (xOtaInProgress == pdTRUE)
      {
        LogInfo("Skipping Reboot command (OTA in progress).");
        continue;
      }
#endif
      LogInfo( "Reboot command" );
      HA_PublishAvailabilityStatus(pcThingName, pcPayloadBuffer, "offline");
      vTaskDelay(pdMS_TO_TICKS(1000));
      vDoSystemReset();
    }

    if (((uxBits & EVT_COMMAND_REPUBLISH_CONFIG) != 0) || (uxBits == 0))
    {
#if ( DEMO_AWS_OTA == 1 )
      if (xOtaInProgress == pdTRUE)
      {
        LogInfo("Skipping discovery republish (OTA in progress).");
        continue;
      }
#endif

      vTaskDelay(( uxRand() % ( 2 * 60000 ) )); /* Add a random delay so not all the devices re-send HAConfigs messages at the same time */

      LogInfo("Republishing Home Assistant discovery (reason: %s).",
             (( uxBits & EVT_COMMAND_REPUBLISH_CONFIG ) != 0 ) ? "HA online" : "periodic");

      prvPublishAllHAConfigs(pcThingName, pcPayloadBuffer);
    }
  }
}

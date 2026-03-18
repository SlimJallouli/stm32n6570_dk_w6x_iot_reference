#pragma once

#include "ha_helpers.h"
#include "sys_evt.h"
#include "ha_globals.h"

#ifdef __cplusplus
extern "C" {
#endif

#if (DEMO_AWS_OTA == 1)
extern volatile AppVersion32_t newAppFirmwareVersion;
#endif

/* Publish or clear HA discovery config for OTA update entity. */
void HA_OTA_PublishConfig( const char * pcThingName, char * pcPayloadBuffer );
void HA_OTA_ClearConfig( const char * pcThingName );

#if (DEMO_AWS_OTA == 1)

typedef enum FwUpdateStatus_t
{
    FW_UPDATE_STATUS_IDLE = 0,
    FW_UPDATE_STATUS_UPDATING,
    FW_UPDATE_STATUS_COMPLETED
} FwUpdateStatus_t;

/* Publish the firmware version/state JSON to <thing>/fw/state. */
MQTTStatus_t HA_OTA_PublishFirmwareVersionStatus( AppVersion32_t appFirmwareVersion,
                                                  AppVersion32_t newAppFirmwareVersion,
                                                  const char * pcThingName,
                                                  FwUpdateStatus_t xStatus );

MQTTStatus_t HA_OTA_PublishFirmwareProgress( AppVersion32_t xInstalled,
                                             AppVersion32_t xLatest,
                                             const char * pcThingName,
                                             uint32_t ulProgress,
                                             uint32_t ulBlocksRemaining );

/* MQTT callback for <thing>/fw/update commands. */
void HA_OTA_HandleFwUpdateCommand( void * pxSubscriptionContext,
                                  MQTTPublishInfo_t * pPublishInfo );

void vOtaProgressTask( void * pvParameters );

#endif

#ifdef __cplusplus
}
#endif

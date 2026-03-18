#pragma once

#include "ha_helpers.h"
#include "sys_evt.h"
#include "ha_globals.h"

#ifdef __cplusplus
extern "C" {
#endif

void HA_Reset_PublishConfig( const char * pcThingName, char * pcPayloadBuffer );
void HA_Reset_HandleRebootCommand( void * pxSubscriptionContext, MQTTPublishInfo_t * pPublishInfo );

#ifdef __cplusplus
}
#endif

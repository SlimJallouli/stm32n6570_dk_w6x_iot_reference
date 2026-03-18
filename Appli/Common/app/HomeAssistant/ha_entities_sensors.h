#pragma once

#include "ha_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

void HA_EnvSensors_PublishConfig( const char * pcThingName, char * pcPayloadBuffer );
void HA_EnvSensors_ClearConfig( const char * pcThingName );

void HA_MotionSensors_PublishConfig( const char * pcThingName, char * pcPayloadBuffer );
void HA_MotionSensors_ClearConfig( const char * pcThingName );

void HA_RangingSensor_PublishConfig( const char * pcThingName, char * pcPayloadBuffer );
void HA_RangingSensor_ClearConfig( const char * pcThingName );

#ifdef __cplusplus
}
#endif

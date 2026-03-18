#pragma once

#include "ha_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

void HA_IMU_PublishConfig( const char * pcThingName, char * pcPayloadBuffer );
void HA_IMU_ClearConfig  ( const char * pcThingName );

#ifdef __cplusplus
}
#endif

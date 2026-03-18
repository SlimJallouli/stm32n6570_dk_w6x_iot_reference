#pragma once

#include "ha_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

void HA_LED_PublishConfig( const char * pcThingName, char * pcPayloadBuffer );
void HA_LED_ClearConfig( const char * pcThingName );

#ifdef __cplusplus
}
#endif

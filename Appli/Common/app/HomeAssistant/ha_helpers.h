#pragma once

#include "ha_config.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------*/
/* Initialization / plumbing */

void HAHelpers_SetMqttAgentHandle( MQTTAgentHandle_t xHandle );

/*-----------------------------------------------------------*/
/* Publish / subscribe helpers */

MQTTStatus_t HA_PublishToTopic( MQTTQoS_t xQoS,
                               bool xRetain,
                               const char *pcTopic,
                               const uint8_t *pucPayload,
                               size_t xPayloadLength );

MQTTStatus_t HA_ClearRetainedTopic( const char *pcTopic );

BaseType_t HA_SubscribeToThingTopic( const char *pcThingName,
                                     const char *pcTopicSuffix,
                                     IncomingPubCallback_t xCallback,
                                     const char *pcLogContext );

BaseType_t HA_SubscribeToTopic( const char * pcTopic,
                                IncomingPubCallback_t xCallback );

TickType_t HA_GetJitteredTimeoutTicks( void );

void HA_PublishAvailabilityStatus( const char *pcThingName,
                                   char *pcPayloadBuffer,
                                   const char *pcAvailability );

#ifdef __cplusplus
}
#endif

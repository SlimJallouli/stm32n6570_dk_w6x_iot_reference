/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : core_mqtt.c
  * @date           : Apr 23, 2025
  * @brief          : 
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/
#include "logging_levels.h"
/* define LOG_LEVEL here if you want to modify the logging level from the default */

#define LOG_LEVEL    LOG_INFO

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#include "core_mqtt_serializer.h"
#include "core_mqtt.h"

#include "w6x_api.h"
#include <string.h>

/* Private Macro -------------------------------------------------------------*/
//#define MQTT_PRE_SEND_HOOK( pContext )  xSemaphoreTake(xW6XMutex, portMAX_DELAY)
//#define MQTT_POST_SEND_HOOK( pContext ) xSemaphoreGive(xW6XMutex)
//#define MQTT_POST_SEND_HOOK( pContext ) vTaskDelay(1000)

#ifndef MQTT_PRE_SEND_HOOK

/**
 * @brief Hook called before a 'send' operation is executed.
 */
    #define MQTT_PRE_SEND_HOOK( pContext )
#endif /* !MQTT_PRE_SEND_HOOK */

#ifndef MQTT_POST_SEND_HOOK

/**
 * @brief Hook called after the 'send' operation is complete.
 */
    #define MQTT_POST_SEND_HOOK( pContext )
#endif /* !MQTT_POST_SEND_HOOK */

#ifndef MQTT_PRE_STATE_UPDATE_HOOK

/**
 * @brief Hook called just before an update to the MQTT state is made.
 */
    #define MQTT_PRE_STATE_UPDATE_HOOK( pContext )
#endif /* !MQTT_PRE_STATE_UPDATE_HOOK */

#ifndef MQTT_POST_STATE_UPDATE_HOOK

/**
 * @brief Hook called just after an update to the MQTT state has
 * been made.
 */
    #define MQTT_POST_STATE_UPDATE_HOOK( pContext )
#endif /* !MQTT_POST_STATE_UPDATE_HOOK */

/* Private Variables ---------------------------------------------------------*/
W6X_MQTT_Data_t *pxMQTTRecvData;

QueueHandle_t xSubMsgQueue = NULL;

static SemaphoreHandle_t xW6XMutex;
/* Private Function prototypes -----------------------------------------------*/

/**
 * @brief Performs matching for special cases when a topic filter ends
 * with a wildcard character.
 *
 * When the topic name has been consumed but there are remaining characters to
 * to match in topic filter, this function handles the following 2 cases:
 * - When the topic filter ends with "/+" or "/#" characters, but the topic
 * name only ends with '/'.
 * - When the topic filter ends with "/#" characters, but the topic name
 * ends at the parent level.
 *
 * @note This function ASSUMES that the topic name been consumed in linear
 * matching with the topic filer, but the topic filter has remaining characters
 * to be matched.
 *
 * @param[in] pTopicFilter The topic filter containing the wildcard.
 * @param[in] topicFilterLength Length of the topic filter being examined.
 * @param[in] filterIndex Index of the topic filter being examined.
 *
 * @return Returns whether the topic filter and the topic name match.
 */
static bool matchEndWildcardsSpecialCases( const char * pTopicFilter,
                                           uint16_t topicFilterLength,
                                           uint16_t filterIndex );

/**
 * @brief Attempt to match topic name with a topic filter starting with a wildcard.
 *
 * If the topic filter starts with a '+' (single-level) wildcard, the function
 * advances the @a pNameIndex by a level in the topic name.
 * If the topic filter starts with a '#' (multi-level) wildcard, the function
 * concludes that both the topic name and topic filter match.
 *
 * @param[in] pTopicName The topic name to match.
 * @param[in] topicNameLength Length of the topic name.
 * @param[in] pTopicFilter The topic filter to match.
 * @param[in] topicFilterLength Length of the topic filter.
 * @param[in,out] pNameIndex Current index in the topic name being examined. It is
 * advanced by one level for `+` wildcards.
 * @param[in, out] pFilterIndex Current index in the topic filter being examined.
 * It is advanced to position of '/' level separator for '+' wildcard.
 * @param[out] pMatch Whether the topic filter and topic name match.
 *
 * @return `true` if the caller of this function should exit; `false` if the
 * caller should continue parsing the topics.
 */
static bool matchWildcards( const char * pTopicName,
                            uint16_t topicNameLength,
                            const char * pTopicFilter,
                            uint16_t topicFilterLength,
                            uint16_t * pNameIndex,
                            uint16_t * pFilterIndex,
                            bool * pMatch );

/**
 * @brief Match a topic name and topic filter allowing the use of wildcards.
 *
 * @param[in] pTopicName The topic name to check.
 * @param[in] topicNameLength Length of the topic name.
 * @param[in] pTopicFilter The topic filter to check.
 * @param[in] topicFilterLength Length of topic filter.
 *
 * @return `true` if the topic name and topic filter match; `false` otherwise.
 */
static bool matchTopicFilter( const char * pTopicName,
                              uint16_t topicNameLength,
                              const char * pTopicFilter,
                              uint16_t topicFilterLength );

/**
 * @brief Validates parameters of #MQTT_Subscribe or #MQTT_Unsubscribe.
 *
 * @param[in] pContext Initialized MQTT context.
 * @param[in] pSubscriptionList List of MQTT subscription info.
 * @param[in] subscriptionCount The number of elements in pSubscriptionList.
 * @param[in] packetId Packet identifier.
 *
 * @return #MQTTBadParameter if invalid parameters are passed;
 * #MQTTSuccess otherwise.
 */
static MQTTStatus_t validateSubscribeUnsubscribeParams( const MQTTContext_t * pContext,
                                                        const MQTTSubscribeInfo_t * pSubscriptionList,
                                                        size_t subscriptionCount,
                                                        uint16_t packetId );

/*-----------------------------------------------------------*/

static bool matchEndWildcardsSpecialCases( const char * pTopicFilter,
                                           uint16_t topicFilterLength,
                                           uint16_t filterIndex )
{
    bool matchFound = false;

    assert( pTopicFilter != NULL );
    assert( topicFilterLength != 0U );

    /* Check if the topic filter has 2 remaining characters and it ends in
     * "/#". This check handles the case to match filter "sport/#" with topic
     * "sport". The reason is that the '#' wildcard represents the parent and
     * any number of child levels in the topic name.*/
    if( ( topicFilterLength >= 3U ) &&
        ( filterIndex == ( topicFilterLength - 3U ) ) &&
        ( pTopicFilter[ filterIndex + 1U ] == '/' ) &&
        ( pTopicFilter[ filterIndex + 2U ] == '#' ) )

    {
        matchFound = true;
    }

    /* Check if the next character is "#" or "+" and the topic filter ends in
     * "/#" or "/+". This check handles the cases to match:
     *
     * - Topic filter "sport/+" with topic "sport/".
     * - Topic filter "sport/#" with topic "sport/".
     */
    if( ( filterIndex == ( topicFilterLength - 2U ) ) &&
        ( pTopicFilter[ filterIndex ] == '/' ) )
    {
        /* Check that the last character is a wildcard. */
        matchFound = ( pTopicFilter[ filterIndex + 1U ] == '+' ) ||
                     ( pTopicFilter[ filterIndex + 1U ] == '#' );
    }

    return matchFound;
}

/*-----------------------------------------------------------*/

static bool matchWildcards( const char * pTopicName,
                            uint16_t topicNameLength,
                            const char * pTopicFilter,
                            uint16_t topicFilterLength,
                            uint16_t * pNameIndex,
                            uint16_t * pFilterIndex,
                            bool * pMatch )
{
    bool shouldStopMatching = false;
    bool locationIsValidForWildcard;

    assert( pTopicName != NULL );
    assert( topicNameLength != 0U );
    assert( pTopicFilter != NULL );
    assert( topicFilterLength != 0U );
    assert( pNameIndex != NULL );
    assert( pFilterIndex != NULL );
    assert( pMatch != NULL );

    /* Wild card in a topic filter is only valid either at the starting position
     * or when it is preceded by a '/'.*/
    locationIsValidForWildcard = ( *pFilterIndex == 0u ) ||
                                 ( pTopicFilter[ *pFilterIndex - 1U ] == '/' );

    if( ( pTopicFilter[ *pFilterIndex ] == '+' ) && ( locationIsValidForWildcard == true ) )
    {
        bool nextLevelExistsInTopicName = false;
        bool nextLevelExistsinTopicFilter = false;

        /* Move topic name index to the end of the current level. The end of the
         * current level is identified by the last character before the next level
         * separator '/'. */
        while( *pNameIndex < topicNameLength )
        {
            /* Exit the loop if we hit the level separator. */
            if( pTopicName[ *pNameIndex ] == '/' )
            {
                nextLevelExistsInTopicName = true;
                break;
            }

            ( *pNameIndex )++;
        }

        /* Determine if the topic filter contains a child level after the current level
         * represented by the '+' wildcard. */
        if( ( *pFilterIndex < ( topicFilterLength - 1U ) ) &&
            ( pTopicFilter[ *pFilterIndex + 1U ] == '/' ) )
        {
            nextLevelExistsinTopicFilter = true;
        }

        /* If the topic name contains a child level but the topic filter ends at
         * the current level, then there does not exist a match. */
        if( ( nextLevelExistsInTopicName == true ) &&
            ( nextLevelExistsinTopicFilter == false ) )
        {
            *pMatch = false;
            shouldStopMatching = true;
        }

        /* If the topic name and topic filter have child levels, then advance the
         * filter index to the level separator in the topic filter, so that match
         * can be performed in the next level.
         * Note: The name index already points to the level separator in the topic
         * name. */
        else if( nextLevelExistsInTopicName == true )
        {
            ( *pFilterIndex )++;
        }
        else
        {
            /* If we have reached here, the the loop terminated on the
             * ( *pNameIndex < topicNameLength) condition, which means that have
             * reached past the end of the topic name, and thus, we decrement the
             * index to the last character in the topic name.*/
            ( *pNameIndex )--;
        }
    }

    /* '#' matches everything remaining in the topic name. It must be the
     * last character in a topic filter. */
    else if( ( pTopicFilter[ *pFilterIndex ] == '#' ) &&
             ( *pFilterIndex == ( topicFilterLength - 1U ) ) &&
             ( locationIsValidForWildcard == true ) )
    {
        /* Subsequent characters don't need to be checked for the
         * multi-level wildcard. */
        *pMatch = true;
        shouldStopMatching = true;
    }
    else
    {
        /* Any character mismatch other than '+' or '#' means the topic
         * name does not match the topic filter. */
        *pMatch = false;
        shouldStopMatching = true;
    }

    return shouldStopMatching;
}

/*-----------------------------------------------------------*/

static bool matchTopicFilter( const char * pTopicName,
                              uint16_t topicNameLength,
                              const char * pTopicFilter,
                              uint16_t topicFilterLength )
{
    bool matchFound = false, shouldStopMatching = false;
    uint16_t nameIndex = 0, filterIndex = 0;

    assert( pTopicName != NULL );
    assert( topicNameLength != 0 );
    assert( pTopicFilter != NULL );
    assert( topicFilterLength != 0 );

    while( ( nameIndex < topicNameLength ) && ( filterIndex < topicFilterLength ) )
    {
        /* Check if the character in the topic name matches the corresponding
         * character in the topic filter string. */
        if( pTopicName[ nameIndex ] == pTopicFilter[ filterIndex ] )
        {
            /* If the topic name has been consumed but the topic filter has not
             * been consumed, match for special cases when the topic filter ends
             * with wildcard character. */
            if( nameIndex == ( topicNameLength - 1U ) )
            {
                matchFound = matchEndWildcardsSpecialCases( pTopicFilter,
                                                            topicFilterLength,
                                                            filterIndex );
            }
        }
        else
        {
            /* Check for matching wildcards. */
            shouldStopMatching = matchWildcards( pTopicName,
                                                 topicNameLength,
                                                 pTopicFilter,
                                                 topicFilterLength,
                                                 &nameIndex,
                                                 &filterIndex,
                                                 &matchFound );
        }

        if( ( matchFound == true ) || ( shouldStopMatching == true ) )
        {
            break;
        }

        /* Increment indexes. */
        nameIndex++;
        filterIndex++;
    }

    if( matchFound == false )
    {
        /* If the end of both strings has been reached, they match. This represents the
         * case when the topic filter contains the '+' wildcard at a non-starting position.
         * For example, when matching either of "sport/+/player" OR "sport/hockey/+" topic
         * filters with "sport/hockey/player" topic name. */
        matchFound = ( nameIndex == topicNameLength ) &&
                     ( filterIndex == topicFilterLength );
    }

    return matchFound;
}

/*-----------------------------------------------------------*/

/**
 * @brief prvIncomingPublishCallback
 * @param pvParameters Pointer to parameters (unused in this task)
 * @retval None
 */
static void prvIncomingPublishCallback(void *pvParameters)
{
  /* Variable declarations */
  MQTTPublishInfo_t mqtt_data;
  MQTTContext_t * pContext = (MQTTContext_t *)pvParameters;
  MQTTDeserializedInfo_t deserializedInfo;
  MQTTPacketInfo_t IncomingPacket;

  /* Log task initialization */
  LogInfo("Task '%s' started.", __func__);

  /* Main processing loop */
  for (;;)
  {
    int32_t xStatus = pdTRUE;

    xStatus = xQueueReceive(xSubMsgQueue, &mqtt_data, portMAX_DELAY);

    if (xStatus == pdTRUE)
    {
      deserializedInfo.packetIdentifier      = 0;
      deserializedInfo.deserializationResult = MQTTSuccess;
      deserializedInfo.pPublishInfo          = &mqtt_data;

      IncomingPacket.headerLength    = sizeof(MQTTPacketInfo_t);
      IncomingPacket.pRemainingData  = NULL;
      IncomingPacket.remainingLength = 0;
      IncomingPacket.type            = MQTT_PACKET_TYPE_PUBLISH;

      pContext->appCallback( pContext, &IncomingPacket, &deserializedInfo );

      /* Free the allocated topic and message */
      vPortFree((char * )mqtt_data.pTopicName);
      vPortFree((char * )mqtt_data.pPayload);
    }
  }

  /* Suspend the task (unlikely to reach here) */
  vTaskSuspend(NULL);
}

/*-----------------------------------------------------------*/
#if 0
static MQTTStatus_t handleIncomingPublish( MQTTContext_t * pContext,
                                           MQTTPacketInfo_t * pIncomingPacket )
{
    MQTTStatus_t status = MQTTBadParameter;

    MQTTPublishState_t publishRecordState = MQTTStateNull;
    uint16_t packetIdentifier = 0U;
    MQTTPublishInfo_t publishInfo;
    MQTTDeserializedInfo_t deserializedInfo;
    bool duplicatePublish = false;

    assert( pContext != NULL );
    assert( pIncomingPacket != NULL );
    assert( pContext->appCallback != NULL );

    status = MQTT_DeserializePublish( pIncomingPacket, &packetIdentifier, &publishInfo );
    LogInfo( ( "De-serialized incoming PUBLISH packet: DeserializerResult=%s.",
               MQTT_Status_strerror( status ) ) );

    if( ( status == MQTTSuccess ) &&
        ( pContext->incomingPublishRecords == NULL ) &&
        ( publishInfo.qos > MQTTQoS0 ) )
    {
        LogError( ( "Incoming publish has QoS > MQTTQoS0 but incoming "
                    "publish records have not been initialized. Dropping the "
                    "incoming publish. Please call MQTT_InitStatefulQoS to enable "
                    "use of QoS1 and QoS2 publishes." ) );
        status = MQTTRecvFailed;
    }

    if( status == MQTTSuccess )
    {
        MQTT_PRE_STATE_UPDATE_HOOK( pContext );

        status = MQTT_UpdateStatePublish( pContext,
                                          packetIdentifier,
                                          MQTT_RECEIVE,
                                          publishInfo.qos,
                                          &publishRecordState );

        MQTT_POST_STATE_UPDATE_HOOK( pContext );

        if( status == MQTTSuccess )
        {
            LogInfo( ( "State record updated. New state=%s.",
                       MQTT_State_strerror( publishRecordState ) ) );
        }

        /* Different cases in which an incoming publish with duplicate flag is
         * handled are as listed below.
         * 1. No collision - This is the first instance of the incoming publish
         *    packet received or an earlier received packet state is lost. This
         *    will be handled as a new incoming publish for both QoS1 and QoS2
         *    publishes.
         * 2. Collision - The incoming packet was received before and a state
         *    record is present in the state engine. For QoS1 and QoS2 publishes
         *    this case can happen at 2 different cases and handling is
         *    different.
         *    a. QoS1 - If a PUBACK is not successfully sent for the incoming
         *       publish due to a connection issue, it can result in broker
         *       sending out a duplicate publish with dup flag set, when a
         *       session is reestablished. It can result in a collision in
         *       state engine. This will be handled by processing the incoming
         *       publish as a new publish ignoring the
         *       #MQTTStateCollision status from the state engine. The publish
         *       data is not passed to the application.
         *    b. QoS2 - If a PUBREC is not successfully sent for the incoming
         *       publish or the PUBREC sent is not successfully received by the
         *       broker due to a connection issue, it can result in broker
         *       sending out a duplicate publish with dup flag set, when a
         *       session is reestablished. It can result in a collision in
         *       state engine. This will be handled by ignoring the
         *       #MQTTStateCollision status from the state engine. The publish
         *       data is not passed to the application. */
        else if( status == MQTTStateCollision )
        {
            status = MQTTSuccess;
            duplicatePublish = true;

            /* Calculate the state for the ack packet that needs to be sent out
             * for the duplicate incoming publish. */
            publishRecordState = MQTT_CalculateStatePublish( MQTT_RECEIVE,
                                                             publishInfo.qos );

            LogDebug( ( "Incoming publish packet with packet id %hu already exists.",
                        ( unsigned short ) packetIdentifier ) );

            if( publishInfo.dup == false )
            {
                LogError( ( "DUP flag is 0 for duplicate packet (MQTT-3.3.1.-1)." ) );
            }
        }
        else
        {
            LogError( ( "Error in updating publish state for incoming publish with packet id %hu."
                        " Error is %s",
                        ( unsigned short ) packetIdentifier,
                        MQTT_Status_strerror( status ) ) );
        }
    }

    if( status == MQTTSuccess )
    {
        /* Set fields of deserialized struct. */
        deserializedInfo.packetIdentifier = packetIdentifier;
        deserializedInfo.pPublishInfo = &publishInfo;
        deserializedInfo.deserializationResult = status;

        /* Invoke application callback to hand the buffer over to application
         * before sending acks.
         * Application callback will be invoked for all publishes, except for
         * duplicate incoming publishes. */
        if( duplicatePublish == false )
        {
            pContext->appCallback( pContext,
                                   pIncomingPacket,
                                   &deserializedInfo );
        }

        /* Send PUBACK or PUBREC if necessary. */
        status = sendPublishAcks( pContext,
                                  packetIdentifier,
                                  publishRecordState );
    }

    return status;
}
#endif
/*-----------------------------------------------------------*/

static MQTTStatus_t receiveSingleIteration( MQTTContext_t * pContext,
                                            bool manageKeepAlive )
{
    MQTTStatus_t status = MQTTSuccess;
#if 0
    MQTTPacketInfo_t incomingPacket = { 0 };
    int32_t recvBytes;
    size_t totalMQTTPacketLength = 0;

    assert( pContext != NULL );
    assert( pContext->networkBuffer.pBuffer != NULL );

    /* Read as many bytes as possible into the network buffer. */
    recvBytes = pContext->transportInterface.recv( pContext->transportInterface.pNetworkContext,
                                                   &( pContext->networkBuffer.pBuffer[ pContext->index ] ),
                                                   pContext->networkBuffer.size - pContext->index );

    if( recvBytes < 0 )
    {
        /* The receive function has failed. Bubble up the error up to the user. */
        status = MQTTRecvFailed;
    }
    else if( ( recvBytes == 0 ) && ( pContext->index == 0U ) )
    {
        /* No more bytes available since the last read and neither is anything in
         * the buffer. */
        status = MQTTNoDataAvailable;
    }

    /* Either something was received, or there is still data to be processed in the
     * buffer, or both. */
    else
    {
        /* Update the number of bytes in the MQTT fixed buffer. */
        pContext->index += ( size_t ) recvBytes;

        status = MQTT_ProcessIncomingPacketTypeAndLength( pContext->networkBuffer.pBuffer,
                                                          &pContext->index,
                                                          &incomingPacket );

        totalMQTTPacketLength = incomingPacket.remainingLength + incomingPacket.headerLength;
    }

    /* No data was received, check for keep alive timeout. */
    if( recvBytes == 0 )
    {
        if( manageKeepAlive == true )
        {
            /* Keep the copy of the status to be reset later. */
            MQTTStatus_t statusCopy = status;

            /* Assign status so an error can be bubbled up to application,
             * but reset it on success. */
            status = handleKeepAlive( pContext );

            if( status == MQTTSuccess )
            {
                /* Reset the status. */
                status = statusCopy;
            }
            else
            {
                LogError( ( "Handling of keep alive failed. Status=%s",
                            MQTT_Status_strerror( status ) ) );
            }
        }
    }

    /* Check whether there is data available before processing the packet further. */
    if( ( status == MQTTNeedMoreBytes ) || ( status == MQTTNoDataAvailable ) )
    {
        /* Do nothing as there is nothing to be processed right now. The proper
         * error code will be bubbled up to the user. */
    }
    /* Any other error code. */
    else if( status != MQTTSuccess )
    {
        LogError( ( "Call to receiveSingleIteration failed. Status=%s",
                    MQTT_Status_strerror( status ) ) );
    }
    /* If the MQTT Packet size is bigger than the buffer itself. */
    else if( totalMQTTPacketLength > pContext->networkBuffer.size )
    {
        /* Discard the packet from the receive buffer and drain the pending
         * data from the socket buffer. */
        status = discardStoredPacket( pContext,
                                      &incomingPacket );
    }
    /* If the total packet is of more length than the bytes we have available. */
    else if( totalMQTTPacketLength > pContext->index )
    {
        status = MQTTNeedMoreBytes;
    }
    else
    {
        /* MISRA else. */
    }

    /* Handle received packet. If incomplete data was read then this will not execute. */
    if( status == MQTTSuccess )
    {
        incomingPacket.pRemainingData = &pContext->networkBuffer.pBuffer[ incomingPacket.headerLength ];

        /* PUBLISH packets allow flags in the lower four bits. For other
         * packet types, they are reserved. */
        if( ( incomingPacket.type & 0xF0U ) == MQTT_PACKET_TYPE_PUBLISH )
        {
            status = handleIncomingPublish( pContext, &incomingPacket );
        }
        else
        {
            status = handleIncomingAck( pContext, &incomingPacket, manageKeepAlive );
        }

        /* Update the index to reflect the remaining bytes in the buffer.  */
        pContext->index -= totalMQTTPacketLength;

        /* Move the remaining bytes to the front of the buffer. */
        ( void ) memmove( pContext->networkBuffer.pBuffer,
                          &( pContext->networkBuffer.pBuffer[ totalMQTTPacketLength ] ),
                          pContext->index );
    }

    if( status == MQTTNoDataAvailable )
    {
        /* No data available is not an error. Reset to MQTTSuccess so the
         * return code will indicate success. */
        status = MQTTSuccess;
    }
#endif
    return status;
}

/*-----------------------------------------------------------*/

static MQTTStatus_t validateSubscribeUnsubscribeParams( const MQTTContext_t * pContext,
                                                        const MQTTSubscribeInfo_t * pSubscriptionList,
                                                        size_t subscriptionCount,
                                                        uint16_t packetId )
{
    MQTTStatus_t status = MQTTSuccess;
    size_t iterator;

    /* Validate all the parameters. */
    if( ( pContext == NULL ) || ( pSubscriptionList == NULL ) )
    {
        LogError( ( "Argument cannot be NULL: pContext=%p, "
                    "pSubscriptionList=%p.",
                    ( void * ) pContext,
                    ( void * ) pSubscriptionList ) );
        status = MQTTBadParameter;
    }
    else if( subscriptionCount == 0UL )
    {
        LogError( ( "Subscription count is 0." ) );
        status = MQTTBadParameter;
    }
#if 0
    else if( packetId == 0U )
    {
        LogError( ( "Packet Id for subscription packet is 0." ) );
        status = MQTTBadParameter;
    }
#endif
    else
    {
        if( pContext->incomingPublishRecords == NULL )
        {
            for( iterator = 0; iterator < subscriptionCount; iterator++ )
            {
                if( pSubscriptionList->qos > MQTTQoS0 )
                {
                    LogError( ( "The incoming publish record list is not "
                                "initialised for QoS1/QoS2 records. Please call "
                                " MQTT_InitStatefulQoS to enable use of QoS1 and "
                                " QoS2 packets." ) );
                    status = MQTTBadParameter;
                    break;
                }
            }
        }
    }

    return status;
}

/* User code -----------------------------------------------------------------*/
MQTTStatus_t MQTT_Init( MQTTContext_t * pContext,
                        const TransportInterface_t * pTransportInterface,
                        MQTTGetCurrentTimeFunc_t getTimeFunction,
                        MQTTEventCallback_t userCallback,
                        const MQTTFixedBuffer_t * pNetworkBuffer )
{
  MQTTStatus_t status = MQTTSuccess;

  /* Validate arguments. */
  if( ( pContext == NULL ) || ( pTransportInterface == NULL ) ||
      ( pNetworkBuffer == NULL ) )
  {
      LogError( ( "Argument cannot be NULL: pContext=%p, "
                  "pTransportInterface=%p, "
                  "pNetworkBuffer=%p",
                  ( void * ) pContext,
                  ( void * ) pTransportInterface,
                  ( void * ) pNetworkBuffer ) );
      status = MQTTBadParameter;
  }
  else if( getTimeFunction == NULL )
  {
      LogError( ( "Invalid parameter: getTimeFunction is NULL" ) );
      status = MQTTBadParameter;
  }
  else if( userCallback == NULL )
  {
      LogError( ( "Invalid parameter: userCallback is NULL" ) );
      status = MQTTBadParameter;
  }
#if !defined(ST67W6X)
  else if( pTransportInterface->recv == NULL )
  {
      LogError( ( "Invalid parameter: pTransportInterface->recv is NULL" ) );
      status = MQTTBadParameter;
  }
  else if( pTransportInterface->send == NULL )
  {
      LogError( ( "Invalid parameter: pTransportInterface->send is NULL" ) );
      status = MQTTBadParameter;
  }
#endif
  else
  {
      ( void ) memset( pContext, 0x00, sizeof( MQTTContext_t ) );

      pContext->connectStatus = MQTTNotConnected;
      pContext->transportInterface = *pTransportInterface;
      pContext->getTime = getTimeFunction;
      pContext->appCallback = userCallback;
      pContext->networkBuffer = *pNetworkBuffer;

      pxMQTTRecvData = (W6X_MQTT_Data_t *)pNetworkBuffer;

      /* Zero is not a valid packet ID per MQTT spec. Start from 1. */
      pContext->nextPacketId = 1;

      if(W6X_MQTT_Init((W6X_MQTT_Data_t *)pNetworkBuffer) != W6X_STATUS_OK)
      {
        status = MQTTBadParameter;
      }

      /* Create W6X Mutex */
      xW6XMutex = xSemaphoreCreateMutex();

      /* Create a queue for subscribed messages */
      xSubMsgQueue = xQueueCreate(10, sizeof(MQTTPublishInfo_t));

      if (xSubMsgQueue == NULL)
      {
        LogError("Failed to create message queue.");
        vTaskSuspend(NULL);
      }

      xTaskCreate(prvIncomingPublishCallback, "mqtt_sub", TASK_STACK_SIZE_SUBSCRIPTION, pContext, TASK_PRIO_SUBSCRIPTION, NULL);
  }

  return status;
}

/*-----------------------------------------------------------*/

MQTTStatus_t MQTT_InitStatefulQoS( MQTTContext_t * pContext,
                                   MQTTPubAckInfo_t * pOutgoingPublishRecords,
                                   size_t outgoingPublishCount,
                                   MQTTPubAckInfo_t * pIncomingPublishRecords,
                                   size_t incomingPublishCount )
{
    MQTTStatus_t status = MQTTSuccess;

    if( pContext == NULL )
    {
        LogError( ( "Argument cannot be NULL: pContext=%p\n",
                    ( void * ) pContext ) );
        status = MQTTBadParameter;
    }

    /* Check whether the arguments make sense. Not equal here behaves
     * like an exclusive-or operator for boolean values. */
    else if( ( outgoingPublishCount == 0U ) !=
             ( pOutgoingPublishRecords == NULL ) )
    {
        LogError( ( "Arguments do not match: pOutgoingPublishRecords=%p, "
                    "outgoingPublishCount=%lu",
                    ( void * ) pOutgoingPublishRecords,
                    outgoingPublishCount ) );
        status = MQTTBadParameter;
    }

    /* Check whether the arguments make sense. Not equal here behaves
     * like an exclusive-or operator for boolean values. */
    else if( ( incomingPublishCount == 0U ) !=
             ( pIncomingPublishRecords == NULL ) )
    {
        LogError( ( "Arguments do not match: pIncomingPublishRecords=%p, "
                    "incomingPublishCount=%lu",
                    ( void * ) pIncomingPublishRecords,
                    incomingPublishCount ) );
        status = MQTTBadParameter;
    }
    else if( pContext->appCallback == NULL )
    {
        LogError( ( "MQTT_InitStatefulQoS must be called only after MQTT_Init has"
                    " been called succesfully.\n" ) );
        status = MQTTBadParameter;
    }
    else
    {
        pContext->incomingPublishRecordMaxCount = incomingPublishCount;
        pContext->incomingPublishRecords = pIncomingPublishRecords;
        pContext->outgoingPublishRecordMaxCount = outgoingPublishCount;
        pContext->outgoingPublishRecords = pOutgoingPublishRecords;
    }

    return status;
}

/*-----------------------------------------------------------*/

MQTTStatus_t MQTT_CancelCallback( const MQTTContext_t * pContext,
                                  uint16_t packetId )
{
    MQTTStatus_t status = MQTTSuccess;
#if 0
    if( pContext == NULL )
    {
        LogWarn( ( "pContext is NULL\n" ) );
        status = MQTTBadParameter;
    }
    else if( pContext->outgoingPublishRecords == NULL )
    {
        LogError( ( "QoS1/QoS2 is not initialized for use. Please, "
                    "call MQTT_InitStatefulQoS to enable QoS1 and QoS2 "
                    "publishes.\n" ) );
        status = MQTTBadParameter;
    }
    else
    {
        MQTT_PRE_STATE_UPDATE_HOOK( pContext );

        status = MQTT_RemoveStateRecord( pContext,
                                         packetId );

        MQTT_POST_STATE_UPDATE_HOOK( pContext );
    }
#endif
    return status;
}

/*-----------------------------------------------------------*/

MQTTStatus_t MQTT_Connect( MQTTContext_t * pContext,
                           const MQTTConnectInfo_t * pConnectInfo,
                           const MQTTPublishInfo_t * pWillInfo,
                           uint32_t timeoutMs,
                           bool * pSessionPresent )
{
  W6X_Status_t xStatus = W6X_STATUS_OK;
  MQTTStatus_t xMQTTStatus = MQTTSuccess;

  W6X_MQTT_Connect_t *pxCtx;

  pxCtx = (W6X_MQTT_Connect_t *)pContext->transportInterface.pNetworkContext;

  /* Take the mutex because the below call should not be interrupted. */
  MQTT_PRE_SEND_HOOK( pContext );

  xStatus = W6X_MQTT_Configure(pxCtx);

  if (xStatus == W6X_STATUS_OK)
  {
    xStatus = W6X_MQTT_Connect(pxCtx);
  }

  if (xStatus != W6X_STATUS_OK)
  {
    xMQTTStatus = MQTTBadParameter;
  }

  /* Give the mutex away. */
  MQTT_POST_SEND_HOOK( pContext );

  return xMQTTStatus;
}

/*-----------------------------------------------------------*/

MQTTStatus_t MQTT_Subscribe( MQTTContext_t * pContext,
                             const MQTTSubscribeInfo_t * pSubscriptionList,
                             size_t subscriptionCount,
                             uint16_t packetId )
{
  W6X_Status_t xStatus = W6X_STATUS_OK;
  MQTTStatus_t xMQTTStatus = MQTTSuccess;

  size_t remainingLength = 0UL, packetSize = 0UL;

  /* Validate arguments. */
  xMQTTStatus = validateSubscribeUnsubscribeParams( pContext,
                                                            pSubscriptionList,
                                                            subscriptionCount,
                                                            packetId );

  if( xMQTTStatus == MQTTSuccess )
  {
      /* Get the remaining length and packet size.*/
    xMQTTStatus = MQTT_GetSubscribePacketSize( pSubscriptionList,
                                            subscriptionCount,
                                            &remainingLength,
                                            &packetSize );
      LogDebug( ( "SUBSCRIBE packet size is %lu and remaining length is %lu.",
                  ( unsigned long ) packetSize,
                  ( unsigned long ) remainingLength ) );
  }

  LogInfo("Subscribing to topic           =\"%s\"", pSubscriptionList->pTopicFilter);

  if (xMQTTStatus == MQTTSuccess)
  {
    /* Take the mutex because the below call should not be interrupted. */
    MQTT_PRE_SEND_HOOK( pContext );

    /* Send MQTT SUBSCRIBE packet. */
    xStatus = W6X_MQTT_Subscribe((uint8_t*) pSubscriptionList->pTopicFilter);

    if (xStatus != W6X_STATUS_OK)
    {
      xMQTTStatus = MQTTBadParameter;
    }

    /* Give the mutex away. */
    MQTT_POST_SEND_HOOK( pContext );
  }

  return xMQTTStatus;
}

/*-----------------------------------------------------------*/

MQTTStatus_t MQTT_Publish( MQTTContext_t * pContext, const MQTTPublishInfo_t * pPublishInfo, uint16_t packetId )
{
  W6X_Status_t xStatus = W6X_STATUS_OK;
  MQTTStatus_t xMQTTStatus = MQTTSuccess;

  /* Take the mutex because the below call should not be interrupted. */
  MQTT_PRE_SEND_HOOK( pContext );

//  LogDebug( ( "Topic size    : %d\n", pPublishInfo->topicNameLength ) );
  LogDebug( ( "Publishing to : %.*s\n", ( int ) pPublishInfo->topicNameLength, pPublishInfo->pTopicName ) );
//  LogDebug( ( "Message size  : %d.\n", pPublishInfo->payloadLength ) );
  LogDebug( ( "Message       : %.*s\n", ( int ) pPublishInfo->payloadLength, pPublishInfo->pPayload ) );

  xStatus = W6X_MQTT_Publish((uint8_t *)pPublishInfo->pTopicName, (uint8_t*) pPublishInfo->pPayload, pPublishInfo->payloadLength);

  if (xStatus != W6X_STATUS_OK)
  {
    xMQTTStatus = MQTTBadParameter;
  }

  /* Give the mutex away. */
  MQTT_POST_SEND_HOOK( pContext );

  return xMQTTStatus;
}

/*-----------------------------------------------------------*/

MQTTStatus_t MQTT_Ping(MQTTContext_t *pContext)
{
  MQTTStatus_t status = MQTTSuccess;

  return status;
}

/*-----------------------------------------------------------*/

MQTTStatus_t MQTT_Unsubscribe( MQTTContext_t * pContext,
                               const MQTTSubscribeInfo_t * pSubscriptionList,
                               size_t subscriptionCount,
                               uint16_t packetId )
{
  W6X_Status_t xStatus = W6X_STATUS_OK;
  MQTTStatus_t xMQTTStatus = MQTTBadParameter;

  size_t remainingLength = 0UL, packetSize = 0UL;

  /* Validate arguments. */
  xMQTTStatus = validateSubscribeUnsubscribeParams( pContext,
                                                            pSubscriptionList,
                                                            subscriptionCount,
                                                            packetId );

  if( xMQTTStatus == MQTTSuccess )
  {
      /* Get the remaining length and packet size.*/
    xMQTTStatus = MQTT_GetUnsubscribePacketSize( pSubscriptionList,
                                              subscriptionCount,
                                              &remainingLength,
                                              &packetSize );
      LogDebug( ( "UNSUBSCRIBE packet size is %lu and remaining length is %lu.",
                  ( unsigned long ) packetSize,
                  ( unsigned long ) remainingLength ) );
  }

  if (xMQTTStatus == MQTTSuccess)
  {
    /* Take the mutex because the below call should not be interrupted. */
    MQTT_PRE_SEND_HOOK( pContext );

    xStatus = W6X_MQTT_Unsubscribe((uint8_t *)pSubscriptionList->pTopicFilter);

    if (xStatus == W6X_STATUS_OK)
    {
      xMQTTStatus = MQTTSuccess;
    }

    /* Give the mutex away. */
    MQTT_POST_SEND_HOOK( pContext );
  }

  return xMQTTStatus;
}

/*-----------------------------------------------------------*/

MQTTStatus_t MQTT_Disconnect( MQTTContext_t * pContext )
{
  MQTTStatus_t xStatus = MQTTSuccess;
  W6X_Status_t Status;

  /* Take the mutex because the below call should not be interrupted. */
  MQTT_PRE_SEND_HOOK( pContext );

  Status = W6X_MQTT_Disconnect();

  if (Status != W6X_STATUS_OK)
  {
    xStatus = MQTTBadParameter;
    LogError("MQTT Disconnect failed.");
  }

  /* Give the mutex away. */
  MQTT_POST_SEND_HOOK( pContext );

  return xStatus;
}

/*-----------------------------------------------------------*/

MQTTStatus_t MQTT_ProcessLoop( MQTTContext_t * pContext )
{
    MQTTStatus_t status = MQTTBadParameter;

    if( pContext == NULL )
    {
        LogError( ( "Invalid input parameter: MQTT Context cannot be NULL." ) );
    }
    else if( pContext->getTime == NULL )
    {
        LogError( ( "Invalid input parameter: MQTT Context must have valid getTime." ) );
    }
    else if( pContext->networkBuffer.pBuffer == NULL )
    {
        LogError( ( "Invalid input parameter: The MQTT context's networkBuffer must not be NULL." ) );
    }
    else
    {
#if 1
        pContext->controlPacketSent = false;
        status = receiveSingleIteration( pContext, true );
#endif
    }

    return status;
}

/*-----------------------------------------------------------*/

uint16_t MQTT_GetPacketId( MQTTContext_t * pContext )
{
    uint16_t packetId = 0U;
#if 0
    if( pContext != NULL )
    {
        MQTT_PRE_STATE_UPDATE_HOOK( pContext );

        packetId = pContext->nextPacketId;

        /* A packet ID of zero is not a valid packet ID. When the max ID
         * is reached the next one should start at 1. */
        if( pContext->nextPacketId == ( uint16_t ) UINT16_MAX )
        {
            pContext->nextPacketId = 1;
        }
        else
        {
            pContext->nextPacketId++;
        }

        MQTT_POST_STATE_UPDATE_HOOK( pContext );
    }
#endif
    return packetId;
}

/*-----------------------------------------------------------*/

MQTTStatus_t MQTT_MatchTopic( const char * pTopicName,
                              const uint16_t topicNameLength,
                              const char * pTopicFilter,
                              const uint16_t topicFilterLength,
                              bool * pIsMatch )
{
    MQTTStatus_t status = MQTTSuccess;
#if 1
    bool topicFilterStartsWithWildcard = false;
    bool matchStatus = false;

    if( ( pTopicName == NULL ) || ( topicNameLength == 0u ) )
    {
        LogError( ( "Invalid paramater: Topic name should be non-NULL and its "
                    "length should be > 0: TopicName=%p, TopicNameLength=%hu",
                    ( void * ) pTopicName,
                    ( unsigned short ) topicNameLength ) );

        status = MQTTBadParameter;
    }
    else if( ( pTopicFilter == NULL ) || ( topicFilterLength == 0u ) )
    {
        LogError( ( "Invalid paramater: Topic filter should be non-NULL and "
                    "its length should be > 0: TopicName=%p, TopicFilterLength=%hu",
                    ( void * ) pTopicFilter,
                    ( unsigned short ) topicFilterLength ) );
        status = MQTTBadParameter;
    }
    else if( pIsMatch == NULL )
    {
        LogError( ( "Invalid paramater: Output parameter, pIsMatch, is NULL" ) );
        status = MQTTBadParameter;
    }
    else
    {
        /* Check for an exact match if the incoming topic name and the registered
         * topic filter length match. */
        if( topicNameLength == topicFilterLength )
        {
            matchStatus = strncmp( pTopicName, pTopicFilter, topicNameLength ) == 0;
        }

        if( matchStatus == false )
        {
            /* If an exact match was not found, match against wildcard characters in
             * topic filter.*/

            /* Determine if topic filter starts with a wildcard. */
            topicFilterStartsWithWildcard = ( pTopicFilter[ 0 ] == '+' ) ||
                                            ( pTopicFilter[ 0 ] == '#' );

            /* Note: According to the MQTT 3.1.1 specification, incoming PUBLISH topic names
             * starting with "$" character cannot be matched against topic filter starting with
             * a wildcard, i.e. for example, "$SYS/sport" cannot be matched with "#" or
             * "+/sport" topic filters. */
            if( !( ( pTopicName[ 0 ] == '$' ) && ( topicFilterStartsWithWildcard == true ) ) )
            {
                matchStatus = matchTopicFilter( pTopicName, topicNameLength, pTopicFilter, topicFilterLength );
            }
        }

        /* Update the output parameter with the match result. */
        *pIsMatch = matchStatus;
    }
#endif
    return status;
}

/*-----------------------------------------------------------*/

MQTTStatus_t MQTT_GetSubAckStatusCodes( const MQTTPacketInfo_t * pSubackPacket,
                                        uint8_t ** pPayloadStart,
                                        size_t * pPayloadSize )
{
    MQTTStatus_t status = MQTTSuccess;

    if( pSubackPacket == NULL )
    {
        LogError( ( "Invalid parameter: pSubackPacket is NULL." ) );
        status = MQTTBadParameter;
    }
    else if( pPayloadStart == NULL )
    {
        LogError( ( "Invalid parameter: pPayloadStart is NULL." ) );
        status = MQTTBadParameter;
    }
    else if( pPayloadSize == NULL )
    {
        LogError( ( "Invalid parameter: pPayloadSize is NULL." ) );
        status = MQTTBadParameter;
    }
    else if( pSubackPacket->type != MQTT_PACKET_TYPE_SUBACK )
    {
        LogError( ( "Invalid parameter: Input packet is not a SUBACK packet: "
                    "ExpectedType=%02x, InputType=%02x",
                    ( int ) MQTT_PACKET_TYPE_SUBACK,
                    ( int ) pSubackPacket->type ) );
        status = MQTTBadParameter;
    }
    else if( pSubackPacket->pRemainingData == NULL )
    {
        LogError( ( "Invalid parameter: pSubackPacket->pRemainingData is NULL" ) );
        status = MQTTBadParameter;
    }

    /* A SUBACK must have a remaining length of at least 3 to accommodate the
     * packet identifier and at least 1 return code. */
    else if( pSubackPacket->remainingLength < 3U )
    {
        LogError( ( "Invalid parameter: Packet remaining length is invalid: "
                    "Should be greater than 2 for SUBACK packet: InputRemainingLength=%lu",
                    ( unsigned long ) pSubackPacket->remainingLength ) );
        status = MQTTBadParameter;
    }
    else
    {
        /* According to the MQTT 3.1.1 protocol specification, the "Remaining Length" field is a
         * length of the variable header (2 bytes) plus the length of the payload.
         * Therefore, we add 2 positions for the starting address of the payload, and
         * subtract 2 bytes from the remaining length for the length of the payload.*/
        *pPayloadStart = &pSubackPacket->pRemainingData[ sizeof( uint16_t ) ];
        *pPayloadSize = pSubackPacket->remainingLength - sizeof( uint16_t );
    }

    return status;
}

/*-----------------------------------------------------------*/

const char * MQTT_Status_strerror( MQTTStatus_t status )
{
    const char * str = NULL;

    switch( status )
    {
        case MQTTSuccess:
            str = "MQTTSuccess";
            break;

        case MQTTBadParameter:
            str = "MQTTBadParameter";
            break;

        case MQTTNoMemory:
            str = "MQTTNoMemory";
            break;

        case MQTTSendFailed:
            str = "MQTTSendFailed";
            break;

        case MQTTRecvFailed:
            str = "MQTTRecvFailed";
            break;

        case MQTTBadResponse:
            str = "MQTTBadResponse";
            break;

        case MQTTServerRefused:
            str = "MQTTServerRefused";
            break;

        case MQTTNoDataAvailable:
            str = "MQTTNoDataAvailable";
            break;

        case MQTTIllegalState:
            str = "MQTTIllegalState";
            break;

        case MQTTStateCollision:
            str = "MQTTStateCollision";
            break;

        case MQTTKeepAliveTimeout:
            str = "MQTTKeepAliveTimeout";
            break;

        default:
            str = "Invalid MQTT Status code";
            break;
    }

    return str;
}

/*-----------------------------------------------------------*/

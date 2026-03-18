/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/*
 * Demo for showing use of the Fleet Provisioning library to use the Fleet
 * Provisioning feature of AWS IoT Core for provisioning devices with
 * credentials. This demo shows how a device can be provisioned with AWS IoT
 * Core using the Certificate Signing Request workflow of the Fleet
 * Provisioning feature.
 *
 * The Fleet Provisioning library provides macros and helper functions for
 * assembling MQTT topics strings, and for determining whether an incoming MQTT
 * message is related to the Fleet Provisioning API of AWS IoT Core. The Fleet
 * Provisioning library does not depend on any particular MQTT library,
 * therefore the functionality for MQTT operations is placed in another file
 * (mqtt_operations.c). This demo uses the coreMQTT library. If needed,
 * mqtt_operations.c can be modified to replace coreMQTT with another MQTT
 * library. This demo requires using the AWS IoT Core broker as Fleet
 * Provisioning is an AWS IoT Core feature.
 *
 * This demo provisions a device certificate using the provisioning by claim
 * workflow with a Certificate Signing Request (CSR). The demo connects to AWS
 * IoT Core using provided claim credentials (whose certificate needs to be
 * registered with IoT Core before running this demo), subscribes to the
 * CreateCertificateFromCsr topics, and obtains a certificate. It then
 * subscribes to the RegisterThing topics and activates the certificate and
 * obtains a Thing using the provisioning template. Finally, it reconnects to
 * AWS IoT Core using the new credentials.
 */

/* Standard includes. */
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "logging_levels.h"

/* Logging configuration for the Fleet Provisioning library. */
#ifndef LIBRARY_LOG_NAME
#define LIBRARY_LOG_NAME    "FleetProvisioning"
#endif

#ifndef LIBRARY_LOG_LEVEL
#define LIBRARY_LOG_LEVEL    LOG_INFO
#endif

/* Kernel includes. */
#include "FreeRTOS.h"
#if defined(DEMO_AWS_FLEET_PROVISION) && !defined(__USE_STSAFE__)

#include "task.h"

/* mbedTLS include for configuring threading functions */
#include "mbedtls/threading.h"
#include "threading_alt.h"

/* TinyCBOR library for CBOR encoding and decoding operations. */
#include "cbor.h"

/* corePKCS11 includes. */
#include "core_pkcs11.h"
#include "core_pkcs11_config.h"
#include "pkcs11_operations.h"

/* AWS IoT Fleet Provisioning Library. */
#include "fleet_provisioning.h"

/* tinycbor_serializer */
#include "tinycbor_serializer.h"

/* MQTT library includes. */
#include "core_mqtt.h"

/* MQTT agent include. */
#include "core_mqtt_agent.h"

/* MQTT agent task API. */
#include "mqtt_agent_task.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

#include "kvstore.h"

#include "cli.h"
#include "cli_prv.h"

/**
 * These configurations are required. Throw compilation error if it is not
 * defined.
 */

#ifndef democonfigPROVISIONING_TEMPLATE_NAME
    #error "Please define democonfigPROVISIONING_TEMPLATE_NAME to the template name registered with AWS IoT Core in demo_config.h."
#endif

/**
 * @brief The length of #democonfigPROVISIONING_TEMPLATE_NAME.
 */
#define fpdemoPROVISIONING_TEMPLATE_NAME_LENGTH    ( ( uint16_t ) ( sizeof( democonfigPROVISIONING_TEMPLATE_NAME ) - 1 ) )

/**
 * @brief Size of AWS IoT Thing name buffer.
 *
 * See https://docs.aws.amazon.com/iot/latest/apireference/API_CreateThing.html#iot-CreateThing-request-thingName
 */
#define fpdemoMAX_THING_NAME_LENGTH                128

/**
 * @brief The maximum number of times to run the loop in this demo.
 *
 * @note The demo loop is attempted to re-run only if it fails in an iteration.
 * Once the demo loop succeeds in an iteration, the demo exits successfully.
 */
#ifndef fpdemoMAX_DEMO_LOOP_COUNT
#define fpdemoMAX_DEMO_LOOP_COUNT    ( 3 )
#endif

/**
 * @brief Time in seconds to wait between retries of the demo loop if
 * demo loop fails.
 */
#define fpdemoDELAY_BETWEEN_DEMO_RETRY_ITERATIONS_SECONDS    ( 10 )

/**
 * @brief Size of buffer in which to hold the certificate signing request (CSR).
 */
#define fpdemoCSR_BUFFER_LENGTH                              4096

/**
 * @brief Size of buffer in which to hold the certificate.
 */
#define fpdemoCERT_BUFFER_LENGTH                             4096

/**
 * @brief Size of buffer in which to hold the certificate id.
 *
 * @note Has a maximum length of 64 for more information see the following link
 * https://docs.aws.amazon.com/iot/latest/apireference/API_Certificate.html#iot-Type-Certificate-certificateId
 */
#define fpdemoCERT_ID_BUFFER_LENGTH                          64

/**
 * @brief Size of buffer in which to hold the certificate ownership token.
 */
#define fpdemoOWNERSHIP_TOKEN_BUFFER_LENGTH                  1024

/**
 * @brief Milliseconds per second.
 */
#define fpdemoMILLISECONDS_PER_SECOND                        ( 1000U )

/**
 * @brief Milliseconds per FreeRTOS tick.
 */
#define fpdemoMILLISECONDS_PER_TICK                          ( fpdemoMILLISECONDS_PER_SECOND / configTICK_RATE_HZ )

/**
 * @brief The maximum amount of time in milliseconds to wait for the commands
 * to be posted to the MQTT agent should the MQTT agent's command queue be full.
 * Tasks wait in the Blocked state, so don't use any CPU time.
 */
#define configMAX_COMMAND_SEND_BLOCK_TIME_MS         ( 500 )
/**
 * @brief Status values of the Fleet Provisioning response.
 */
typedef enum
{
  ResponseNotReceived, ResponseAccepted, ResponseRejected
} ResponseStatus_t;

/*-----------------------------------------------------------*/

/**
 * @brief Status reported from the MQTT publish callback.
 */
static ResponseStatus_t xResponseStatus;

/**
 * @brief Buffer to hold responses received from the AWS IoT Fleet Provisioning
 * APIs. When the MQTT publish callback receives an expected Fleet Provisioning
 * accepted payload, it copies it into this buffer.
 */
static uint8_t *pucPayloadBuffer;//[democonfigNETWORK_BUFFER_SIZE * 2];

/**
 * @brief Length of the payload stored in #pucPayloadBuffer. This is set by the
 * MQTT publish callback when it copies a received payload into #pucPayloadBuffer.
 */
static size_t xPayloadLength;
static size_t xRxPayloadLength;

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
 * @brief Callback to receive the incoming publish messages from the MQTT
 * broker. Sets xResponseStatus if an expected CreateCertificateFromCsr or
 * RegisterThing response is received, and copies the response into
 * responseBuffer if the response is an accepted one.
 *
 * @param[in] pPublishInfo Pointer to publish info of the incoming publish.
 * @param[in] usPacketIdentifier Packet identifier of the incoming publish.
 */
static void vIncomingPublishCallback(void *pvIncomingPublishCallbackContext, MQTTPublishInfo_t *pxPublishInfo);

/**
 * @brief Subscribe to the CreateCertificateFromCsr accepted and rejected topics.
 */
static bool xSubscribeToCsrResponseTopics(void);

/**
 * @brief Unsubscribe from the CreateCertificateFromCsr accepted and rejected topics.
 */
static bool xUnsubscribeFromCsrResponseTopics(void);

/**
 * @brief Subscribe to the RegisterThing accepted and rejected topics.
 */
static bool xSubscribeToRegisterThingResponseTopics(void);

/**
 * @brief Unsubscribe from the RegisterThing accepted and rejected topics.
 */
static bool xUnsubscribeFromRegisterThingResponseTopics(void);

#define configPAYLOAD_BUFFER_LENGTH fpdemoCERT_BUFFER_LENGTH

static void vIncomingPublishCallback(void *pvIncomingPublishCallbackContext, MQTTPublishInfo_t *pxPublishInfo);

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
static void vPublishCommandCallback(MQTTAgentCommandContext_t *pxCommandContext, MQTTAgentReturnInfo_t *pxReturnInfo);

static bool xSubscribeToTopic(MQTTQoS_t xQoS, char *pcTopicFilter);

static bool xUnsubscribeFromTopic(char *pcTopicFilter);

static bool xPublishToTopic(MQTTQoS_t xQoS, char *pcTopic, uint8_t *pucPayload, size_t xPayloadLength);

/*-----------------------------------------------------------*/

static void vIncomingPublishCallback(void *pvIncomingPublishCallbackContext, MQTTPublishInfo_t *pxPublishInfo)
{
  FleetProvisioningStatus_t xStatus;
  FleetProvisioningTopic_t xApi;

  LogInfo("vIncomingPublishCallback");

  (void) pvIncomingPublishCallbackContext;

  xStatus = FleetProvisioning_MatchTopic(pxPublishInfo->pTopicName, pxPublishInfo->topicNameLength, &xApi);

  if (xStatus != FleetProvisioningSuccess)
  {
    LogWarn(( "Unexpected publish message received. Topic: %.*s.", ( int ) pxPublishInfo->topicNameLength, ( const char * ) pxPublishInfo->pTopicName ));
  }
  else
  {
    if (xApi == FleetProvCborCreateCertFromCsrAccepted)
    {
      LogInfo(( "Received accepted response from Fleet Provisioning CreateCertificateFromCsr API." ));

      xResponseStatus = ResponseAccepted;

      /* Copy the payload from the MQTT library's buffer to #pucPayloadBuffer. */
      (void) memcpy((void*) pucPayloadBuffer, (const void*) pxPublishInfo->pPayload, (size_t) pxPublishInfo->payloadLength);

      xRxPayloadLength = pxPublishInfo->payloadLength;
    }
    else if (xApi == FleetProvCborCreateCertFromCsrRejected)
    {
      LogError(( "Received rejected response from Fleet Provisioning CreateCertificateFromCsr API." ));

      xResponseStatus = ResponseRejected;
    }
    else if (xApi == FleetProvCborRegisterThingAccepted)
    {
      LogInfo(( "Received accepted response from Fleet Provisioning RegisterThing API." ));

      xRxPayloadLength = ResponseAccepted;

      /* Copy the payload from the MQTT library's buffer to #pucPayloadBuffer. */
      (void) memcpy((void*) pucPayloadBuffer, (const void*) pxPublishInfo->pPayload, (size_t) pxPublishInfo->payloadLength);

      xRxPayloadLength = pxPublishInfo->payloadLength;
    }
    else if (xApi == FleetProvCborRegisterThingRejected)
    {
      LogError(( "Received rejected response from Fleet Provisioning RegisterThing API." ));

      LogInfo("%s", pxPublishInfo->pPayload);

      xRxPayloadLength = ResponseRejected;
    }
    else
    {
      LogError(( "Received message on unexpected Fleet Provisioning topic. Topic: %.*s.", ( int ) pxPublishInfo->topicNameLength, ( const char * ) pxPublishInfo->pTopicName ));
    }
  }
}

static bool xSubscribeToTopic(MQTTQoS_t xQoS, char *pcTopicFilter)
{
  MQTTStatus_t xMQTTStatus;

  /* Loop in case the queue used to communicate with the MQTT agent is full and
   * attempts to post to it time out.  The queue will not become full if the
   * priority of the MQTT agent task is higher than the priority of the task
   * calling this function. */
  do
  {
    xMQTTStatus = MqttAgent_SubscribeSync(xMQTTAgentHandle, pcTopicFilter, xQoS, vIncomingPublishCallback, NULL);

    if (xMQTTStatus != MQTTSuccess)
    {
      LogError(( "Failed to SUBSCRIBE to topic with error = %u.", xMQTTStatus ));
    }
    else
    {
      LogInfo(( "Subscribed to topic %.*s.\n\n", strlen( pcTopicFilter ), pcTopicFilter ));
    }
  } while (xMQTTStatus != MQTTSuccess);

  return xMQTTStatus == MQTTSuccess;
}

static bool xUnsubscribeFromTopic(char *pcTopicFilter)
{
  MQTTStatus_t xMQTTStatus;

  /* Loop in case the queue used to communicate with the MQTT agent is full and
   * attempts to post to it time out.  The queue will not become full if the
   * priority of the MQTT agent task is higher than the priority of the task
   * calling this function. */
  do
  {
    xMQTTStatus = MqttAgent_UnSubscribeSync(xMQTTAgentHandle, pcTopicFilter, vIncomingPublishCallback, NULL);

    if (xMQTTStatus != MQTTSuccess)
    {
      LogError(( "Failed to UNSUBSCRIBE from topic with error = %u.", xMQTTStatus ));
    }
    else
    {
      LogInfo(( "Unsubscribed to topic %.*s.\n\n", strlen( pcTopicFilter ), pcTopicFilter ));
    }
  } while (xMQTTStatus != MQTTSuccess);

  return xMQTTStatus == MQTTSuccess;
}

static void vPublishCommandCallback(MQTTAgentCommandContext_t *pxCommandContext, MQTTAgentReturnInfo_t *pxReturnInfo)
{
  if (pxCommandContext->xTaskToNotify != NULL)
  {
    xTaskNotify(pxCommandContext->xTaskToNotify, pxReturnInfo->returnCode, eSetValueWithOverwrite);
  }
}

static bool xPublishToTopic(MQTTQoS_t xQoS, char *pcTopic, uint8_t *pucPayload, size_t xPayloadLength)
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
  xPublishInfo.pTopicName = pcTopic;
  xPublishInfo.topicNameLength = (uint16_t) strlen(pcTopic);
  xPublishInfo.pPayload = pucPayload;
  xPublishInfo.payloadLength = xPayloadLength;

  xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

  xCommandParams.blockTimeMs = configMAX_COMMAND_SEND_BLOCK_TIME_MS;
  xCommandParams.cmdCompleteCallback = vPublishCommandCallback;
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
        xMQTTStatus = (MQTTStatus_t) (ulNotifiedValue);
      }
      else
      {
        xMQTTStatus = MQTTRecvFailed;
      }
    }
  } while (xMQTTStatus != MQTTSuccess);

  return xMQTTStatus == MQTTSuccess;
}

static bool xSubscribeToCsrResponseTopics(void)
{
  bool status;

  status = xSubscribeToTopic(MQTTQoS0, FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC);

  if (status == false)
  {
    LogError(( "Failed to subscribe to fleet provisioning topic: %.*s.", FP_CBOR_CREATE_CERT_ACCEPTED_LENGTH, FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC ));
  }

  if (status == true)
  {
    status = xSubscribeToTopic(MQTTQoS0, FP_CBOR_CREATE_CERT_REJECTED_TOPIC);

    if (status == false)
    {
      LogError(( "Failed to subscribe to fleet provisioning topic: %.*s.", FP_CBOR_CREATE_CERT_REJECTED_LENGTH, FP_CBOR_CREATE_CERT_REJECTED_TOPIC ));
    }
  }

  return status;
}
/*-----------------------------------------------------------*/

static bool xUnsubscribeFromCsrResponseTopics(void)
{
  bool status;

  status = xUnsubscribeFromTopic(FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC);

  if (status == false)
  {
    LogError(( "Failed to unsubscribe from fleet provisioning topic: %.*s.", FP_CBOR_CREATE_CERT_ACCEPTED_LENGTH, FP_CBOR_CREATE_CERT_ACCEPTED_TOPIC ));
  }

  if (status == false)
  {
    status = xUnsubscribeFromTopic(FP_CBOR_CREATE_CERT_REJECTED_TOPIC);

    if (status == false)
    {
      LogError(( "Failed to unsubscribe from fleet provisioning topic: %.*s.", FP_CBOR_CREATE_CERT_REJECTED_LENGTH, FP_CBOR_CREATE_CERT_REJECTED_TOPIC ));
    }
  }

  return status;
}
/*-----------------------------------------------------------*/

static bool xSubscribeToRegisterThingResponseTopics(void)
{
  bool status;

  status = xSubscribeToTopic(MQTTQoS0, FP_CBOR_REGISTER_ACCEPTED_TOPIC(democonfigPROVISIONING_TEMPLATE_NAME));

  if (status == false)
  {
    LogError(( "Failed to subscribe to fleet provisioning topic: %.*s.", FP_CBOR_REGISTER_ACCEPTED_LENGTH( fpdemoPROVISIONING_TEMPLATE_NAME_LENGTH ), FP_CBOR_REGISTER_ACCEPTED_TOPIC( democonfigPROVISIONING_TEMPLATE_NAME ) ));
  }

  if (status == true)
  {
    status = xSubscribeToTopic(MQTTQoS0, FP_CBOR_REGISTER_REJECTED_TOPIC(democonfigPROVISIONING_TEMPLATE_NAME));

    if (status == false)
    {
      LogError(( "Failed to subscribe to fleet provisioning topic: %.*s.", FP_CBOR_REGISTER_REJECTED_LENGTH( fpdemoPROVISIONING_TEMPLATE_NAME_LENGTH ), FP_CBOR_REGISTER_REJECTED_TOPIC( democonfigPROVISIONING_TEMPLATE_NAME ) ));
    }
  }

  return status;
}

/*-----------------------------------------------------------*/

static bool xUnsubscribeFromRegisterThingResponseTopics(void)
{
  bool status;

  status = xUnsubscribeFromTopic(FP_CBOR_REGISTER_ACCEPTED_TOPIC(democonfigPROVISIONING_TEMPLATE_NAME));

  if (status == false)
  {
    LogError(( "Failed to unsubscribe from fleet provisioning topic: %.*s.", FP_CBOR_REGISTER_ACCEPTED_LENGTH( fpdemoPROVISIONING_TEMPLATE_NAME_LENGTH ), FP_CBOR_REGISTER_ACCEPTED_TOPIC( democonfigPROVISIONING_TEMPLATE_NAME ) ));
  }

  if (status == true)
  {
    status = xUnsubscribeFromTopic(FP_CBOR_REGISTER_REJECTED_TOPIC(democonfigPROVISIONING_TEMPLATE_NAME));

    if (status == false)
    {
      LogError(( "Failed to unsubscribe from fleet provisioning topic: %.*s.", FP_CBOR_REGISTER_REJECTED_LENGTH( fpdemoPROVISIONING_TEMPLATE_NAME_LENGTH ), FP_CBOR_REGISTER_REJECTED_TOPIC( democonfigPROVISIONING_TEMPLATE_NAME ) ));
    }
  }

  return status;
}
/*-----------------------------------------------------------*/

/* This example uses a single application task, which shows that how to use
 * the Fleet Provisioning library to generate and validate AWS IoT Fleet
 * Provisioning MQTT topics, and use the coreMQTT library to communicate with
 * the AWS IoT Fleet Provisioning APIs. */
int prvFleetProvisioningTask(void *pvParameters)
{
  bool xStatus = false;

  /* Buffer for holding the CSR. */
  char *pcCsr = pvPortMalloc(fpdemoCSR_BUFFER_LENGTH);
  configASSERT(pcCsr != NULL);

  size_t xCsrLength = 0;

  /* Buffer for holding received certificate until it is saved. */
  char *pcCertificate = pvPortMalloc(fpdemoCERT_BUFFER_LENGTH);
  configASSERT(pcCertificate != NULL);

  size_t xCertificateLength;

  /* Buffer for holding the certificate ID. */
  char *pcCertificateId = pvPortMalloc(fpdemoCERT_ID_BUFFER_LENGTH);
  configASSERT(pcCertificateId != NULL);

  size_t xCertificateIdLength;

  /* Buffer for holding the certificate ownership token. */
  char *pcOwnershipToken = pvPortMalloc(fpdemoOWNERSHIP_TOKEN_BUFFER_LENGTH);
  configASSERT(pcOwnershipToken != NULL);

  size_t xOwnershipTokenLength;

  /* Buffer for holding the ThingName. */
  char *pcThingName = NULL; /* To be allocated by the KVStore_getStringHeap() function */

  /* Length of the AWS IoT Thing name. */
  size_t xThingNameLength;

  /* Buffer for holding the CSR Subject. */
  char * pcCSR_SUBJECT_NAME  = pvPortMalloc(democonfigMAX_THING_NAME_LENGTH);
  configASSERT(pcCSR_SUBJECT_NAME != NULL);

  /* Buffer for holding the ThingGroup. */
  char * pcThingGroupName =  NULL; /* To be allocated by the KVStore_getStringHeap() function */

  /* Length of the AWS IoT Thing Group name. */
  size_t xThingGroupNameLength;

  /* PKCS11 Session handle */
  CK_SESSION_HANDLE xP11Session;

  /* PKCS11 operations status */
  CK_RV xPkcs11Ret = CKR_OK;

  /* MQTT Quality Of Service */
  MQTTQoS_t xQoS = 0;

  /* Silence compiler warnings about unused variables. */
  (void) pvParameters;

  /* Buffer to hold the MQTT data */
  pucPayloadBuffer = pvPortMalloc(democonfigNETWORK_BUFFER_SIZE * 2);
  configASSERT(pucPayloadBuffer != NULL);

  memset(pcCsr, 0, fpdemoCSR_BUFFER_LENGTH);

  /* Wait until the MQTT agent is ready */
  vSleepUntilMQTTAgentReady();

  /* Get the MQTT Agent handle */
  xMQTTAgentHandle = xGetMqttAgentHandle();
  configASSERT(xMQTTAgentHandle != NULL);

  /* Wait until we are connected to AWS */
  vSleepUntilMQTTAgentConnected();

  LogInfo(( "MQTT Agent is connected. Starting the fleet provisioning task. " ));

  /* Generate the Thing Name from KV Store */
  pcThingName = KVStore_getStringHeap( CS_CORE_THING_NAME, (size_t *)&( xThingNameLength ) );

  /* Generate the subject */
  snprintf(pcCSR_SUBJECT_NAME, democonfigMAX_THING_NAME_LENGTH, "CN=%s", pcThingName);

  /* Get the ThingGroupName */
  pcThingGroupName = KVStore_getStringHeap(CS_THING_GROUP_NAME, (size_t *)&xThingGroupNameLength);

  do
  {
    /* Initialize the buffer lengths to their max lengths. */
    xCertificateLength = fpdemoCERT_BUFFER_LENGTH;
    xCertificateIdLength = fpdemoCERT_ID_BUFFER_LENGTH;
    xOwnershipTokenLength = fpdemoOWNERSHIP_TOKEN_BUFFER_LENGTH;

    /* Initialize the PKCS #11 module */
    xPkcs11Ret = xInitializePkcs11Session(&xP11Session);

    if (xPkcs11Ret != CKR_OK)
    {
      LogError(( "Failed to initialize PKCS #11." ));
      xStatus = false;
    }
    else
    {
      LogInfo("xGenerateKeyAndCsr");
      xStatus = xGenerateKeyAndCsr(xP11Session, pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS, pkcs11configLABEL_DEVICE_PUBLIC_KEY_FOR_TLS, pcCsr, fpdemoCSR_BUFFER_LENGTH,pcCSR_SUBJECT_NAME, &xCsrLength);

      if (xStatus == false)
      {
        LogError(( "Failed to generate Key and Certificate Signing Request." ));

        xPkcs11CloseSession(xP11Session);
      }
    }

    /**** Call the CreateCertificateFromCsr API ***************************/

    /* We use the CreateCertificatefromCsr API to obtain a client certificate for a key on the device by means of sending a certificate signing request (CSR). */
    if (xStatus == true)
    {
      /* Subscribe to the CreateCertificateFromCsr accepted and rejected topics. In this demo we use CBOR encoding for the payloads, so we use the CBOR variants of the topics. */
      LogInfo("xSubscribeToCsrResponseTopics");

      xStatus = xSubscribeToCsrResponseTopics();

      if (xStatus == true)
      {
        LogInfo("xSubscribeToRegisterThingResponseTopics");

        /* Subscribe to the RegisterThing response topics. */
        xStatus = xSubscribeToRegisterThingResponseTopics();
      }
    }

    if (xStatus == true)
    {
      LogInfo("xGenerateCsrRequest");
      /* Create the request payload containing the CSR to publish to the CreateCertificateFromCsr APIs. */
      xStatus = xGenerateCsrRequest(pucPayloadBuffer, democonfigNETWORK_BUFFER_SIZE, pcCsr, xCsrLength, &xPayloadLength);
    }

    if (xStatus == true)
    {
      LogInfo("Publish CREATE_CERT");
      /* Publish the CSR to the CreateCertificatefromCsr API. */
      xStatus = xPublishToTopic(xQoS, FP_CBOR_CREATE_CERT_PUBLISH_TOPIC, (uint8_t*) pucPayloadBuffer, xPayloadLength);

      if (xStatus == false)
      {
        LogError(( "Failed to publish to fleet provisioning topic: %.*s.", FP_CBOR_CREATE_CERT_PUBLISH_LENGTH, FP_CBOR_CREATE_CERT_PUBLISH_TOPIC ));
      }
    }

    if (xStatus == true)
    {
      vTaskDelay(10000);

      LogInfo("xParseCsrResponse");
      /* From the response, extract the certificate, certificate ID, and certificate ownership token. */
      xStatus = xParseCsrResponse(pucPayloadBuffer, xRxPayloadLength, pcCertificate, &xCertificateLength, pcCertificateId, &xCertificateIdLength, pcOwnershipToken, &xOwnershipTokenLength);

      if (xStatus == true)
      {
        LogInfo(( "Received certificate with Id: %.*s", ( int ) xCertificateIdLength, pcCertificateId ));
      }
    }

    if (xStatus == true)
    {
      /* Save the certificate into PKCS #11. */
      xStatus = xLoadCertificate(xP11Session, pcCertificate, pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS, xCertificateLength);
    }

    /* Unsubscribe from the CreateCertificateFromCsr topics. */
    xUnsubscribeFromCsrResponseTopics();

    /**** Call the RegisterThing API **************************************/

    /* We then use the RegisterThing API to activate the received certificate, provision AWS IoT resources according to the provisioning template, and receive device configuration. */
    if (xStatus == true)
    {
      memset(pucPayloadBuffer, 0, democonfigNETWORK_BUFFER_SIZE * 2);
      /* Create the request payload to publish to the RegisterThing API. */
      xStatus = xGenerateRegisterThingRequest(pucPayloadBuffer, democonfigNETWORK_BUFFER_SIZE, pcOwnershipToken, xOwnershipTokenLength,
            pcThingName, xThingNameLength,
            pcThingGroupName, xThingGroupNameLength, &xPayloadLength);
    }

    if (xStatus == true)
    {

      /* Publish the RegisterThing request. */
      xStatus = xPublishToTopic(xQoS, FP_CBOR_REGISTER_PUBLISH_TOPIC(democonfigPROVISIONING_TEMPLATE_NAME), (uint8_t*) pucPayloadBuffer, xPayloadLength);

      if (xStatus != true)
      {
        LogError(( "Failed to publish to fleet provisioning topic: %.*s.", FP_CBOR_REGISTER_PUBLISH_LENGTH( fpdemoPROVISIONING_TEMPLATE_NAME_LENGTH ), FP_CBOR_REGISTER_PUBLISH_TOPIC( democonfigPROVISIONING_TEMPLATE_NAME ) ));
      }
    }

    if (xStatus == true)
    {
      vTaskDelay(3000);

      /* Extract the Thing name from the response. */
      xThingNameLength = fpdemoMAX_THING_NAME_LENGTH;
      xStatus = xParseRegisterThingResponse(pucPayloadBuffer, xRxPayloadLength, pcThingName, &xThingNameLength);

      if (xStatus == true)
      {
        LogInfo(( "Received AWS IoT Thing name: %.*s", ( int ) xThingNameLength, pcThingName ));
      }
    }

    /* Unsubscribe from the RegisterThing topics. */
    xUnsubscribeFromRegisterThingResponseTopics();
  } while (0);

  vPortFree(pcCsr);
  vPortFree(pcCertificate);
  vPortFree(pcCertificateId);
  vPortFree(pcOwnershipToken);
  vPortFree(pcThingName);
  vPortFree(pcCSR_SUBJECT_NAME);
  vPortFree(pcThingGroupName);
  vPortFree(pucPayloadBuffer);

  if (xStatus == true)
  {
    /* Update the KV Store */
    KVStore_setUInt32(CS_PROVISIONED, 1);
    KVStore_xCommitChanges();

    vDoSystemReset();
  }

  /* Delete this task. */
  LogInfo(( "Deleting Fleet Provisioning Demo task." ));
  vTaskDelete( NULL);

  return (xStatus == true) ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif
/*-----------------------------------------------------------*/

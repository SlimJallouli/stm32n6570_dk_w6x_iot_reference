/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : w6x_transport.c
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
#if defined(MBEDTLS_CONFIG_FILE)
#include "FreeRTOS.h"
#include "w6x_transport.h"
#include "w6x_api.h"
#include "w6x_fs.h"
#include "core_pkcs11_pal.h"
#include "core_pkcs11_pal_utils.h"
#include "mqtt_metrics.h"
#include "kvstore.h"

#include <string.h>
/* Private typedef -----------------------------------------------------------*/

/* Private Macro -------------------------------------------------------------*/

/* Private Variables ---------------------------------------------------------*/

/* Private Function prototypes -----------------------------------------------*/

/* User code -----------------------------------------------------------------*/
/*-----------------------------------------------------------*/

static bool prvConfigCheckCertificate(char *file_name, char *label)
{
  bool status = false;
  CK_RV pkcs11_status = CKR_OK;
  CK_OBJECT_HANDLE xHandle = eInvalidHandle;
  CK_BYTE_PTR pucData = NULL;
  CK_ULONG ulDataSize = 0;
  CK_BBOOL IsPrivate = CK_FALSE;
  CK_ULONG usLength = 0;
  W6X_Status_t xW6X_Status = W6X_STATUS_ERROR;

  xW6X_Status = W6X_CheckFile((char*) file_name);

  if (xW6X_Status == W6X_STATUS_OK)
  {
    xW6X_Status = W6X_GetFileSize((char*) file_name, &usLength);
  }

  xHandle = PKCS11_PAL_FindObject((CK_BYTE_PTR) label, usLength);

  if (eInvalidHandle != xHandle)
  {
    pkcs11_status = PKCS11_PAL_GetObjectValue(xHandle, &pucData, &ulDataSize, &IsPrivate);
  }

  if(ulDataSize<=4)
  {
    return false;
  }

  if(usLength == ulDataSize)
  {
    LogInfo("%s found in W6X", file_name);
    return true;
  }

  LogInfo("%s not found in W6X", file_name);

  if (CKR_OK == pkcs11_status)
  {
    xW6X_Status = W6X_CreateFile((char*) file_name);

    if (xW6X_Status == W6X_STATUS_OK)
    {
      LogInfo("Writing %s to W6X", file_name);

      xW6X_Status = W6X_WriteFile((char*) file_name, (char *)pucData, &ulDataSize);
    }

    if (xW6X_Status == W6X_STATUS_OK)
    {
      status = true;
    }
  }

  if(pucData != NULL)
  {
    vPortFree(pucData);
  }

  return status;
}

/*-----------------------------------------------------------*/

static bool prvConfigMQTTSecurity(W6X_MQTT_Connect_t *pxCtx)
{
  BaseType_t xSuccess = pdTRUE;

  pxCtx->Scheme = KVStore_getUInt32(CS_MQTT_SECURITY, &xSuccess);

  if (xSuccess == pdFALSE)
  {
    LogError("Invalid MQTT security configuration read from KVStore.");
    return false;
  }

  return true;
}

/*-----------------------------------------------------------*/

static bool prvConfigCertificates(W6X_MQTT_Connect_t *pxCtx, const PkiObject_t *pxPrivateKey, const PkiObject_t *pxClientCert, const PkiObject_t *pxRootCaCerts)
{
  bool status = false;

  CK_RV pkcs11_status;

  pkcs11_status = PKCS11_PAL_Initialize();

  if (CKR_OK != pkcs11_status)
  {
    return false;
  }

  if (pxCtx->Scheme != 4)
  {
    return true; // Skip certificate loading for non-secure schemes
  }

  if (!W6X_FileInit())
  {
    return false;
  }

  if (CKR_OK == pkcs11_status)
  {
    const char *pcFileName = NULL;
    CK_OBJECT_HANDLE xHandle = (CK_OBJECT_HANDLE) eInvalidHandle;

    PAL_UTILS_LabelToFilenameHandle(pxRootCaCerts->pcPkcs11Label, &pcFileName, &xHandle);

    if (xHandle != eInvalidHandle)
    {
      memcpy(pxCtx->CACertificate, pcFileName, strlen(pcFileName));
      PAL_UTILS_LabelToFilenameHandle(pxClientCert->pcPkcs11Label, &pcFileName, &xHandle);
    }

    if (xHandle != eInvalidHandle)
    {
      memcpy(pxCtx->Certificate, pcFileName, strlen(pcFileName));
      PAL_UTILS_LabelToFilenameHandle(pxPrivateKey->pcPkcs11Label, &pcFileName, &xHandle);
    }

    if (xHandle != eInvalidHandle)
    {
      memcpy(pxCtx->PrivateKey, pcFileName, strlen(pcFileName));
      status =  true;
    }
  }

  if(CKR_OK == pkcs11_status)
  {
#if defined(FLEET_PROVISION_DEMO)
    BaseType_t xSuccess = pdTRUE;

    uint32_t provisioned = KVStore_getUInt32( CS_PROVISIONED, &( xSuccess ) );

    if(provisioned == 0)
    {
      status  = prvConfigCheckCertificate((char *)pxCtx->CACertificate, pkcs11configLABEL_ROOT_CERTIFICATE );
      status &= prvConfigCheckCertificate((char *)pxCtx->Certificate  , pkcs11configLABEL_CLAIM_CERTIFICATE);
      status &= prvConfigCheckCertificate((char *)pxCtx->PrivateKey   , pkcs11configLABEL_CLAIM_PRIVATE_KEY);
    }
    else
    {
      status  = prvConfigCheckCertificate((char *)pxCtx->CACertificate, pkcs11configLABEL_ROOT_CERTIFICATE          );
      status &= prvConfigCheckCertificate((char *)pxCtx->Certificate  , pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS);
      status &= prvConfigCheckCertificate((char *)pxCtx->PrivateKey   , pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS);
    }

#else
    status  = prvConfigCheckCertificate((char *)pxCtx->CACertificate, pkcs11configLABEL_ROOT_CERTIFICATE          );
    status &= prvConfigCheckCertificate((char *)pxCtx->Certificate  , pkcs11configLABEL_DEVICE_CERTIFICATE_FOR_TLS);
    status &= prvConfigCheckCertificate((char *)pxCtx->PrivateKey   , pkcs11configLABEL_DEVICE_PRIVATE_KEY_FOR_TLS);
#endif
  }

  return status;
}

/*-----------------------------------------------------------*/

static bool prvConfigMQTTEndpoint(W6X_MQTT_Connect_t *pxCtx, const char *pcHostName)
{
  memcpy(pxCtx->HostName, pcHostName, strlen(pcHostName));

  return true;
}

/*-----------------------------------------------------------*/

static bool prvConfigMQTTClientId(W6X_MQTT_Connect_t *pxCtx)
{
  uint32_t uid0 = HAL_GetUIDw0();
  uint32_t uid1 = HAL_GetUIDw1();
  uint32_t uid2 = HAL_GetUIDw2();

  memset(pxCtx->MQClientId,0, 32);

  snprintf((char *)pxCtx->MQClientId, 24, "%08X%08X%08X", (int)uid0, (int)uid1, (int)uid2);

  return true;
}

/*-----------------------------------------------------------*/

static bool prvConfigMQTTUser(W6X_MQTT_Connect_t *pxCtx)
{
  memset(pxCtx->MQUserName, 0, 32);

  char *pucMqttEndpoint;
  size_t uxMqttEndpointLen;

  pucMqttEndpoint = KVStore_getStringHeap(CS_CORE_MQTT_ENDPOINT, &uxMqttEndpointLen);

  if (uxMqttEndpointLen)
  {
    /* If we are connecting to AWS */
    if (strstr(pucMqttEndpoint, "amazonaws") != NULL)
    {
      snprintf((char*) pxCtx->MQUserName, 32, "%s", AWS_IOT_METRICS_STRING);
    }

    vPortFree(pucMqttEndpoint);
  }

  return true;
}

/*-----------------------------------------------------------*/

static bool prvConfigMQTTPort(W6X_MQTT_Connect_t *pxCtx, uint16_t usPort)
{
  pxCtx->HostPort = usPort;

  return true;
}

/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

NetworkContext_t* w6x_transport_allocate(void)
{
  return pvPortMalloc( sizeof( W6X_MQTT_Connect_t ) );
}

/*-----------------------------------------------------------*/

W6X_Status_t w6x_transport_configure(NetworkContext_t *pxNetworkContext, const char **ppcAlpnProtos, const PkiObject_t *pxPrivateKey, const PkiObject_t *pxClientCert, const PkiObject_t *pxRootCaCerts, const size_t uxNumRootCA)
{
  W6X_Status_t xStatus = W6X_STATUS_OK;
  W6X_MQTT_Connect_t * pxCtx = (W6X_MQTT_Connect_t *)pxNetworkContext;

  if (!prvConfigMQTTSecurity(pxCtx))
  {
    return W6X_STATUS_ERROR;
  }

  if (!prvConfigCertificates(pxCtx, pxPrivateKey, pxClientCert, pxRootCaCerts))
  {
    return W6X_STATUS_ERROR;
  }

  return xStatus;
}

/*-----------------------------------------------------------*/

W6X_Status_t w6x_transport_connect(NetworkContext_t *pxNetworkContext, const char *pcHostName, uint16_t usPort, uint32_t ulRecvTimeoutMs, uint32_t ulSendTimeoutMs)
{
  W6X_Status_t xStatus = W6X_STATUS_OK;
  W6X_MQTT_Connect_t * pxCtx = (W6X_MQTT_Connect_t *)pxNetworkContext;

  memcpy(pxCtx->HostName, pcHostName, strlen(pcHostName));

  if (!prvConfigMQTTEndpoint(pxCtx, pcHostName))
  {
    return W6X_STATUS_ERROR;
  }

  if (!prvConfigMQTTClientId(pxCtx))
  {
    return W6X_STATUS_ERROR;
  }

  if (!prvConfigMQTTUser(pxCtx))
  {
    return W6X_STATUS_ERROR;
  }

  if (!prvConfigMQTTPort(pxCtx, usPort))
  {
    return W6X_STATUS_ERROR;
  }

  pxCtx->SNI_enabled = 1;

  return xStatus;
}

/*-----------------------------------------------------------*/

void w6x_transport_disconnect(NetworkContext_t *pxNetworkContext)
{
  W6X_MQTT_Disconnect();
}

/*-----------------------------------------------------------*/

void w6x_transport_free(NetworkContext_t *pxNetworkContext)
{
 vPortFree(pxNetworkContext);
}
#endif

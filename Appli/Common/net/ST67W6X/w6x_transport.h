/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : w6x_transport.h
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
#include "transport_interface.h"
#include "PkiObject.h"
#include "w6x_types.h"

/* Private typedef -----------------------------------------------------------*/

/* Private Macro -------------------------------------------------------------*/

/* Private Variables ---------------------------------------------------------*/

/* Private Function prototypes -----------------------------------------------*/

/* User code -----------------------------------------------------------------*/

/**
  * @brief  function description
  * @retval return type
  */
NetworkContext_t* w6x_transport_allocate(void);

W6X_Status_t w6x_transport_configure(NetworkContext_t *pxNetworkContext, const char **ppcAlpnProtos, const PkiObject_t *pxPrivateKey, const PkiObject_t *pxClientCert, const PkiObject_t *pxRootCaCerts, const size_t uxNumRootCA);

W6X_Status_t w6x_transport_connect(NetworkContext_t *pxNetworkContext, const char *pcHostName, uint16_t usPort, uint32_t ulRecvTimeoutMs, uint32_t ulSendTimeoutMs);

void w6x_transport_disconnect(NetworkContext_t *pxNetworkContext);

void w6x_transport_free(NetworkContext_t *pxNetworkContext);
#endif

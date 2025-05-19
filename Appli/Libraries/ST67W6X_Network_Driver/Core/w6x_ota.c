/**
  ******************************************************************************
  * @file    w6x_ota.c
  * @author  GPM Application Team
  * @brief   This file provides code for W6x OTA API
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

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "w6x_api.h"       /* Prototypes of the functions implemented in this file */
#include "w6x_internal.h"
#include "w61_at_api.h"    /* Prototypes of the functions called by this file */
#include "w61_io.h"        /* Prototypes of the BUS functions to be registered */
#include "common_parser.h" /* Common Parser functions */

/* Global variables ----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/** @defgroup ST67W6X_Private_OTA_Constants ST67W6X OTA Constants
  * @ingroup  ST67W6X_Private_OTA
  * @{
  */

/** Max length of a IPv4 in string format */
#define IP4_ADDR_STR_LENGTH   16

/** data buffer for ota send can't exceed 4096 (hardcoded in ncp code)
  * Need to pay attention to this value, NCP header can't be bigger than this value because NCP attempts
  * to read header from first chunk received */
#define OTA_DATA_BUFFER_SIZE  4096

#if (HTTP_HEAD_MAX_RESP_BUFFER_SIZE > OTA_DATA_BUFFER_SIZE)
#error "OTA_DATA_BUFFER_SIZE value is smaller than HTTP_HEAD_MAX_RESP_BUFFER_SIZE. \
        OTA buffer should contain HTTP header response max accepted size in case some of the HTTP body is received."
#endif /* HTTP_HEAD_MAX_RESP_BUFFER_SIZE > OTA_DATA_BUFFER_SIZE */

/** How many retry data socket receive attempts before declaring timeout, allows to compensate
  * when the NCP is stuck on receiving data and does give any sign of life for a long time */
#define OTA_RETRY_ATTEMPTS    4

/** @} */

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/** @defgroup ST67W6X_Private_OTA_Variables ST67W6X OTA Variables
  * @ingroup  ST67W6X_Private_OTA
  * @{
  */
static W61_Object_t  *p_DrvObj; /*!< Global W61 context pointer */

/** @} */

/* Functions Definition ------------------------------------------------------*/
/** @addtogroup ST67W6X_API_OTA_Public_Functions
  * @{
  */

W6X_Status_t W6X_OTA_Starts(uint32_t enable)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  p_DrvObj = W61_ObjGet();
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* OTA start on W61 currently supports only value 1 or 0 as parameters, all other value will raise an error */
  if ((enable != 0) && (enable != 1))
  {
    return ret;
  }

  return TranslateErrorStatus(W61_OTA_starts(p_DrvObj, enable));
}

W6X_Status_t W6X_OTA_Finish(void)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  p_DrvObj = W61_ObjGet();
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  return TranslateErrorStatus(W61_OTA_Finish(p_DrvObj));
}

W6X_Status_t W6X_OTA_Send(uint8_t *buff, uint32_t len)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  p_DrvObj = W61_ObjGet();
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);
  NULL_ASSERT(buff, "buff not defined");

  if (len == 0)
  {
    return W6X_STATUS_OK;
  }

  return TranslateErrorStatus(W61_OTA_Send(p_DrvObj, buff, len));
}

W6X_Status_t W6X_OTA_Update_WiFi(char *http_server_addr, uint16_t http_server_port, const uint8_t *uri)
{
  int32_t sock = -1;
  W6X_Status_t ret = W6X_STATUS_ERROR;
  int32_t send_data = 0;
  /* Retry attempt on receiving socket data failure (that could be a timeout or data not pulled yet for example) */
  uint8_t retry_attempts = 0;
  /* Buffer that will receive chunk of data */
  uint8_t *buffer;
  size_t body_len = 0;
  /* Total amount of data received */
  uint32_t count = 0;
  /* Size of the data we should receive, indicated by the header response to our request */
  uint32_t content_to_receive = 0;
  /* What type of request we want to do */
  W6X_http_connection_t http_req_settings;
  /* We don't support proxy use */
  http_req_settings.use_proxy = 0;
  http_req_settings.req_type  = REQ_TYPE_GET;
  uint32_t auto_enable;
  uint32_t ps_mode;
  uint32_t dtim;

  buffer = pvPortMalloc(OTA_DATA_BUFFER_SIZE);
  if (buffer == NULL)
  {
    LogError("Failed to allocate buffer for OTA transmission");
    goto _err1;
  }

  /* Some verification of the passed parameters */
  if ((http_server_addr == NULL) || (uri == NULL))
  {
    LogError("Parameters of function W6X OTA update over WiFi are NULL or incorrect");
    goto _err2;
  }

  /* Save low power config */
  W6X_WiFi_GetDTIM(&dtim);
  W6X_GetPowerSaveAuto(&auto_enable);
  W6X_GetPowerMode(&ps_mode);

  /* Disable low power */
  W6X_SetPowerSaveAuto(0);
  W6X_SetPowerMode(0);
  W6X_WiFi_SetDTIM(0);

  /* Terminate OTA transmission on NCP side to ensure clear state */
  ret =  W6X_OTA_Starts(0);
  if (ret != W6X_STATUS_OK)
  {
    LogError("Failed to terminate the NCP OTA transmission, %d", ret);
    goto _err3;
  }

  /* Starts OTA on NCP side */
  ret =  W6X_OTA_Starts(1);
  if (ret != W6X_STATUS_OK)
  {
    LogError("Failed to start the NCP OTA , %d", ret);
    goto _err3;
  }

  /* Init the HTTP connection, it configures and connects a TCP client socket (HTTP client, that is us)
   * to a TCP server socket (HTTP server) */
  ret = W6X_HTTP_Client_Init(&sock, http_server_addr, http_server_port);
  if (ret != W6X_STATUS_OK)
  {
    goto _err3;
  }

  /* Set receive data to 0 */
  memset(buffer, 0x00, OTA_DATA_BUFFER_SIZE);

  /* Does a HTTP request, content_to_receive is greater than 0 if content to receive is expected.
   * buffer contains eventual HTTP body data already received with body_len the length of data.
   * buffer needs to be allocate with a size equal or greater than HTTP_HEAD_MAX_RESP_BUFFER_SIZE value */
  ret = W6X_HTTP_OTA_Client_Request(sock, http_server_addr, uri, &http_req_settings,
                                    &content_to_receive, buffer, &body_len);
  if (ret != W6X_STATUS_OK)
  {
    goto _err4;
  }

  /* If some of data from the HTTP body is already in the buffer/received,
   * we increment the counter of already received data */
  if (body_len > 0)
  {
    count += body_len;
  }

  /* Receive the first 4096 bytes this is done to retrieve the whole NCP binary header
   * before sending it to the NCP that checks for header integrity at first OTA send */
  while (count < OTA_DATA_BUFFER_SIZE)
  {
    send_data = W6X_Net_Recv(sock, buffer + count, OTA_DATA_BUFFER_SIZE - count, 0);
    /* Today == 0 value could be a timeout, so currently == 0 case is a valid data receive case */
    if (send_data >= 0)
    {
      count += send_data;
      LogDebug("Received %i of total %i first OTA send. Currently received %i\r\n",
               send_data, OTA_DATA_BUFFER_SIZE, count);
    }
    else
    {
      LogError("Failed to receive the first %i bytes", OTA_DATA_BUFFER_SIZE);
      ret = W6X_STATUS_ERROR;
      goto _err4;
    }
  }

  /* Process the first 4096 bytes */
  ret = W6X_OTA_Send(buffer, OTA_DATA_BUFFER_SIZE);
  if (ret != W6X_STATUS_OK)
  {
    LogError("Failed to send buffer to NCP via OTA send, error code : %d", ret);
    goto _err4;
  }
  count = OTA_DATA_BUFFER_SIZE;
  LogDebug("Size of OTA transmission:%i\r\n", OTA_DATA_BUFFER_SIZE);

  /* Receive the remaining data of the NCP binary and send it to the NCP */
  do
  {
    /* Receive data in chunk of size OTA_DATA_BUFFER_SIZE if possible */
    send_data = W6X_Net_Recv(sock, buffer, OTA_DATA_BUFFER_SIZE, 0);
    if (send_data > 0)
    {
      /* Send the data to the NCP for the OTA */
      count += send_data;
      ret =  W6X_OTA_Send(buffer, send_data);
      if (ret != W6X_STATUS_OK)
      {
        LogError("Failed to send buffer of %i bytes to NCP via OTA send, error code : %d", send_data, ret);
        goto _err4;
      }

      LogDebug("Size of OTA transmission:%i\r\n", send_data);
      /* If we received all the data we can end the loop */
      if (count >= content_to_receive)
      {
        break;
      }
    }
    else
    {
      retry_attempts++;
    }
    /* In case of error, we do some retry attempts before giving an error */
  } while (send_data > 0 || retry_attempts < OTA_RETRY_ATTEMPTS);

  /* Check if all the data has been transferred */
  if (count != content_to_receive)
  {
    ret = W6X_STATUS_ERROR;
    LogError("Unable to receive all the data");
    goto _err4;
  }

  /* De init the HTTP client, it closes the TCP socket */
  (void)W6X_HTTP_Client_DeInit(sock);

  /* Finish OTA on NCP side */
  ret =  W6X_OTA_Finish();
  if (ret != W6X_STATUS_OK)
  {
    LogError("Failed to finish OTA transmission, %d", ret);
    goto _err3;
  }

  /* Restore low power config */
  W6X_SetPowerSaveAuto(auto_enable);
  W6X_SetPowerMode(ps_mode);
  W6X_WiFi_SetDTIM(dtim);

  return W6X_STATUS_OK;

_err4:
  (void)W6X_HTTP_Client_DeInit(sock);
_err3:
  /* Restore low power config */
  W6X_SetPowerSaveAuto(auto_enable);
  W6X_SetPowerMode(ps_mode);
  W6X_WiFi_SetDTIM(dtim);
_err2:
  vPortFree(buffer);
_err1:
  return ret;
}

/** @} */

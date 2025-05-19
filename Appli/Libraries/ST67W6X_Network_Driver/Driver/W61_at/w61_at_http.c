/**
  ******************************************************************************
  * @file    w61_at_http.c
  * @author  GPM Application Team
  * @brief   This file provides code for W61 HTTP AT module
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
#include <stdio.h>
#include "w61_at_api.h"
#include "w61_at_common.h"
#include "w61_at_internal.h"
#include "w61_at_rx_parser.h"
#include "w61_io.h" /* SPI_XFER_MTU_BYTES */
#include "common_parser.h" /* Common Parser functions */

#if (SYS_DBG_ENABLE_TA4 >= 1)
#include "trcRecorder.h"
#endif /* SYS_DBG_ENABLE_TA4 */

/* Global variables ----------------------------------------------------------*/
/** @defgroup ST67W61_AT_HTTP_Variables ST67W61 AT Driver HTTP Variables
  * @ingroup ST67W61_AT_HTTP
  * @{
  */

/** Current mode of Get HTTP (0 active, 1 passive) */
uint32_t ActivePassiveMode = 0;

/** @} */

/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/** sscanf parsing error string */
static const char W61_Parsing_Error_str[] = "Parsing of the result failed";

/* Private function prototypes -----------------------------------------------*/
/** @addtogroup ST67W61_AT_HTTP_Functions
  * @{
  */

/**
  * @brief  Send AT command to get http data from a given URL
  * @param  Obj: pointer to module handle
  * @param  Url: URL to get data from
  * @param  Size: Size of the data to get
  * @param  Timeout: Timeout in ms
  * @return Operation Status.
  */
static W61_Status_t W61_HTTP_Client_Get(W61_Object_t *Obj, uint8_t *Url, uint32_t *Size, uint32_t Timeout);

/* Functions Definition ------------------------------------------------------*/
W61_Status_t W61_HTTP_GetMode(W61_Object_t *Obj, uint32_t *ActivePassive)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint8_t RespString[20];
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(ActivePassive);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+HTTPRECVMODE?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, RespString, Obj->NcpTimeout);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char const *) RespString, "+HTTPRECVMODE:%ld\r\n", ActivePassive) != 1)
      {
        LogError("%s", W61_Parsing_Error_str);
        ret = W61_STATUS_ERROR;
      }
      else
      {
        ret = W61_STATUS_OK;
      }

    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_HTTP_SetMode(W61_Object_t *Obj, uint32_t ActivePassive)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  ActivePassiveMode = ActivePassive;
  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+HTTPRECVMODE=%ld\r\n", ActivePassive);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Obj->NcpTimeout);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_HTTP_SetReceiveBufferLen(W61_Object_t *Obj, uint32_t Len)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+HTTPRECVBUF=%ld\r\n", Len);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Obj->NcpTimeout);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_HTTP_GetReceiveBufferLen(W61_Object_t *Obj, uint32_t *Len)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint8_t RespString[20];
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Len);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+HTTPRECVBUF?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, RespString, Obj->NcpTimeout);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char const *) RespString, "+HTTPRECVBUF:%ld\r\n", Len) != 1)
      {
        LogError("%s", W61_Parsing_Error_str);
        ret = W61_STATUS_ERROR;
      }
      else
      {
        ret = W61_STATUS_OK;
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_HTTP_Client_Get_URL(W61_Object_t *Obj, uint8_t *Url)
{
  return W61_STATUS_ERROR;
}

W61_Status_t W61_HTTP_Client_Set_URL(W61_Object_t *Obj, uint8_t *Url)
{
  return W61_STATUS_ERROR;
}

W61_Status_t W61_HTTP_Client_Get_Size(W61_Object_t *Obj, uint8_t *Url, uint32_t *Size, uint32_t Timeout)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Url);
  W61_NULL_ASSERT(Size);
  uint32_t dummy_linkid;

  if (ATlock(Obj, Timeout))
  {
    /* AT_Query timeout should let the time to NCP to return SEND:ERROR message */
    if (Timeout < W61_NET_TIMEOUT)
    {
      Timeout = W61_NET_TIMEOUT;
    }
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+HTTPGETSIZE=0,\"%s\"\r\n", (const char *)Url);
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, Timeout);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((const char *)Obj->CmdResp, "+HTTPGETSIZE::%ld,%ld\r\n", &dummy_linkid, Size) != 1)
      {
        LogError("%s", W61_Parsing_Error_str);
        ret = W61_STATUS_ERROR;
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  UNUSED(dummy_linkid);
  return ret;
}

W61_Status_t W61_HTTP_Client(W61_Object_t *Obj, uint8_t Option, uint8_t ContentType, uint8_t *Url, uint8_t *Data,
                             uint8_t *Buf, uint32_t Timeout)
{
  return W61_STATUS_ERROR;
}

W61_Status_t W61_HTTP_Client_Passive_Get(W61_Object_t *Obj, uint8_t *Url, uint32_t *Size, uint32_t Timeout)
{
  return W61_HTTP_Client_Get(Obj, Url, Size, Timeout);
}

W61_Status_t W61_HTTP_Client_Active_Get(W61_Object_t *Obj, uint8_t *Url, uint8_t *Buf, uint32_t BufLen,
                                        uint32_t Timeout)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint32_t data_len = 0;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Url);
  W61_NULL_ASSERT(Buf);

  Obj->HTTPCtx.AppBuffRecvData = Buf;
  Obj->HTTPCtx.AppBuffRecvDataSize = BufLen;

  ret = W61_HTTP_Client_Get(Obj, Url, &data_len, Timeout);
  if (data_len > BufLen)
  {
    return W61_STATUS_IO_ERROR;
  }

  return ret;
}

W61_Status_t W61_HTTP_AvailableDataLength(W61_Object_t *Obj, uint8_t ReqID, uint32_t *AvailableDataSize)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(AvailableDataSize);
  uint32_t dummy_linkid;

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+HTTPRECVLEN,0?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, Obj->NcpTimeout);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char const *) Obj->CmdResp, "+HTTPRECVLEN:%ld,%ld\r\n", &dummy_linkid, AvailableDataSize) != 1)
      {
        ret = W61_STATUS_ERROR;
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  UNUSED(dummy_linkid);
  return ret;
}

W61_Status_t W61_HTTP_PullDataFromSocket(W61_Object_t *Obj, uint8_t ReqID, uint32_t Reqlen, uint8_t *pData,
                                         uint32_t *Receivedlen, uint32_t Timeout)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t cmp = 0;
  int32_t recv_len;
  char *str_token;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(pData);
  W61_NULL_ASSERT(Receivedlen);

  *Receivedlen = 0;

  if (Reqlen > (SPI_XFER_MTU_BYTES - 32))  /* 32 margin for AT header size */
  {
    Reqlen = (SPI_XFER_MTU_BYTES - 32);
  }

  if (ATlock(Obj, Timeout))
  {
    /* AT_Query timeout should let the time to NCP to return SEND:ERROR message */
    if (Timeout < W61_NET_TIMEOUT)
    {
      Timeout = W61_NET_TIMEOUT;
    }
    Obj->HTTPCtx.AppBuffRecvData = pData;
    Obj->HTTPCtx.AppBuffRecvDataSize = Reqlen;
    /* snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+HTTPRECVDATA=%d,%ld\r\n", ReqID, Reqlen); */
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+HTTPRECVDATA=0,%ld\r\n", Reqlen);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Timeout);
      Obj->HTTPCtx.AppBuffRecvData = NULL;
      Obj->HTTPCtx.AppBuffRecvDataSize = 0;

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        cmp = strncmp((char *) Obj->CmdResp, "+HTTPRECVDATA:", sizeof("+HTTPRECVDATA:") - 1);
      }

      if (cmp == 0)
      {
        str_token = strtok((char *)(Obj->CmdResp + sizeof("+HTTPRECVDATA:") - 1), ",");
        if (str_token != NULL)
        {
          *Receivedlen = (uint32_t) ParseNumber(str_token, NULL);
        }
        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Obj->NcpTimeout);
        if ((recv_len > 0) && (recv_len <= strlen(AT_OK_STRING)))
        {
          Obj->CmdResp[recv_len] = 0;
          if (IS_STRING_OK)
          {
            ret = W61_STATUS_OK;
          }
        }
        else
        {
          ret = W61_STATUS_IO_ERROR;
        }
      }
      else
      {
        cmp = strncmp((char *) Obj->CmdResp + sizeof("+HTTPSTATUS:y,") - 1, "ERROR\r\n", sizeof("ERROR\r\n") - 1);
        if (cmp == 0)
        {
          (void)ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Obj->NcpTimeout);
          (void)ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Obj->NcpTimeout);
          ret = W61_STATUS_ERROR;
        }
        else
        {
          cmp = strncmp((char *) Obj->CmdResp, AT_ERROR_STRING, sizeof(AT_ERROR_STRING) - 1);
          if (cmp == 0)
          {
            ret = W61_STATUS_ERROR;
          }
          else
          {
            ret = W61_STATUS_IO_ERROR;
          }
        }
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_HTTP_Client_Put(W61_Object_t *Obj, uint8_t *Url, uint8_t *Data, uint8_t *Buf, uint32_t Timeout)
{
  return W61_STATUS_ERROR;
}

W61_Status_t W61_HTTP_Client_Post(W61_Object_t *Obj, uint8_t *Url, uint8_t *Data, uint8_t *Buf, uint32_t Timeout)
{
  return W61_STATUS_ERROR;
}

/* Private Functions Definition ----------------------------------------------*/
static W61_Status_t W61_HTTP_Client_Get(W61_Object_t *Obj, uint8_t *Url, uint32_t *Size, uint32_t Timeout)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t cmp = -1;
  int32_t recv_len;
  uint32_t status_offset;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Url);
  W61_NULL_ASSERT(Size);

  if (ATlock(Obj, Timeout))
  {
    /* AT_SetExecute timeout should let the time to NCP to return SEND:ERROR message */
    if (Timeout < W61_NET_TIMEOUT)
    {
      Timeout = W61_NET_TIMEOUT;
    }
    /* In case of Passive mode it gates only the size of the page,
       in case of Active mode it receives also the data : the rx_parser puts it in the given buffer */
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+HTTPCGET=0,\"%s\"\r\n", (char *)Url);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Obj->NcpTimeout);
    if (ret == W61_STATUS_OK)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Timeout);
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        cmp = strncmp((char *) Obj->CmdResp, "+HTTPC:", sizeof("+HTTPC:") - 1);
      }

      if (cmp == 0) /* if +HTTPC: is received (otherwise is HTTPSTATUS:ERROR) */
      {
        char *end = strstr((char *)Obj->CmdResp, ",");
        if (end)
        {
          *end = 0;
        }
        *Size = ParseNumber((char *) Obj->CmdResp + sizeof("+HTTPC:y,") - 1, NULL);

        /* receive the status message */
        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Obj->NcpTimeout);
      }

      /* Parse the HTTP status */
      if (recv_len > 0)
      {
        status_offset = sizeof("+HTTPSTATUS:y,") - 1;
        if (strncmp((char *)Obj->CmdResp + status_offset, "OK", sizeof("OK") - 1) == 0)
        {
          ret = W61_STATUS_OK;
        }
        /* Check if the response is ERROR */
        else if (strncmp((char *)Obj->CmdResp + status_offset, "ERROR", sizeof("ERROR") - 1) == 0)
        {
          LogError("ParseOkErr Status : %s", Obj->CmdResp);
          ret = W61_STATUS_ERROR;
        }
        else /* Unknown response */
        {
          LogError("ParseOkErr Status : %s", Obj->CmdResp);
          ret = W61_STATUS_IO_ERROR;
        }
      }
      else  /* timeout: status message not received */
      {
        LogError("Timeout: HTTP status message not received");
        ret = W61_STATUS_IO_ERROR;
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

/** @} */

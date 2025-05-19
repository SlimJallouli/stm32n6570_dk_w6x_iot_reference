/**
  ******************************************************************************
  * @file    w61_at_net.c
  * @author  GPM Application Team
  * @brief   This file provides code for W61 Net AT module
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
/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/** @addtogroup ST67W61_AT_Net_Constants
  * @{
  */

#if (!defined(W61_PING_REPETITION) || (W61_PING_REPETITION == 0))
#undef W61_PING_REPETITION
/** Number of ping repetition */
#define W61_PING_REPETITION                       4
#endif /* W61_PING_REPETITION */

#if (!defined(W61_PING_PACKET_SIZE) || (W61_PING_PACKET_SIZE == 0))
#undef W61_PING_PACKET_SIZE
/** Ping packet size */
#define W61_PING_PACKET_SIZE                      64
#endif /* W61_PING_PACKET_SIZE */

#if (!defined(W61_PING_INTERVAL) || (W61_PING_INTERVAL == 0))
#undef W61_PING_INTERVAL
/** Ping interval in ms */
#define W61_PING_INTERVAL                         1000
#endif /* W61_PING_INTERVAL */

/** Default ping interval in ms */
#define DEFAULT_PING_INTERVAL                     1000

/** Maximum ping interval in ms */
#define MAX_PING_INTERNAL                         3500

/** Minimum ping interval in ms */
#define MIN_PING_INTERNAL                         100

/** Maximum ping repetition */
#define MAX_PING_REPETITION                       65534

/** Maximum ping size */
#define MAX_PING_SIZE                             10000

/** Timeout for ping command */
#define PING_TIMEOUT                              20000

/** Timeout for start client command */
#define START_CLIENT_TIMEOUT                      5000

/** Size of AT Header data in bytes: +CIPRECVDATA:xxxx,"xxx.xxx.xxx.xxx",xxxxx, */
#define W61_AT_HEADER_DATA_SIZE                   64

#define W61_NET_EVT_SOCK_DATA_KEYWORD            "+IPD:"          /*!< Socket data event keyword */
#define W61_NET_EVT_SOCK_GLOBAL_KEYWORD          "+CIP:"          /*!< Socket global event keyword */
#define W61_NET_EVT_SOCK_CONNECTED_KEYWORD       "CONNECTED"      /*!< Socket connected keyword */
#define W61_NET_EVT_SOCK_DISCONNECTED_KEYWORD    "DISCONNECTED"   /*!< Socket disconnected keyword */

/** @} */

/* Private macros ------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Functions Definition ------------------------------------------------------*/
/** @addtogroup ST67W61_AT_Net_Functions
  * @{
  */

void AT_Net_Event(W61_Object_t *Obj, const uint8_t *rxbuf, int32_t rxbuf_len)
{
  char *ptr = (char *)rxbuf;
  int32_t socket_id = 0;
  W61_CbParamNetData_t  cb_param_net_data;

  if ((Obj == NULL) || (Obj->ulcbs.UL_net_cb == NULL))
  {
    return;
  }

  if (strncmp(ptr, W61_NET_EVT_SOCK_DATA_KEYWORD, sizeof(W61_NET_EVT_SOCK_DATA_KEYWORD) - 1) == 0)
  {
    ptr += sizeof(W61_NET_EVT_SOCK_DATA_KEYWORD) - 1;
    socket_id = ParseNumber(ptr, NULL);
    if (socket_id < 0 || socket_id > W61_NET_MAX_CONNECTIONS - 1)
    {
      return;
    }
    cb_param_net_data.socket_id = (uint8_t)socket_id;

    /* Move pointer after the socket ID */
    ptr += (sizeof("0,") - 1);

    /* Get the available data length */
    uint8_t offset = 0;
    cb_param_net_data.available_data_length = (uint32_t) ParseNumber(ptr, &offset);

    /* Get remote ip*/
    ptr += offset + (sizeof(",\"") - 1);              /* Skip the " that comes before the IP */
    cb_param_net_data.remote_ip[3] = (uint8_t) ParseNumber(ptr, &offset);
    ptr += offset + 1;
    cb_param_net_data.remote_ip[2] = (uint8_t) ParseNumber(ptr, &offset);
    ptr += offset + 1;
    cb_param_net_data.remote_ip[1] = (uint8_t) ParseNumber(ptr, &offset);
    ptr += offset + 1;
    cb_param_net_data.remote_ip[0] = (uint8_t) ParseNumber(ptr, &offset);
    ptr += offset + (sizeof("\",") - 1);                        /* Skip the " that comes after the IP */

    /* Get remote Port*/
    cb_param_net_data.remote_port = (uint16_t) ParseNumber(ptr, NULL);

    Obj->ulcbs.UL_net_cb(W61_NET_EVT_SOCK_DATA_ID, &cb_param_net_data);
    return;
  }

  if (strncmp(ptr, W61_NET_EVT_SOCK_GLOBAL_KEYWORD, sizeof(W61_NET_EVT_SOCK_GLOBAL_KEYWORD) - 1) == 0)
  {
    ptr += sizeof(W61_NET_EVT_SOCK_GLOBAL_KEYWORD) - 1;
    /* Get the socket ID (Multi socket mode always enabled) */
    socket_id = ParseNumber(ptr, 0);
    if (socket_id < 0 || socket_id > W61_NET_MAX_CONNECTIONS - 1)
    {
      return;
    }
    cb_param_net_data.socket_id = (uint8_t)socket_id;

    /* Move pointer after the socket ID */
    ptr += sizeof("0,") - 1;
    if (strncmp(ptr, W61_NET_EVT_SOCK_CONNECTED_KEYWORD, sizeof(W61_NET_EVT_SOCK_CONNECTED_KEYWORD) - 1) == 0)
    {
      Obj->ulcbs.UL_net_cb(W61_NET_EVT_SOCK_CONNECTED_ID, &cb_param_net_data);
    }
    else if (strncmp(ptr, W61_NET_EVT_SOCK_DISCONNECTED_KEYWORD,
                     sizeof(W61_NET_EVT_SOCK_DISCONNECTED_KEYWORD) - 1) == 0)
    {
      Obj->ulcbs.UL_net_cb(W61_NET_EVT_SOCK_DISCONNECTED_ID, &cb_param_net_data);
    }
    return;
  }
}

W61_Status_t W61_Net_Init(W61_Object_t *Obj)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  char *cmd_lst[3] =
  {
    "AT+CIPMUX=1\r\n",        /* Enable the multi socket mode */
    "AT+CIPRECVMODE=1\r\n",   /* Set PASSIVE receive mode */
    "AT+CIPDINFO=1\r\n"       /* Set IPD Verbose mode */
  };

  for (uint8_t i = 0; i < 3; i++)
  {
    if (ATlock(Obj, AT_LOCK_TIMEOUT))
    {
      ret = AT_SetExecute(Obj, (uint8_t *)cmd_lst[i], Obj->NcpTimeout);
      ATunlock(Obj);
    }
    else
    {
      ret = W61_STATUS_BUSY;
    }

    if (ret != W61_STATUS_OK)
    {
      goto _err;
    }
  }

_err:
  return ret;
}

W61_Status_t W61_Net_Ping(W61_Object_t *Obj, char *location, uint16_t length, uint16_t count, uint16_t interval,
                          uint32_t *average_time, uint16_t *received_response)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char *ping_response;
  int32_t recv_len;
  uint32_t total_time = 0;
  uint32_t ping;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(location);
  W61_NULL_ASSERT(average_time);
  W61_NULL_ASSERT(received_response);

  /*Handle default values */
  if (length == 0)
  {
    length = W61_PING_PACKET_SIZE;
  }
  /* If value is out of range, set to max value */
  if (length > MAX_PING_SIZE)
  {
    length = MAX_PING_SIZE;
  }

  if (count == 0)
  {
    count = W61_PING_REPETITION;
  }
  /* If value is out of range, set to max value */
  if (count > MAX_PING_REPETITION)
  {
    count = MAX_PING_REPETITION;
  }

#if ((W61_PING_INTERVAL < MIN_PING_INTERNAL) || (W61_PING_INTERVAL > MAX_PING_INTERNAL))
  /* If value is out of range, set to default value */
  interval = DEFAULT_PING_INTERVAL;
#else
  if ((interval < MIN_PING_INTERNAL) || (interval > MAX_PING_INTERNAL))
  {
    interval = W61_PING_INTERVAL;
  }
#endif /* W61_PING_INTERVAL */

  /* Set the received ping count */
  *received_response = 0;

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+PING=\"%s\",%d,%d,%d\r\n",
             location, length, count, interval);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      /* Parse the OK/ERROR response */
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, PING_TIMEOUT);
      if (recv_len > 0)
      {
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
      }

      if ((recv_len <= 0) || (ret != W61_STATUS_OK))
      {
        ATunlock(Obj);
        LogError("W61_Net_Ping: W61_STATUS_TIMEOUT. It can lead to unexpected behavior.");
        return W61_STATUS_TIMEOUT;
      }

      for (int32_t response_count = 0; response_count < count; response_count++)
      {
        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, PING_TIMEOUT + interval);

        if (recv_len > 0)
        {
          Obj->CmdResp[recv_len] = 0;
          ping_response = (char *)Obj->CmdResp;
          if (strncmp(ping_response, "+PING:", sizeof("+PING:") - 1) == 0)
          {
            if (strncmp(ping_response, "+PING:TIMEOUT", sizeof("+PING:TIMEOUT") - 1) != 0)
            {
              ping = (uint32_t) ParseNumber((char *)(ping_response + sizeof("+PING:") - 1), NULL);
              if (ping > 0)
              {
                total_time += ping;
                (*received_response)++;
              }
            }
          }
        }
        else
        {
          /* Since only open connections are displayed, status was read instead in the last occurrence
           * if all available connections are not used
           */
          break;
        }
      }
    }

    if (*received_response > 0)
    {
      *average_time = total_time / *received_response;
    }
    else
    {
      *average_time = 0;
    }

    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Net_DNS_LookUp(W61_Object_t *Obj, const char *url, uint8_t *ipaddress)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char *ptr;
  char *token;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(url);
  W61_NULL_ASSERT(ipaddress);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPDOMAIN=\"%s\"\r\n", url);
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_NET_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      ptr = strstr((char *)(Obj->CmdResp), "+CIPDOMAIN:");
      if (ptr == NULL)
      {
        ret = W61_STATUS_ERROR;
      }
      else
      {
        ptr += sizeof("+CIPDOMAIN:");
        token = strstr(ptr, "\r");
        if (token == NULL)
        {
          ret = W61_STATUS_ERROR;
        }
        else
        {
          *(--token) = 0;
          ParseIP(ptr, ipaddress);
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

W61_Status_t W61_Net_StartClientConnection(W61_Object_t *Obj, W61_Conn_t *conn)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char *type = "TCP";
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(conn);

  if (conn->Type == W61_TCP_CONNECTION)
  {
    type = "TCP";
  }
  else if (conn->Type == W61_UDP_CONNECTION)
  {
    type = "UDP";
  }
  else if (conn->Type == W61_TCP_SSL_CONNECTION)
  {
    type = "SSL";
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSTART=%d,\"%s\",\"" IPSTR "\",%d,%d\r\n",
             conn->Number, type, IP2STR(conn->RemoteIP), conn->RemotePort, conn->KeepAlive);
    ret = AT_SetExecute(Obj, Obj->CmdResp, START_CLIENT_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_Net_StopClientConnection(W61_Object_t *Obj, W61_Conn_t *conn)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(conn);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPCLOSE=%d\r\n", conn->Number);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_NET_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Net_StartServer(W61_Object_t *Obj, uint32_t Port, W61_ConnType_e Type, uint8_t ca_enable,
                                 uint32_t keepalive)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  char *type = "TCP";
  if (Type == W61_TCP_CONNECTION)
  {
    type = "TCP";
  }
  else if (Type == W61_UDP_CONNECTION)
  {
    type = "UDP";
  }
  else if (Type == W61_TCP_SSL_CONNECTION)
  {
    type = "SSL";
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSERVER=1,%ld,\"%s\",%d,%ld\r\n",
             Port, type, ca_enable, keepalive);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_NET_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Net_StopServer(W61_Object_t *Obj, uint8_t close_connections)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSERVER=0,%d\r\n", close_connections);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_NET_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Net_SendData(W61_Object_t *Obj, uint8_t Socket, uint8_t *pdata, uint32_t Reqlen,
                              uint32_t *SentLen, uint32_t Timeout)
{
  /* TODO: Do we want Timeout to be set by upper layer? (socket API don't pass a user Timeout)? */
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(pdata);
  W61_NULL_ASSERT(SentLen);

  if (Reqlen > SPI_XFER_MTU_BYTES)
  {
    Reqlen = SPI_XFER_MTU_BYTES;
  }

  *SentLen = Reqlen;

  if (ATlock(Obj, Timeout))
  {
    /* AT_SetExecute timeout should let the time to NCP to return SEND:ERROR message */
    if (Timeout < W61_NET_TIMEOUT)
    {
      Timeout = W61_NET_TIMEOUT;
    }
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSEND=%d,%ld\r\n", Socket, Reqlen);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Timeout);
    if (ret == W61_STATUS_OK)
    {
      ret = AT_RequestSendData(Obj, pdata, Reqlen, Timeout, true);
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  if (ret != W61_STATUS_OK)
  {
    *SentLen = 0;
  }

  return ret;
}

W61_Status_t W61_Net_SendData_Non_Connected(W61_Object_t *Obj, uint8_t Socket, char *IpAddress, uint32_t Port,
                                            uint8_t *pdata, uint32_t Reqlen, uint32_t *SentLen, uint32_t Timeout)
{
  /* TODO Do we want Timeout to be set by upper layer? (socket API don't pass a user Timeout)? */
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(IpAddress);
  W61_NULL_ASSERT(pdata);
  W61_NULL_ASSERT(SentLen);

  if (Reqlen > SPI_XFER_MTU_BYTES)
  {
    Reqlen = SPI_XFER_MTU_BYTES;
  }

  *SentLen = Reqlen;

  if (ATlock(Obj, Timeout))
  {
    /* AT_SetExecute timeout should let the time to NCP to return SEND:ERROR message */
    if (Timeout < W61_NET_TIMEOUT)
    {
      Timeout = W61_NET_TIMEOUT;
    }
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSEND=%d,%ld,\"%s\",%ld\r\n",
             Socket, Reqlen, IpAddress, Port);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Timeout);
    if (ret == W61_STATUS_OK)
    {
      ret = AT_RequestSendData(Obj, pdata, Reqlen, Timeout, true);
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  if (ret != W61_STATUS_OK)
  {
    *SentLen = 0;
  }

  return ret;
}

W61_Status_t W61_Net_SetReceiveBufferLen(W61_Object_t *Obj, uint8_t Socket, uint32_t BufLen)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPRECVBUF=%d,%ld\r\n", Socket, BufLen);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Obj->NcpTimeout);

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_GetReceiveBufferLen(W61_Object_t *Obj, uint8_t Socket, uint32_t *BufLen)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPRECVBUF=%d?\r\n", Socket);
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, Obj->NcpTimeout);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char const *) Obj->CmdResp, "+CIPRECVBUF:%ld\r\n", BufLen) != 1)
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

  return ret;
}

W61_Status_t W61_Net_IsDataAvailableOnSocket(W61_Object_t *Obj, uint8_t Socket, uint32_t *AvailableDataSize)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  int32_t len[5] = {0};   /* There are 5 comma in the string */
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(AvailableDataSize);

  if (Socket > 4)
  {
    return W61_STATUS_ERROR;
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPRECVLEN?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {

      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        if (sscanf((char *)Obj->CmdResp, "+CIPRECVLEN:%ld,%ld,%ld,%ld,%ld,",
                   &len[0], &len[1], &len[2], &len[3], &len[4]) != 5)
        {
          ATunlock(Obj);
          return W61_STATUS_ERROR;
        }
      }

      *AvailableDataSize = len[Socket];

      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      if ((recv_len > 0) && (recv_len <= strlen(AT_OK_STRING)))
      {
        Obj->CmdResp[recv_len] = 0;
        if (IS_STRING_OK)
        {
          ret = W61_STATUS_OK;
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

W61_Status_t W61_Net_PullDataFromSocket(W61_Object_t *Obj, uint8_t Socket, uint32_t Reqlen, uint8_t *pData,
                                        uint32_t *Receivedlen, uint32_t Timeout)
{
  /* TODO: should the Timeout given as parameter the max to keep the lock?
   * can this function return a status error follow by +CIPRECVDATA: (as in case of HTTPRECVDATA)? */
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(pData);
  W61_NULL_ASSERT(Receivedlen);

  *Receivedlen = 0;
  if (Reqlen > (SPI_XFER_MTU_BYTES - W61_AT_HEADER_DATA_SIZE))
  {
    Reqlen = (SPI_XFER_MTU_BYTES - W61_AT_HEADER_DATA_SIZE);
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    Obj->NetCtx.AppBuffRecvData = pData;
    Obj->NetCtx.AppBuffRecvDataSize = Reqlen;
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPRECVDATA=%d,%ld\r\n", Socket, Reqlen);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Timeout);
      Obj->NetCtx.AppBuffRecvData = NULL;
      Obj->NetCtx.AppBuffRecvDataSize = 0;

      if (recv_len > 0)
      {
        if (sscanf((char *)Obj->CmdResp, "+CIPRECVDATA:%ld", Receivedlen) != 1)
        {
          ATunlock(Obj);
          return ret;
        }
      }

      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Timeout);
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
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Net_GetServerMaxConnections(W61_Object_t *Obj, uint8_t *MaxConnections)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(MaxConnections);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSERVERMAXCONN?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        if (strncmp((char *) Obj->CmdResp, "+CIPSERVERMAXCONN:", sizeof("+CIPSERVERMAXCONN:") - 1) == 0)
        {
          *MaxConnections = (uint32_t) ParseNumber((char *) Obj->CmdResp + sizeof("+CIPSERVERMAXCONN:") - 1, NULL);
          ret = W61_STATUS_OK;
        }

        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
        if (recv_len > 0)
        {
          Obj->CmdResp[recv_len] = 0;
          ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_SetServerMaxConnections(W61_Object_t *Obj, uint8_t MaxConnections)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSERVERMAXCONN=%d\r\n", MaxConnections);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_GetSNTPConfiguration(W61_Object_t *Obj, uint8_t *Enable, int16_t *Timezone,
                                          uint8_t *SntpServer1, uint8_t *SntpServer2, uint8_t *SntpServer3)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t cmp = -1;
  int32_t i;
  int32_t recv_len;
  char *parameters_strings[5];
  char *end;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Enable);
  W61_NULL_ASSERT(Timezone);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSNTPCFG?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        cmp = strncmp((char *) Obj->CmdResp, "+CIPSNTPCFG:", sizeof("+CIPSNTPCFG:") - 1);
      }
      if (cmp == 0)
      {
        end = strstr((char *)Obj->CmdResp, "\r");
        if (end)
        {
          *end = 0;
        }
        parameters_strings[0] = strtok((char *) Obj->CmdResp + sizeof("+CIPSNTPCFG:") - 1, ",");
        *Enable = (uint8_t) ParseNumber(parameters_strings[0], NULL);
        if (*Enable == 1)
        {
          for (i = 1; i < 5; i++)
          {
            parameters_strings[i] = strtok(NULL, ",");
            if (parameters_strings[i] == NULL)
            {
              break;
            }
          }
          *Timezone = (int16_t) ParseNumber(parameters_strings[1], NULL);
          if (SntpServer1 != NULL)
          {
            if ((parameters_strings[2] != NULL) && (strlen(parameters_strings[2]) >= 2))
            {
              strncpy((char *)SntpServer1, &(parameters_strings[2][1]), strlen(parameters_strings[2]) - 2);
              SntpServer1[strlen(parameters_strings[2]) - 2] = '\0';
            }
            else
            {
              SntpServer1[0] = '\0';
            }
          }

          if (SntpServer2 != NULL)
          {
            if ((parameters_strings[3] != NULL) && (strlen(parameters_strings[3]) >= 2))
            {
              strncpy((char *)SntpServer2, &(parameters_strings[3][1]), strlen(parameters_strings[3]) - 2);
              SntpServer2[strlen(parameters_strings[3]) - 2] = '\0';
            }
            else
            {
              SntpServer2[0] = '\0';
            }
          }

          if (SntpServer3 != NULL)
          {
            if ((parameters_strings[4] != NULL) && (strlen(parameters_strings[4]) >= 2))
            {
              strncpy((char *)SntpServer3, &(parameters_strings[4][1]), strlen(parameters_strings[4]) - 2);
              SntpServer3[strlen(parameters_strings[4]) - 2] = '\0';
            }
            else
            {
              SntpServer3[0] = '\0';
            }
          }
        }
      }

      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_SetSNTPConfiguration(W61_Object_t *Obj, uint8_t Enable, int16_t Timezone, uint8_t *SntpServer1,
                                          uint8_t *SntpServer2, uint8_t *SntpServer3)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  int32_t len;
  W61_NULL_ASSERT(Obj);

  if ((SntpServer1 == NULL) && (SntpServer2 == NULL) && (SntpServer3 == NULL))
  {
    LogError("SNTP servers URL missing");
    return ret;
  }

  len = snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSNTPCFG=%d,%d", Enable, Timezone);

  if (SntpServer1 != NULL)
  {
    len += snprintf((char *)&Obj->CmdResp[len], W61_ATD_CMDRSP_STRING_SIZE - len, ",\"%s\"", SntpServer1);
  }

  if (SntpServer2 != NULL)
  {
    len += snprintf((char *)&Obj->CmdResp[len], W61_ATD_CMDRSP_STRING_SIZE - len, ",\"%s\"", SntpServer2);
  }

  if (SntpServer3 != NULL)
  {
    len += snprintf((char *)&Obj->CmdResp[len], W61_ATD_CMDRSP_STRING_SIZE - len, ",\"%s\"", SntpServer3);
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)&Obj->CmdResp[len], W61_ATD_CMDRSP_STRING_SIZE - len, "\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_GetSNTPTime(W61_Object_t *Obj, uint8_t *Time)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t cmp = -1;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Time);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSNTPTIME?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        cmp = strncmp((char *) Obj->CmdResp, "+CIPSNTPTIME:", sizeof("+CIPSNTPTIME:") - 1);
      }

      if (cmp == 0)
      {
        strncpy((char *)Time, (char *)Obj->CmdResp + strlen("+CIPSNTPTIME:"),
                strlen((char *)Obj->CmdResp) - strlen("+CIPSNTPTIME:") - 2);
        Time[strlen((char *)Obj->CmdResp) - strlen("+CIPSNTPTIME:") - 2] = '\0';
        ret = W61_STATUS_OK;
      }
      else
      {
        Time[0] = '\0';
      }

      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_GetSNTPInterval(W61_Object_t *Obj, uint16_t *Interval)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t cmp = -1;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Interval);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSNTPINTV?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        cmp = strncmp((char *) Obj->CmdResp, "+CIPSNTPINTV:", sizeof("+CIPSNTPINTV:") - 1);
      }
      if (cmp == 0)
      {
        *Interval = (uint16_t) ParseNumber((char *) Obj->CmdResp + sizeof("+CIPSNTPINTV:") - 1, NULL);

        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
        if (recv_len > 0)
        {
          Obj->CmdResp[recv_len] = 0;
          ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_SetSNTPInterval(W61_Object_t *Obj, uint16_t Interval)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSNTPINTV=%d\r\n", Interval);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_GetTCPOpt(W61_Object_t *Obj, uint8_t Socket, int16_t *Linger, uint16_t *TcpNoDelay,
                               uint16_t *SoSndTimeout, uint16_t *KeepAlive)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  int32_t cmp;
  uint32_t parameters[5];
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Linger);
  W61_NULL_ASSERT(TcpNoDelay);
  W61_NULL_ASSERT(SoSndTimeout);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPTCPOPT?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      for (int32_t cur_conn = 0; cur_conn < 5; cur_conn++)
      {
        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
        if (recv_len > 0)
        {
          cmp = sscanf((char *)Obj->CmdResp, "+CIPTCPOPT:%ld,%ld,%ld,%ld,%ld",
                       &parameters[0], &parameters[1], &parameters[2], &parameters[3], &parameters[4]);

          if (cmp == 0)
          {
            break; /* No more connection */
          }

          if (cmp != 5) /*Parameters are missing */
          {
            ATunlock(Obj);
            return W61_STATUS_ERROR;
          }

          if (parameters[0] == Socket)
          {
            *Linger = (int16_t) parameters[1];
            *TcpNoDelay = (uint16_t) parameters[2];
            *SoSndTimeout = (uint16_t) parameters[3];
            *KeepAlive = (uint16_t) parameters[4];
            ret = W61_STATUS_OK;
          }
        }
      }

      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_SetTCPOpt(W61_Object_t *Obj, uint8_t Socket, int16_t Linger, uint16_t TcpNoDelay,
                               uint16_t SoSndTimeout, uint16_t KeepAlive)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPTCPOPT=%d,%d,%d,%d,%d\r\n",
             Socket, Linger, TcpNoDelay, SoSndTimeout, KeepAlive);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_GetSocketInformation(W61_Object_t *Obj, uint8_t Socket, uint8_t *Protocol, uint8_t *RemoteIp,
                                          uint32_t *RemotePort, uint32_t *LocalPort, uint8_t *Type)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t cmp;
  int32_t recv_len;
  uint8_t read_status = 0;
  uint32_t local_socket = 0;
  char local_protocol[8];
  char remote_ip[24];
  uint32_t remote_port;
  uint32_t local_port;
  uint32_t tetype;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Protocol);
  W61_NULL_ASSERT(RemoteIp);
  W61_NULL_ASSERT(RemotePort);
  W61_NULL_ASSERT(LocalPort);
  W61_NULL_ASSERT(Type);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSTATE?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      for (int32_t cur_conn = 0; cur_conn < 5; cur_conn++)
      {
        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
        if (recv_len > 0)
        {
          cmp = sscanf((const char *)Obj->CmdResp,
                       "+CIPSTATUS:%ld,\"%[^\"]\",\"%[^\"]\",%ld,%ld,%ld\r\n",
                       &local_socket, local_protocol, remote_ip, &remote_port, &local_port, &tetype);
          if (cmp == 6)
          {
            if (local_socket == Socket)
            {
              memcpy((char *)Protocol, local_protocol, strlen(local_protocol) + 1);
              memcpy((char *)RemoteIp, remote_ip, strlen(remote_ip) + 1);
              *RemotePort = remote_port;
              *LocalPort = local_port;
              *Type = tetype;
            }
          }
          else
          {
            /* Since only open connections are displayed, status was read instead in the last occurrence
             * if all available connections are not used
             */
            read_status = 1;
            break;
          }
        }
      }
      if (read_status == 0)
      {
        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      }
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_GetSSLConfiguration(W61_Object_t *Obj, uint8_t Socket, uint8_t *AuthMode, uint8_t *Certificate,
                                         uint8_t *PrivateKey, uint8_t *CaCertificate)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t cmp = -1;
  int32_t recv_len;
  int32_t i;
  char *parameters_strings[5];
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(AuthMode);
  W61_NULL_ASSERT(Certificate);
  W61_NULL_ASSERT(PrivateKey);
  W61_NULL_ASSERT(CaCertificate);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSSLCCONF=%d?\r\n", Socket);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        cmp = strncmp((char *) Obj->CmdResp, "+CIPSSLCCONF:", sizeof("+CIPSSLCCONF:") - 1);
      }
      if (cmp == 0)
      {
        char *end = strstr((char *)Obj->CmdResp, "\r");
        if (end)
        {
          *end = 0;
        }
        parameters_strings[0] = strtok((char *) Obj->CmdResp + sizeof("+CIPSSLCCONF:") - 1, ",");
        for (i = 1; i < 5; i++)
        {
          parameters_strings[i] = strtok(NULL, ",");
          if (parameters_strings[i] == NULL)
          {
            break;
          }
        }
        strtok((char *)Obj->CmdResp, "\r\n");

        *AuthMode = (uint8_t)ParseNumber(parameters_strings[1], NULL);
        if ((parameters_strings[2] != NULL) && (strlen(parameters_strings[2]) >= 2))
        {
          strncpy((char *)Certificate, &(parameters_strings[2][1]), strlen(parameters_strings[2]) - 2);
          Certificate[strlen(parameters_strings[2]) - 2] = '\0';
        }
        else
        {
          Certificate[0] = '\0';
        }

        if ((parameters_strings[3] != NULL) && (strlen(parameters_strings[3]) >= 2))
        {
          strncpy((char *)PrivateKey, &(parameters_strings[3][1]), strlen(parameters_strings[3]) - 2);
          PrivateKey[strlen(parameters_strings[3]) - 2] = '\0';
        }
        else
        {
          PrivateKey[0] = '\0';
        }

        if ((parameters_strings[4] != NULL) && (strlen(parameters_strings[4]) >= 2))
        {
          strncpy((char *)CaCertificate, &(parameters_strings[4][1]), strlen(parameters_strings[4]) - 2);
          CaCertificate[strlen(parameters_strings[4]) - 2] = '\0';
        }
        else
        {
          CaCertificate[0] = '\0';
        }

        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
        if (recv_len > 0)
        {
          Obj->CmdResp[recv_len] = 0;
          ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_SetSSLConfiguration(W61_Object_t *Obj, uint8_t Socket, uint8_t AuthMode, uint8_t *Certificate,
                                         uint8_t *PrivateKey, uint8_t *CaCertificate)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);
  if (!Certificate)
  {
    Certificate = (uint8_t *)"";
  }
  if (!PrivateKey)
  {
    PrivateKey = (uint8_t *)"";
  }
  if (!CaCertificate)
  {
    CaCertificate = (uint8_t *)"";
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSSLCCONF=%d,%d,\"%s\",\"%s\",\"%s\"\r\n", Socket,
             AuthMode, Certificate, PrivateKey, CaCertificate);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_GetSSLServerName(W61_Object_t *Obj, uint8_t Socket, uint8_t *SslSni)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t cmp = -1;
  int32_t recv_len;
  char *parameters_strings[2];
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(SslSni);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSSLCSNI=%d?\r\n", Socket);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        cmp = strncmp((char *) Obj->CmdResp, "+CIPSSLCSNI:", sizeof("+CIPSSLCSNI:") - 1);
      }
      if (cmp == 0)
      {
        char *end = strstr((char *)Obj->CmdResp, "\r");
        if (end)
        {
          *end = 0;
        }
        parameters_strings[0] = strtok((char *) Obj->CmdResp + sizeof("+CIPSSLCSNI:") - 1, ",");
        parameters_strings[1] = strtok(NULL, ",");
        strtok((char *)Obj->CmdResp, "\r\n");

        if ((parameters_strings[1] != NULL) && (strlen(parameters_strings[1]) >= 2))
        {
          strncpy((char *)SslSni, &(parameters_strings[1][1]), strlen(parameters_strings[1]) - 2);
          SslSni[strlen(parameters_strings[1]) - 2] = '\0';
        }
        else
        {
          SslSni[0] = '\0';
        }

        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
        if (recv_len > 0)
        {
          Obj->CmdResp[recv_len] = 0;
          ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_SetSSLServerName(W61_Object_t *Obj, uint8_t Socket, uint8_t *SslSni)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(SslSni);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSSLCSNI=%d,\"%s\"\r\n", Socket, SslSni);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_GetSSLApplicationLayerProtocol(W61_Object_t *Obj, uint8_t Socket, uint8_t *Alpn1,
                                                    uint8_t *Alpn2, uint8_t *Alpn3)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t cmp = -1;
  int32_t recv_len;
  int32_t i;
  char *parameters_strings[4];
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Alpn1);
  W61_NULL_ASSERT(Alpn2);
  W61_NULL_ASSERT(Alpn3);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSSLCALPN=%d?\r\n", Socket);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        cmp = strncmp((char *) Obj->CmdResp, "+CIPSSLCALPN:", sizeof("+CIPSSLCALPN:") - 1);
      }
      if (cmp == 0)
      {
        char *end = strstr((char *)Obj->CmdResp, "\r");
        if (end)
        {
          *end = 0;
        }
        parameters_strings[0] = strtok((char *) Obj->CmdResp + sizeof("+CIPSSLCALPN:") - 1, ",");
        if (parameters_strings[0] != NULL)
        {
          for (i = 1; i < 4; i++)
          {
            parameters_strings[i] = strtok(NULL, ",");
            if (parameters_strings[i] == NULL)
            {
              break;
            }
          }
          if ((parameters_strings[1] != NULL) && (strlen(parameters_strings[1]) >= 2))
          {
            strncpy((char *)Alpn1, &(parameters_strings[1][1]), strlen(parameters_strings[1]) - 2);
            Alpn1[strlen(parameters_strings[1]) - 2] = '\0';

            if ((parameters_strings[2] != NULL) && (strlen(parameters_strings[2]) >= 2))
            {
              strncpy((char *)Alpn2, &(parameters_strings[2][1]), strlen(parameters_strings[2]) - 2);
              Alpn2[strlen(parameters_strings[2]) - 2] = '\0';

              if ((parameters_strings[3] != NULL) && (strlen(parameters_strings[3]) >= 2))
              {
                strncpy((char *)Alpn3, &(parameters_strings[3][1]), strlen(parameters_strings[3]) - 2);
                Alpn3[strlen(parameters_strings[3]) - 2] = '\0';
              }
            }
          }
        }

        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
        if (recv_len > 0)
        {
          Obj->CmdResp[recv_len] = 0;
          ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_SetSSLALPN(W61_Object_t *Obj, uint8_t Socket, uint8_t *Alpn1,
                                uint8_t *Alpn2, uint8_t *Alpn3)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  uint8_t alpn_count = 0;
  char alpns[50] = {0};
  int32_t size = 0;
  int32_t offset = 0;
  W61_NULL_ASSERT(Obj);

  if ((Alpn1 != NULL) && (strlen((char *)Alpn1) > 0))
  {
    size = snprintf(&alpns[offset], sizeof(alpns) - offset, ",\"%s\"", Alpn1);
    if (size > 0)
    {
      offset += size;
      alpn_count++;
    }
    else
    {
      goto _err;
    }
  }

  if ((Alpn2 != NULL) && (strlen((char *)Alpn2) > 0))
  {
    size = snprintf(&alpns[offset], sizeof(alpns) - offset, ",\"%s\"", Alpn2);
    if (size > 0)
    {
      offset += size;
      alpn_count++;
    }
    else
    {
      goto _err;
    }
  }

  if ((Alpn3 != NULL) && (strlen((char *)Alpn3) > 0))
  {
    size = snprintf(&alpns[offset], sizeof(alpns) - offset, ",\"%s\"", Alpn3);
    if (size > 0)
    {
      offset += size;
      alpn_count++;
    }
    else
    {
      goto _err;
    }
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSSLCALPN=%d,%d%s\r\n",
             Socket, alpn_count, alpns);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

_err:
  return ret;
}

W61_Status_t W61_Net_GetSSLPSK(W61_Object_t *Obj, uint8_t Socket, uint8_t *Psk, uint8_t *Hint)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t cmp = -1;
  int32_t recv_len;
  char *parameters_strings[2];
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Psk);
  W61_NULL_ASSERT(Hint);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSSLCPSK=%d?\r\n", Socket);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        cmp = strncmp((char *) Obj->CmdResp, "+CIPSSLCPSK:", sizeof("+CIPSSLCPSK:") - 1);
      }

      if (cmp == 0)
      {
        char *end = strstr((char *)Obj->CmdResp, "\r");
        if (end)
        {
          *end = 0;
        }

        parameters_strings[0] = strtok((char *) Obj->CmdResp + sizeof("+CIPSSLPSK:X,"), ",");
        if ((parameters_strings[0] != NULL) && (strlen(parameters_strings[0]) >= 2))
        {
          strncpy((char *)Psk, &(parameters_strings[0][1]), strlen(parameters_strings[0]) - 2);
          Psk[strlen(parameters_strings[0]) - 2] = '\0';

          parameters_strings[1] = strtok(NULL, ",");
          if ((parameters_strings[1] != NULL) && (strlen(parameters_strings[1])  >= 2))
          {
            strncpy((char *)Hint, &(parameters_strings[1][1]), strlen(parameters_strings[1]) - 2);
            Hint[strlen(parameters_strings[1]) - 2] = '\0';
          }
        }

        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
        if (recv_len > 0)
        {
          Obj->CmdResp[recv_len] = 0;
          ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

W61_Status_t W61_Net_SetSSLPSK(W61_Object_t *Obj, uint8_t Socket, uint8_t *Psk, uint8_t *Hint)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Psk);
  W61_NULL_ASSERT(Hint);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSSLCPSK=%d,\"%s\",\"%s\"\r\n",
             Socket, Psk, Hint);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_NET_TIMEOUT);
      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
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

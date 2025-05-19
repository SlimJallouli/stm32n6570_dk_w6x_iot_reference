/**
  ******************************************************************************
  * @file    w6x_http.c
  * @author  GPM Application Team
  * @brief   This file provides code for W6x HTTP API and a simple HTTP client API
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

/*
 * Portions of this file are based on LwIP's http client example application, LwIP version is 2.2.0.
 * Which is licensed under modified BSD-3 Clause license as indicated below.
 * See https://savannah.nongnu.org/projects/lwip/ for more information.
 *
 * Reference source :
 * https://github.com/lwip-tcpip/lwip/blob/master/src/apps/http/http_client.c
 *
 */

/*
 * Copyright (c) 2018 Simon Goldschmidt <goldsimon@gmx.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Simon Goldschmidt <goldsimon@gmx.de>
 *
 */

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "w6x_types.h"
#include "w6x_api.h"       /* Prototypes of the functions implemented in this file */
#include "w6x_internal.h"
#include "w61_at_api.h"    /* Prototypes of the functions called by this file */
#include "w61_io.h"        /* Prototypes of the BUS functions to be registered */
#include "common_parser.h" /* Common Parser functions */
#include "w6x_default_config.h"    /* HTTP_PASSIVE_RECV_BUFFER_SIZE */

/* Global variables ----------------------------------------------------------*/
/** @defgroup ST67W6X_Private_HTTP_Variables ST67W6X HTTP Variables
  * @ingroup  ST67W6X_Private_HTTP
  * @{
  */
/** HTTP mode used by the NCP */
W6X_http_mode_e W6X_HTTP_mode = W6X_HTTP_MODE_ACTIVE;

/** @} */

/* Private typedef -----------------------------------------------------------*/
/** @defgroup ST67W6X_Private_HTTP_Types ST67W6X HTTP Types
  * @ingroup  ST67W6X_Private_HTTP
  * @{
  */

/**
  * @brief  HTTP request task context structure
  */
struct http_request_task_obj
{
  int32_t sock;                         /*!< Socket used for the HTTP request */
  char *uri;                            /*!< URI to request */
  ip_addr_t server;                     /*!< Server IP address */
  char method[6];                       /*!< HTTP method to use */
  void *post_data;                      /*!< Data to post */
  size_t post_data_len;                 /*!< Length of the data to post */
  struct httpc_connection settings_t;   /*!< HTTP connection settings */
};

/**
  * @brief  Define generic categories for HTTP response codes
  */
typedef enum
{
  HTTP_CATEGORY_UNKNOWN = 0,
  HTTP_CATEGORY_INFORMATIONAL = 1,
  HTTP_CATEGORY_SUCCESS = 2,
  HTTP_CATEGORY_REDIRECTION = 3,
  HTTP_CATEGORY_CLIENT_ERROR = 4,
  HTTP_CATEGORY_SERVER_ERROR = 5
} HttpStatusCategory;

/** @} */

/* Private defines -----------------------------------------------------------*/
/** @defgroup ST67W6X_Private_HTTP_Constants ST67W6X HTTP Constants
  * @ingroup  ST67W6X_Private_HTTP
  * @{
  */

#ifndef W6X_HTTP_CLIENT_THREAD_STACK_SIZE
/** HTTP Client thread stack size */
#define W6X_HTTP_CLIENT_THREAD_STACK_SIZE   4096
#endif /* W6X_HTTP_CLIENT_THREAD_STACK_SIZE */

#ifndef W6X_HTTP_CLIENT_THREAD_PRIO
/** HTTP Client thread priority */
#define W6X_HTTP_CLIENT_THREAD_PRIO         30
#endif /* W6X_HTTP_CLIENT_THREAD_PRIO */

/** Size of an IPv4 array  */
#define SERVER_ADDR_SIZE                    4

/** Maximum size of accepted HTTP response buffer */
#define HEAD_MAX_RESP_BUFFER_SIZE           2048

/** Maximum size of the HTTP request buffer */
#define REQUEST_HEAD_MAX_ACCEPTED_SIZE      1024

/** Size of the TCP socket used by the HTTP client, recommended to be at least 0x2000 when fetching lots of data.
  * 0x2000 is the value used in the SPI host project for OTA update, which retrieves around 1 mega bytes of data.*/
#define HTTP_CLIENT_TCP_SOCKET_SIZE         0x3000

/** Timeout value in millisecond for receiving data via TCP socket used by the HTTP client.
  * This value is set to compensate for when the NCP get stuck for a long time (1 second or more)
  * when retrieving data from an HTTP server for example */
#define HTTP_CLIENT_TCP_SOCK_RECV_TIMEOUT   1000

/** HTTP Client custom version string */
#define CUSTOM_VERSION_STRING               "1.0"

/** HTTP client agent string */
#define HTTPC_CLIENT_AGENT                  "w6x/" CUSTOM_VERSION_STRING

/** @} */

/* Private macros ------------------------------------------------------------*/
/** @defgroup ST67W6X_Private_HTTP_Macros ST67W6X HTTP Macros
  * @ingroup  ST67W6X_Private_HTTP
  * @{
  */

/** HEAD request with host */
#define HTTPC_REQ_HEAD_11_HOST                                                         \
  "HEAD %s HTTP/1.1\r\n"  /* URI */                                                    \
  "User-Agent: %s\r\n"    /* User-Agent */                                             \
  "Accept: */*\r\n"                                                                    \
  "Host: %s\r\n"          /* server name */                                            \
  "Connection: Close\r\n" /* we don't support persistent connections, yet */           \
  "\r\n"

/** HEAD request with host format */
#define HTTPC_REQ_HEAD_11_HOST_FORMAT(uri, srv_name) \
  HTTPC_REQ_HEAD_11_HOST, uri, HTTPC_CLIENT_AGENT, srv_name

/** HEAD request basic */
#define HTTPC_REQ_HEAD_11                                                              \
  "HEAD %s HTTP/1.1\r\n"  /* URI */                                                    \
  "User-Agent: %s\r\n"    /* User-Agent */                                             \
  "Accept: */*\r\n"                                                                    \
  "Connection: Close\r\n" /* we don't support persistent connections, yet */           \
  "\r\n"

/** HEAD request basic format */
#define HTTPC_REQ_HEAD_11_FORMAT(uri) HTTPC_REQ_HEAD_11, uri, HTTPC_CLIENT_AGENT

/** GET request basic */
#define HTTPC_REQ_10                                                                   \
  "GET %s HTTP/1.0\r\n"   /* URI */                                                    \
  "User-Agent: %s\r\n"    /* User-Agent */                                             \
  "Accept: */*\r\n"                                                                    \
  "Connection: Close\r\n" /* we don't support persistent connections, yet */           \
  "\r\n"

/** GET request basic format */
#define HTTPC_REQ_10_FORMAT(uri) HTTPC_REQ_10, uri, HTTPC_CLIENT_AGENT

/** GET request basic simple */
#define HTTPC_REQ_10_SIMPLE                                                            \
  "GET %s HTTP/1.0\r\n" /* URI */                                                      \
  "\r\n"

/** GET request basic simple format */
#define HTTPC_REQ_10_SIMPLE_FORMAT(uri) HTTPC_REQ_10_SIMPLE, uri

/** GET request with host */
#define HTTPC_REQ_11_HOST                                                              \
  "GET %s HTTP/1.1\r\n"   /* URI */                                                    \
  "User-Agent: %s\r\n"    /* User-Agent */                                             \
  "Accept: */*\r\n"                                                                    \
  "Host: %s\r\n"          /* server name */                                            \
  "Connection: Close\r\n" /* we don't support persistent connections, yet */           \
  "\r\n"

/** GET request with host format */
#define HTTPC_REQ_11_HOST_FORMAT(uri, srv_name) \
  HTTPC_REQ_11_HOST, uri, HTTPC_CLIENT_AGENT, srv_name

/** GET request with proxy */
#define HTTPC_REQ_11_PROXY                                                             \
  "GET http://%s%s HTTP/1.1\r\n" /* HOST, URI */                                       \
  "User-Agent: %s\r\n"           /* User-Agent */                                      \
  "Accept: */*\r\n"                                                                    \
  "Host: %s\r\n"                 /* server name */                                     \
  "Connection: Close\r\n"        /* we don't support persistent connections, yet */    \
  "\r\n"

/** GET request with proxy format */
#define HTTPC_REQ_11_PROXY_FORMAT(host, uri, srv_name) \
  HTTPC_REQ_11_PROXY, host, uri, HTTPC_CLIENT_AGENT, srv_name

/** GET request with proxy (non-default server port) */
#define HTTPC_REQ_11_PROXY_PORT                                                        \
  "GET http://%s:%d%s HTTP/1.1\r\n" /* HOST, host-port, URI */                         \
  "User-Agent: %s\r\n"              /* User-Agent */                                   \
  "Accept: */*\r\n"                                                                    \
  "Host: %s\r\n"                    /* server name */                                  \
  "Connection: Close\r\n"           /* we don't support persistent connections, yet */ \
  "\r\n"

/** GET request with proxy (non-default server port) format */
#define HTTPC_REQ_11_PROXY_PORT_FORMAT(host, host_port, uri, srv_name) \
  HTTPC_REQ_11_PROXY_PORT, host, host_port, uri, HTTPC_CLIENT_AGENT, srv_name

/** @} */

/* Private variables ---------------------------------------------------------*/
/** @defgroup ST67W6X_Private_HTTP_Variables ST67W6X HTTP Variables
  * @ingroup  ST67W6X_Private_HTTP
  * @{
  */
static W61_Object_t  *p_DrvObj; /*!< Global W61 context pointer */

static uint32_t NcpHttpBufferSize; /*!< Size of the HTTP buffer used by the NCP */
/** @} */

/* Private function prototypes -----------------------------------------------*/
/** @defgroup ST67W6X_Private_HTTP_Functions ST67W6X HTTP Functions
  * @ingroup  ST67W6X_Private_HTTP
  * @{
  */

/**
  * @brief  Function to get the category of a status code
  * @param  status: status code defined by HttpStatusCode
  * @return category of the status code, list of possible value are defined by HttpStatusCategory
  */
static inline HttpStatusCategory W6X_HTTP_Client_Get_Status_Category(HttpStatusCode status);

/**
  * @brief  Create an HTTP client request stored in buff of size buff_size by using uri and http connection settings
  * @param  buff: buffer to store the request
  * @param  buff_size: size of the buffer
  * @param  server_name: server name to request
  * @param  uri: uri to request
  * @param  settings: http connection settings
  * @return size of the request if success, negative value or 0 otherwise
  * @note   Currently only GET request is supported
  */
static int32_t W6X_HTTP_Client_Create_Request_String(uint8_t *buff, size_t buff_size, const char *server_name,
                                                     const uint8_t *uri, const W6X_http_connection_t *settings);

/**
  * @brief  Wait for for full header response to be received \
  *         Ending of an header response is CRLF CRLF \
  *         buffer should be big enough to receive the whole response header
  * @param  sock: socket to use
  * @param  buffer: buffer to store the response
  * @param  body_start: buffer containing the start of the HTTP request body if any.
  *         If none is expected or yet received, body_start points to the end of the HTTP header in buffer.
  * @param  body_len: gives the length of the HTTP request body already received in buffer.
  *         Equals 0 if no body expected or not yet received.
  * @return W6X_STATUS_OK if header was received successfully, W6X_STATUS_ERROR otherwise
  */
static W6X_Status_t W6X_HTTP_Client_Wait_For_Header(int32_t sock, uint8_t *buffer, uint8_t *body_start,
                                                    size_t *body_len);

/**
  * @brief  Parse the response header content length value
  * @param  buffer: buffer containing the http response
  * @param  content_length: pointer to the content length value
  * @return W6X_STATUS_OK if success, W6X_STATUS_ERROR otherwise
  */
static W6X_Status_t W6X_HTTP_Client_Parse_Content_Length(const uint8_t *buffer, uint32_t *content_length);

/**
  * @brief  Parse http header response status
  * @param  buffer: buffer containing the http response
  * @param  http_version: http version
  * @param  http_status: http status code
  * @return W6X_STATUS_OK if success, W6X_STATUS_ERROR otherwise
  */
static W6X_Status_t W6X_HTTP_Client_Parse_Response_Status(const uint8_t *buffer, uint16_t *http_version,
                                                          HttpStatusCode *http_status);

/**
  * @brief  HTTP client task
  * @param  arg: Task argument
  */
static void W6X_HTTP_Client_task(void *arg);

/** @} */

/* Functions Definition ------------------------------------------------------*/
/** @addtogroup ST67W6X_API_HTTP_Public_Functions
  * @{
  */
W6X_Status_t W6X_HTTP_Init(W6X_http_mode_e HTTP_mode)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  uint32_t ncp_current_mode;
  /* Get the global W61 context pointer */
  p_DrvObj = W61_ObjGet();
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  ret = TranslateErrorStatus(W61_HTTP_GetMode(p_DrvObj, &ncp_current_mode));
  if (ret != W6X_STATUS_OK)
  {
    goto _err;
  }

  if ((HTTP_mode == W6X_HTTP_MODE_PASSIVE) && (ncp_current_mode != W6X_HTTP_MODE_PASSIVE))
  {
    /* Set NCP receive buffer size for PASSIVE mode */
    /* NCP passive buffer length can only be changed when the NCP is in active mode */
    ret = TranslateErrorStatus(W61_HTTP_SetReceiveBufferLen(p_DrvObj, HTTP_PASSIVE_RECV_BUFFER_SIZE));
    if (ret != W6X_STATUS_OK)
    {
      goto _err;
    }
  }

  ret = TranslateErrorStatus(W61_HTTP_GetReceiveBufferLen(p_DrvObj, &NcpHttpBufferSize));
  if (ret != 0)
  {
    LogError("Could not set Receive buffer size");
    return ret;
  }
  else
  {
    if (HTTP_PASSIVE_RECV_BUFFER_SIZE != NcpHttpBufferSize)
    {
      LogWarn("HTTP buffer size was not set as expected, size id %d", NcpHttpBufferSize);
    }
  }

  /* Set receive mode */
  if (HTTP_mode != ncp_current_mode)
  {
    ret = TranslateErrorStatus(W61_HTTP_SetMode(p_DrvObj, HTTP_mode));
    if (ret != W6X_STATUS_OK)
    {
      goto _err;
    }
  }
  W6X_HTTP_mode = HTTP_mode;

  return W6X_STATUS_OK;

_err:
  return ret;
}

W6X_Status_t W6X_HTTP_Client_Init(int32_t *sock, const char *http_server_addr, uint16_t http_server_port)
{
  struct sockaddr_in addr_t = {0};
  /* Requested TCP socket size on NCP side */
  int32_t optval = HTTP_CLIENT_TCP_SOCKET_SIZE;
  /* Requested timeout value for TCP receive data */
  int32_t timeout = HTTP_CLIENT_TCP_SOCK_RECV_TIMEOUT;
  uint32_t numeric_ipv4;
  int8_t is_ip = 0;

  /* Create a TCP socket */
  *sock = W6X_Net_Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (*sock < 0)
  {
    LogError("Socket creation failed");
    goto _err1;
  }

  /* Check if http_server_addr is an IPv4 address or not. If it is IPv4,
   * it will also give an uint32_t representation of this IP stored in numeric_ipv4 */
  is_ip = IPv4_Addr_Aton(http_server_addr, &numeric_ipv4);
  if (is_ip != 1)
  {
    /* Store the resolved IP address in a temporary variable (the variable is only used for the if scope) */
    uint8_t server_ip_addr[SERVER_ADDR_SIZE];

    /* Resolve IP Address from the input URL */
    if (W6X_Net_GetHostAddress(http_server_addr, server_ip_addr) == W6X_STATUS_OK)
    {
      LogInfo("IP Address from Hostname [%s]: " IPSTR, http_server_addr, IP2STR(server_ip_addr));
    }
    else
    {
      LogError("IP Address identification failed");
      goto _err2;
    }
    /* Store the address in a uint32_t representation of the IP address */
    numeric_ipv4 = ATON_R(server_ip_addr);
  }

  LogDebug("Socket creation done");

  /* Configure receive data timeout for the TCP socket */
  if (0 != W6X_Net_Setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)))
  {
    LogError("Socket set timeout option failed");
    goto _err2;
  }

  /* Configure the socket receive buffer size */
  if (0 != W6X_Net_Setsockopt(*sock, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval)))
  {
    LogError("Socket set receive buff option failed");
    goto _err2;
  }

  /* Connect to a given target */
  addr_t.sin_family = AF_INET;
  addr_t.sin_port = PP_HTONS(http_server_port);
  addr_t.sin_addr_t.s_addr = numeric_ipv4;
  if (0 != W6X_Net_Connect(*sock, (struct sockaddr *)&addr_t, sizeof(addr_t)))
  {
    LogError("Socket connection failed");
    goto _err2;
  }

  LogDebug("Socket %i connected", *sock);

  return W6X_STATUS_OK;

_err2:
  (void)W6X_HTTP_Client_DeInit(*sock);
_err1:
  return W6X_STATUS_ERROR;
}

W6X_Status_t W6X_HTTP_Client_DeInit(int32_t sock)
{
  LogDebug("Closing socket %i", sock);

  /* Close the TCP socket */
  if (0 != W6X_Net_Close(sock))
  {
    return W6X_STATUS_ERROR;
  }

  return W6X_STATUS_OK;
}

W6X_Status_t W6X_HTTP_OTA_Client_Request(int32_t sock, const char *server_name, const uint8_t *uri,
                                         const W6X_http_connection_t *settings, uint32_t *content_length,
                                         uint8_t *body_start, size_t *body_len)
{
  int32_t req_len = 0;
  int32_t req_len2 = 0;
  uint8_t *req_buffer;
  int32_t count_done;
  uint8_t *buff_head;
  HttpStatusCode http_status = HTTP_VERSION_NOT_SUPPORTED;
  uint16_t http_version = 0;

  /* Get the length of the HTTP request */
  req_len = W6X_HTTP_Client_Create_Request_String(NULL, 0, server_name, uri, settings);
  if (req_len <= 0)
  {
    goto _err1;
  }

  /* Allocate dynamically the HTTP request based on previous result */
  req_buffer = pvPortMalloc((size_t)req_len);
  if (req_buffer == NULL)
  {
    goto _err1;
  }

  /* Prepare send request */
  req_len2 = W6X_HTTP_Client_Create_Request_String(req_buffer, (size_t)req_len, server_name, uri, settings);
  if (req_len2 <= 0)
  {
    vPortFree(req_buffer);
    goto _err1;
  }

  /* Send the HTTP request to the HTTP server via the TCP socket */
  count_done = W6X_Net_Send(sock, req_buffer, (size_t)req_len, 0);
  if (count_done < 0)
  {
    LogError("Failed to send data to tcp server (%i), try again", count_done);
    vPortFree(req_buffer);
    goto _err1;
  }

  LogDebug("HTTP request send data success (%i)", count_done);

  /* Free the send request buffer */
  vPortFree(req_buffer);

  buff_head = pvPortMalloc(HTTP_HEAD_MAX_RESP_BUFFER_SIZE);
  if (buff_head == NULL)
  {
    goto _err;
  }

  /** Set receive data to 0 */
  memset(buff_head, 0x00, HTTP_HEAD_MAX_RESP_BUFFER_SIZE);

  /* Wait to receive an answer to the HTTP request we sent */
  if (W6X_STATUS_OK != W6X_HTTP_Client_Wait_For_Header(sock, buff_head, body_start, body_len))
  {
    goto _err;
  }

  /* Parse the HTTP response status */
  if (W6X_HTTP_Client_Parse_Response_Status(buff_head, &http_version, &http_status) != W6X_STATUS_OK)
  {
    goto _err;
  }

  /* Get the returned status category and check if it is a success, we only support 200 OK currently */
  if (W6X_HTTP_Client_Get_Status_Category(http_status) != HTTP_CATEGORY_SUCCESS)
  {
    goto _err;
  }

  /* Parse the content length field of the HTTP response to know how much body content to expect,
   * if we don't expect a body we set the content length value to 0 */
  if (settings->req_type == REQ_TYPE_GET)
  {
    if (W6X_HTTP_Client_Parse_Content_Length(buff_head, content_length) != W6X_STATUS_OK)
    {
      goto _err;
    }
  }
  else
  {
    *content_length = 0U;
  }
  vPortFree(buff_head);

  return W6X_STATUS_OK;

_err:
  vPortFree(buff_head);
_err1:
  return W6X_STATUS_ERROR;
}

W6X_Status_t W6X_HTTP_Get_URL(uint8_t *Url)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  return TranslateErrorStatus(W61_HTTP_Client_Set_URL(p_DrvObj, Url));
}

W6X_Status_t W6X_HTTP_Set_URL(uint8_t *Url)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  return TranslateErrorStatus(W61_HTTP_Client_Set_URL(p_DrvObj, Url));
}

W6X_Status_t W6X_HTTP_Get_Size(uint8_t *Url, uint32_t *Size, uint32_t Timeout)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  return TranslateErrorStatus(W61_HTTP_Client_Get_Size(p_DrvObj, Url, Size, Timeout));
}

W6X_Status_t W6X_HTTP_Client(uint8_t Option, uint8_t ContentType, uint8_t *Url, uint8_t *Data, uint8_t *Buf,
                             uint32_t Timeout)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  return TranslateErrorStatus(W61_HTTP_Client(p_DrvObj, Option, ContentType, Url, Data, Buf, Timeout));
}

W6X_Status_t W6X_HTTP_Get(uint8_t *Url, uint8_t *Buf, uint32_t BufLen, uint32_t Timeout)
{
  uint32_t url_data_size;
  uint32_t cumulated_recv_len;
  uint32_t recv_len;
  uint32_t chunk_size = BufLen;
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  if (W6X_HTTP_mode == W6X_HTTP_MODE_ACTIVE)
  {
    ret = TranslateErrorStatus(W61_HTTP_Client_Active_Get(p_DrvObj, Url, Buf, BufLen, Timeout));
  }
  else
  {

    ret = TranslateErrorStatus(W61_HTTP_Client_Passive_Get(p_DrvObj, Url, &url_data_size, Timeout));

    if (W6X_STATUS_OK == ret)
    {
      if (url_data_size > BufLen)
      {
        url_data_size = BufLen;  /* truncate the size if the application buffer is not big enough */
      }

      if (chunk_size > NcpHttpBufferSize)
      {
        chunk_size = NcpHttpBufferSize;
      }

      /* Should the below function be given as W6X_ API to the application ? To be discussed */
      /* see next two functions W6X_HTTP_Passive_Get and W6X_HTTP_PullDataFromSocket */
      cumulated_recv_len = 0;
      while (cumulated_recv_len < url_data_size)
      {
        ret = TranslateErrorStatus(W61_HTTP_PullDataFromSocket(p_DrvObj, 0, chunk_size, Buf + cumulated_recv_len,
                                                               &recv_len, Timeout));
        if (ret != W6X_STATUS_OK)
        {
          break;
        }
        cumulated_recv_len += recv_len;
      }
    }
  }

  return ret;
}

W6X_Status_t W6X_HTTP_Passive_Get(uint8_t *Url, uint32_t *Size, uint32_t Timeout)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  return TranslateErrorStatus(W61_HTTP_Client_Passive_Get(p_DrvObj, Url, Size, Timeout));
}

W6X_Status_t W6X_HTTP_PullDataFromSocket(uint8_t ReqID, uint32_t Reqlen, uint8_t *Buf,
                                         uint32_t *Receivedlen, uint32_t Timeout)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  if (Reqlen > NcpHttpBufferSize)
  {
    Reqlen = NcpHttpBufferSize;
  }
  return TranslateErrorStatus(W61_HTTP_PullDataFromSocket(p_DrvObj, ReqID, Reqlen, Buf, Receivedlen, Timeout));
}

W6X_Status_t W6X_HTTP_Put(uint8_t *Url, uint8_t *Data, uint8_t *Buf, uint32_t Timeout)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  return TranslateErrorStatus(W61_HTTP_Client_Put(p_DrvObj, Url, Data, Buf, Timeout));
}

W6X_Status_t W6X_HTTP_Post(uint8_t *Url, uint8_t *Data, uint8_t *Buf, uint32_t Timeout)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  return TranslateErrorStatus(W61_HTTP_Client_Post(p_DrvObj, Url, Data, Buf, Timeout));
}

/** @} */

/* Private Functions Definition ----------------------------------------------*/
/** @addtogroup ST67W6X_Private_HTTP_Functions
  * @{
  */

static inline HttpStatusCategory W6X_HTTP_Client_Get_Status_Category(HttpStatusCode status)
{
  switch (status / 100)
  {
    case 1:
      return HTTP_CATEGORY_INFORMATIONAL;
    case 2:
      return HTTP_CATEGORY_SUCCESS;
    case 3:
      return HTTP_CATEGORY_REDIRECTION;
    case 4:
      return HTTP_CATEGORY_CLIENT_ERROR;
    case 5:
      return HTTP_CATEGORY_SERVER_ERROR;
    default:
      return HTTP_CATEGORY_UNKNOWN;
  }
}

static int32_t W6X_HTTP_Client_Create_Request_String(uint8_t *buff, size_t buff_size, const char *server_name,
                                                     const uint8_t *uri, const W6X_http_connection_t *settings)
{
  int32_t req_len;
  switch (settings->req_type)
  {
    case REQ_TYPE_GET :
      req_len =  snprintf((char *)buff, buff_size, HTTPC_REQ_11_HOST_FORMAT(uri, server_name));
      break;
    default:
      /* Return -1 in case of none supported method or incorrect parameters */
      return -1;
      break;
  }
  if ((req_len < 0) || (req_len > REQUEST_HEAD_MAX_ACCEPTED_SIZE))
  {
    return -1;
  }
  /* for last character \r to be not ignored by snprintf, in case of snprintf error,
   * reqlen =-1 which means return value is 0. */
  return req_len + 1;
}

static W6X_Status_t W6X_HTTP_Client_Wait_For_Header(int32_t sock, uint8_t *buffer, uint8_t *body_start,
                                                    size_t *body_len)
{
  uint8_t header_received = 0;
  int32_t data_received = 0;
  uint32_t total_data_received = 0;
  uint8_t *header_end;
  /* Read the headers */
  do
  {
    data_received = W6X_Net_Recv(sock, buffer + total_data_received,
                                 HTTP_HEAD_MAX_RESP_BUFFER_SIZE - total_data_received, 0);
    if (data_received < 0)
    {
      LogError("Receive failed with %i\n", data_received);
      goto _err;
    }

    total_data_received += data_received;

    /* Look for the end of the headers */
    header_end = (uint8_t *) strstr((char *)buffer, "\r\n\r\n");
    if (header_end != NULL)
    {
      header_received = 1;
      header_end += 4U; /** + 4U to go past the "\r\n\r\n" termination */
      *body_len = (size_t)(total_data_received - (header_end - buffer));

      /** Copy body content to the provided buffer */
      (void)memcpy(body_start, header_end, *body_len);
      break;
    }
  } while (!header_received && (total_data_received < HTTP_HEAD_MAX_RESP_BUFFER_SIZE));

  if (!header_received)
  {
    LogError("Failed to receive HTTP headers");
    goto _err;
  }

  return W6X_STATUS_OK;

_err:
  return W6X_STATUS_ERROR;
}

static W6X_Status_t W6X_HTTP_Client_Parse_Content_Length(const uint8_t *buffer, uint32_t *content_length)
{
  const uint8_t  *header_end = (uint8_t *) strstr((char *)buffer, "\r\n\r\n");
  if (header_end != NULL)
  {
    /** Look for the Content-Length header field */
    const uint8_t  *content_len_hdr = (const uint8_t *)strstr((const char *)buffer, "Content-Length: ");
    if (content_len_hdr != NULL)
    {
      /** Parse content len value */
      const uint8_t *content_len_line_end = (const uint8_t *) strstr((const char *)content_len_hdr, "\r\n");;
      if (content_len_line_end != NULL)
      {
        uint8_t content_len_num[16];
        size_t content_len_num_len = (size_t)(content_len_line_end - content_len_hdr - 16);
        memset(content_len_num, 0, sizeof(content_len_num));
        if (strncpy((char *)content_len_num, (char *)(content_len_hdr + 16), content_len_num_len) != NULL)
        {
          int32_t len = ParseNumber((char *)content_len_num, NULL);
          if ((len >= 0))
          {
            *content_length = (uint32_t)len;
            return W6X_STATUS_OK;
          }
        }
      }
    }
  }
  return W6X_STATUS_ERROR;
}

static W6X_Status_t W6X_HTTP_Client_Parse_Response_Status(const uint8_t *buffer, uint16_t *http_version,
                                                          HttpStatusCode *http_status)
{
  uint8_t *header_end;
  /* Look for the end of the header */
  header_end = (uint8_t *) strstr((char *)buffer, "\r\n\r\n");
  if (header_end != NULL)
  {
    header_end += 4; /* Move past the "\r\n\r\n" */
    uint8_t *space1;
    space1 = (uint8_t *) strstr((char *)buffer, " ");
    if (space1 != NULL)
    {
      if ((strncmp((char *)buffer, "HTTP/", 5) == 0)  && (buffer[6] == '.'))
      {
        uint8_t status_num[10];
        size_t status_num_len;
        /* parse http version */
        uint16_t version = buffer[5] - '0';
        version <<= 8;
        version |= buffer[7] - '0';
        *http_version = version;
        /* parse HTTP status number */
        const uint8_t *space2 = (uint8_t *) strstr((char *)space1 + 1, " ");
        if (space2 != NULL)
        {
          status_num_len = space2 - space1 - 1;
        }
        else
        {
          status_num_len = header_end - space1 - 1;
        }
        memset(status_num, 0, sizeof(status_num));
        strncpy((char *)status_num, (char *)(space1 + 1), status_num_len);
        int32_t status = ParseNumber((char *)status_num, NULL);
        if ((status > 0) && (status <= 0xFFFF))
        {
          *http_status = (HttpStatusCode)status;
          return W6X_STATUS_OK;
        }
      }
    }
  }
  return W6X_STATUS_ERROR;
}

static void W6X_HTTP_Client_task(void *arg)
{
  struct http_request_task_obj *Obj = (struct http_request_task_obj *) arg;
  uint32_t content_length = 0;
  int32_t req_len = 0;
  int32_t req_len2 = 0;
  int32_t recv_data = 0;
  int32_t total_recv_data = 0;
  uint8_t *req_buffer = NULL;
  int32_t count_done;
  uint8_t *buff_head;
  uint8_t *body_start;
  HttpStatusCode http_status = HTTP_VERSION_NOT_SUPPORTED;
  uint16_t http_version = 0;
  size_t body_len = 0;
  char server_name[INET_ADDRSTRLEN] = {0};

  int8_t method;
  if (strncmp(Obj->method, "GET", 3) == 0)
  {
    method = REQ_TYPE_GET;
  }
  else if (strncmp(Obj->method, "PUT", 3) == 0)
  {
    method = REQ_TYPE_PUT;
  }
  else if (strncmp(Obj->method, "POST", 4) == 0)
  {
    method = REQ_TYPE_POST;
  }
  else if (strncmp(Obj->method, "HEAD", 4) == 0)
  {
    method = REQ_TYPE_HEAD;
  }
  else
  {
    LogError("Unknown Request type");
    goto _err;
  }

  if (method == REQ_TYPE_GET)
  {
    (void)W6X_Net_Inet_ntop(AF_INET, (void *) & (Obj->server.u_addr.ip4), server_name, INET_ADDRSTRLEN);
    /* Get the length of the HTTP request */
    req_len = strlen(HTTPC_REQ_11_HOST) + strlen(Obj->uri) + strlen(HTTPC_CLIENT_AGENT) + strlen(server_name);
    /* Allocate dynamically the HTTP request based on previous result */
    req_buffer = pvPortMalloc(req_len);
    if (req_buffer == NULL)
    {
      goto _err;
    }
    /* Prepare send request */
    req_len2 = snprintf((char *)req_buffer, req_len, HTTPC_REQ_11_HOST_FORMAT(Obj->uri, Obj->settings_t.server_name));
    if (req_len2 <= 0 || req_len2 > req_len)
    {
      vPortFree(req_buffer);
      goto _err;
    }
  }
  else
  {
    LogError("Only GET request is supported");
    goto _err;
  }

  /* Send the HTTP request to the HTTP server via the TCP socket */
  count_done = W6X_Net_Send(Obj->sock, req_buffer, req_len2, 0);
  if (count_done < 0)
  {
    LogError("Failed to send data to tcp server (%i), try again", count_done);
    vPortFree(req_buffer);
    goto _err;
  }

  LogDebug("HTTP request send data success (%i)", count_done);
  vPortFree(req_buffer);
  vTaskDelay(100);
  buff_head = pvPortMalloc(HEAD_MAX_RESP_BUFFER_SIZE);
  if (buff_head == NULL)
  {
    goto _err;
  }
  body_start = pvPortMalloc(HEAD_MAX_RESP_BUFFER_SIZE);
  if (body_start == NULL)
  {
    vPortFree(buff_head);
    goto _err;
  }
  /* Wait to receive an answer to the HTTP request we sent */
  if (W6X_STATUS_OK != W6X_HTTP_Client_Wait_For_Header(Obj->sock, buff_head, body_start, &body_len))
  {
    LogError("W6X_HTTP_Client_Wait_For_Header failed");
    vPortFree(buff_head);
    vPortFree(body_start);
    goto _err;
  }

  /* Parse the HTTP response status */
  if (W6X_HTTP_Client_Parse_Response_Status(buff_head, &http_version, &http_status) != W6X_STATUS_OK)
  {
    LogError("W6X_HTTP_Client_Parse_Response_Status failed");
    vPortFree(buff_head);
    vPortFree(body_start);
    goto _err;
  }

  /* Get the returned status category and check if it is a success, we only support 200 OK currently */
  if (W6X_HTTP_Client_Get_Status_Category(http_status) != HTTP_CATEGORY_SUCCESS)
  {
    LogError("W6X_HTTP_Client_Get_Status_Category failed");
    vPortFree(buff_head);
    vPortFree(body_start);
    goto _err;
  }

  /* Parse the content length field of the HTTP response to know how much body content to expect,
   * if we don't expect a body we set the content length value to 0 */
  if (method == REQ_TYPE_GET)
  {
    if (W6X_HTTP_Client_Parse_Content_Length(buff_head, &content_length) != W6X_STATUS_OK)
    {
      LogDebug("Content Length tag not found in header");
    }
  }

  if (Obj->settings_t.headers_done_fn)
  {
    (Obj->settings_t.headers_done_fn)(NULL, Obj->settings_t.callback_arg, buff_head,
                                      strlen((char *)buff_head) - body_len, content_length);
  }
  if (Obj->settings_t.result_fn)
  {
    (Obj->settings_t.result_fn)(Obj->settings_t.callback_arg, http_status, content_length, 0, 0);
  }
  if (body_len > 0)
  {
    total_recv_data += recv_data;
    /* Pass Data to callback if not NULL */
    if (Obj->settings_t.recv_fn)
    {
      (Obj->settings_t.recv_fn)(Obj->settings_t.recv_fn_arg, body_start, 0);
    }
  }
  vPortFree(body_start);
  memset(buff_head, 0, HEAD_MAX_RESP_BUFFER_SIZE);
  do
  {
    recv_data = W6X_Net_Recv(Obj->sock, buff_head, HEAD_MAX_RESP_BUFFER_SIZE, 0);
    if (recv_data > 0)
    {
      total_recv_data += recv_data;
      /* Pass Data to callback if not NULL */
      if (Obj->settings_t.recv_fn)
      {
        (Obj->settings_t.recv_fn)(Obj->settings_t.recv_fn_arg, buff_head, 0);
      }
      memset(buff_head, 0, HEAD_MAX_RESP_BUFFER_SIZE);
    }
    if (content_length > 0 && total_recv_data >= content_length)
    {
      break;
    }
  } while (recv_data > 0);

  vPortFree(buff_head);

_err:
  W6X_Net_Tls_Credential_Delete(Obj->sock, TLS_CREDENTIAL_CA_CERTIFICATE);
  W6X_Net_Close(Obj->sock);
  vPortFree(Obj->post_data);
  vPortFree(Obj->uri);
  vPortFree(Obj);
  vTaskDelete(NULL);
}

W6X_Status_t W6X_Httpc_Request(const ip_addr_t *server_addr, uint16_t port, const char *uri, const char *method,
                               const void *post_data, size_t post_data_len, httpc_result_fn result_fn,
                               void *callback_arg, httpc_headers_done_fn headers_done_fn, httpc_data_fn data_fn,
                               const struct httpc_connection *settings)
{
  int32_t sock;
  struct sockaddr_in addr_t = {0};
  int32_t optval = HTTP_CLIENT_TCP_SOCKET_SIZE;
  int32_t timeout = HTTP_CLIENT_TCP_SOCK_RECV_TIMEOUT;
  uint8_t sec_tag_list[] = { 0 };
  char alpn_list[16] = "http1.1,http1.2";
  struct http_request_task_obj *Obj = NULL;
  /* Create a TCP socket  if the port is not 443 which is used for https*/
  if (port != 443)
  {
    sock = W6X_Net_Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
    {
      LogError("Socket creation failed");
      return W6X_STATUS_ERROR;
    }
  }
  else
  {
    sock = W6X_Net_Socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);
    if (sock < 0)
    {
      LogError("Socket creation failed");
      return W6X_STATUS_ERROR;
    }

    /* Use a tag that has the value of the socket id to be able to run multiple
     * without waiting for the previous to end */
    if (settings->https_certificate != NULL && settings->https_certificate_len > 0)
    {
      sec_tag_list[0] = sock;
      if (W6X_Net_Tls_Credential_Add(sec_tag_list[0], TLS_CREDENTIAL_CA_CERTIFICATE,
                                     (void *)settings->https_certificate, settings->https_certificate_len) != 0)
      {
        LogError("Socket creation failed");
        W6X_Net_Close(sock);
        return W6X_STATUS_ERROR;
      }

      if (W6X_Net_Setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_list, sizeof(sec_tag_list)) != 0)
      {
        LogInfo("Set socket option failed");
        goto _err;
      }
    }

    LogDebug("Socket option set");

    if (W6X_Net_Setsockopt(sock, SOL_TLS, TLS_ALPN_LIST, alpn_list, 2) != 0)
    {
      LogError("Set ALPN failed");
      goto _err;
    }

    LogDebug("ALPN set");

    if (W6X_Net_Setsockopt(sock, SOL_TLS, TLS_HOSTNAME, settings->server_name, strlen(settings->server_name)) != 0)
    {
      LogError("Set SNI failed");
      goto _err;
    }

    LogDebug("SNI set");
  }

  LogDebug("Socket creation done");
  /* Configure receive data timeout for the TCP socket */
  if (0 != W6X_Net_Setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout,  sizeof(timeout)))
  {
    LogError("Socket set timeout option failed");
    goto _err;
  }

  /* Configure the socket receive buffer size */
  if (0 != W6X_Net_Setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval)))
  {
    LogError("Socket set receive buff option failed");
    goto _err;
  }

  /* supports only IPv4 without DNS */
  addr_t.sin_family = AF_INET;
  addr_t.sin_port = PP_HTONS(port);
  addr_t.sin_addr_t.s_addr = server_addr->u_addr.ip4;
  if (0 != W6X_Net_Connect(sock, (struct sockaddr *)&addr_t, sizeof(addr_t)))
  {
    LogError("Socket connection failed");
    goto _err;
  }

  LogDebug("Socket %i connected", sock);
  Obj = pvPortMalloc(sizeof(struct http_request_task_obj));
  if (Obj == NULL)
  {
    LogError("Callback structure allocation failed");
    goto _err;
  }
  memset(Obj, 0, sizeof(struct http_request_task_obj));
  Obj->sock = sock;
  Obj->post_data_len = post_data_len;

  if (post_data != NULL && post_data_len > 0)
  {
    Obj->post_data = pvPortMalloc(post_data_len);
    if (Obj->post_data == NULL)
    {
      vPortFree(Obj);
      LogError("Post data ");
      goto _err;
    }
    memcpy(Obj->post_data, post_data, Obj->post_data_len);
  }

  Obj->uri = pvPortMalloc(strlen(uri) + 1);
  if (Obj->uri == NULL)
  {
    vPortFree(Obj);
    vPortFree(Obj->post_data);
    LogError("Callback structure allocation failed");
    goto _err;
  }
  strncpy(Obj->uri, uri, strlen(uri));
  Obj->uri[strlen(uri)] = 0;
  strncpy(Obj->method, method, 5);
  memcpy(&(Obj->server), server_addr, sizeof(ip_addr_t));
  memcpy(&(Obj->settings_t), settings, sizeof(struct httpc_connection));

  /*Run HTTP get and close socket in separate thread */
  if (pdPASS == xTaskCreate(W6X_HTTP_Client_task, "HTTP task", W6X_HTTP_CLIENT_THREAD_STACK_SIZE >> 2,
                            Obj, W6X_HTTP_CLIENT_THREAD_PRIO, NULL))
  {
    return W6X_STATUS_OK;
  }

  /*Failed to start the task, need to free buffer and structures */
  vPortFree(Obj->post_data);
  vPortFree(Obj->uri);
  vPortFree(Obj);
_err:
  W6X_Net_Tls_Credential_Delete(sock, TLS_CREDENTIAL_CA_CERTIFICATE);
  W6X_Net_Close(sock);
  return W6X_STATUS_ERROR;
}
/** @} */

/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : echo_client.c
 * @date           :
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
#include "logging.h"
#include "sys_evt.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include <string.h>

#if defined(ST67W6X)
#include "w6x_lwip_port.h"
#else
#include "lwip/sockets.h"
#endif

/* Private typedef -----------------------------------------------------------*/

/* Private Macro -------------------------------------------------------------*/

#if 1
#define ECHO_SERVER_URL "192.168.1.18"
#define ECHO_PORT 7
#else
#define ECHO_SERVER_URL "tcpbin.com"
#define ECHO_PORT 4242
#endif

#define BUFFER_SIZE 1024

/* Private Variables ---------------------------------------------------------*/

/* Private Function prototypes -----------------------------------------------*/
static int create_socket(void);
static BaseType_t resolve_hostname(const char *hostname, uint8_t *host_addr);
static BaseType_t connect_to_server(int sock, const uint8_t *host_addr, in_port_t port);
static BaseType_t send_message(int sock, const char *msg);
static BaseType_t receive_message(int sock, char *pBuf, int size);

/* User code -----------------------------------------------------------------*/
/* Main Echo Client Task */
void vEchoClientTask(void *pvParameters)
{
  uint16_t message_id = 0;
  int sock = -1;

  BaseType_t xStatus = pdTRUE;

  uint8_t *host_addr = pvPortMalloc(4);
  configASSERT(host_addr != NULL);

  char *pBuf         = pvPortMalloc(BUFFER_SIZE);
  configASSERT(pBuf != NULL);

  LogInfo("%s started", __func__);
  (void) pvParameters;

  /* Wait until connected to the network */
  (void) xEventGroupWaitBits(xSystemEvents, EVT_MASK_NET_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);

  /* Create socket */
  sock = create_socket();

  if (sock < 0)
  {
    LogError("Socket creation failed");
    xStatus = pdFALSE;
  }

  if (pdTRUE == xStatus)
  {
    /* Resolve hostname */
    xStatus = resolve_hostname(ECHO_SERVER_URL, host_addr);

    if (pdTRUE != xStatus)
    {
      LogError("Hostname resolution failed");
    }
  }

  if (pdTRUE == xStatus)
  {
    /* Connect to server */
    xStatus = connect_to_server(sock, host_addr, ECHO_PORT);

    if (pdTRUE != xStatus)
    {
      LogError("Connection to server failed");
    }
    else
    {
      LogInfo("Connected to %s:%d using sock %d",ECHO_SERVER_URL, ECHO_PORT, sock);
    }
  }

  while (pdTRUE == xStatus)
  {
    /* Generate and send message */
    char msg[256];
    snprintf(msg, sizeof(msg), "socket test msg id: %d", message_id++);

    xStatus = send_message(sock, msg);

    if (pdTRUE != xStatus)
    {
      LogError("Message sending failed");
    }

    if (pdTRUE == xStatus)
    {
      /* Receive response */
      xStatus = receive_message(sock, pBuf, BUFFER_SIZE);

      if (pdTRUE != xStatus)
      {
        LogError("Message reception failed");
      }
    }

    if (pdTRUE == xStatus)
    {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }


  /* Cleanup resources before exiting */
  LogInfo("Cleanup resources");

  if (sock >= 0)
  {
    LogInfo("Closing socket: %d", sock);
    lwip_close(sock);
  }

  if (pBuf != NULL)
  {
    vPortFree(pBuf);
  }

  if (host_addr != NULL)
  {
    vPortFree(host_addr);
  }

  vTaskSuspend(NULL);
}

/* Subroutine: Socket creation */
static int create_socket(void)
{
  int sock = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (sock < 0)
  {
    LogError("Socket creation failed");
  }

  return sock;
}

/* Subroutine: Resolve hostname to IP address */
static BaseType_t resolve_hostname(const char *hostname, uint8_t *host_addr)
{
  BaseType_t xStatus = pdTRUE;
  int32_t lError = lwip_gethostbyname(hostname, host_addr);

  if (lError == W6X_STATUS_OK)
  {
    LogInfo("IP Address from Hostname [%s]: %d.%d.%d.%d", hostname, host_addr[0], host_addr[1], host_addr[2], host_addr[3]);
  }
  else
  {
    LogError("IP Address identification failed");
    xStatus = pdFALSE;
  }

  return xStatus;
}

/* Subroutine: Connect to the server */
static BaseType_t connect_to_server(int sock, const uint8_t *host_addr, in_port_t port)
{
  BaseType_t xStatus = pdTRUE;
  struct sockaddr_in address;

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr(host_addr);
  address.sin_port = htons(port);

  if (lwip_connect(sock, (struct sockaddr*) &address, sizeof(address)) < 0)
  {
    LogError("Socket connect failed");
    xStatus = pdFALSE;
  }

  return xStatus;
}

/* Subroutine: Send message */
static BaseType_t send_message(int sock, const char *msg)
{
  BaseType_t xStatus = pdTRUE;

  ssize_t sent_bytes = lwip_send(sock, msg, strlen(msg), 0);

  if (sent_bytes < 0)
  {
    LogError("Error: Unable to send message");
    xStatus = pdFALSE;
  }

  LogInfo("%d bytes sent", sent_bytes);

  return xStatus;
}

/* Subroutine: Receive message */
static BaseType_t receive_message(int sock, char *pBuf, int size)
{
  BaseType_t xStatus = pdTRUE;

  ssize_t received_bytes = lwip_recv(sock, pBuf, size, 0);

  if (received_bytes < 0)
  {
    LogError("Unable to receive message");
    xStatus = pdFALSE;
  }
  else if (received_bytes == 0)
  {
    LogError("Connection closed by server");
    xStatus = pdFALSE;
  }
  else
  {
    pBuf[received_bytes] = '\0';
    LogInfo("Received: %s", pBuf);
  }

  return xStatus;
}




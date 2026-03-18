/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : echo_server.c
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

#if defined(ST67W6X_NCP)
#include "../../net/W6X_ARCH_T01/w6x_lwip_port.h"
#else
#include "lwip/sockets.h"
#endif

/* Private typedef -----------------------------------------------------------*/

/* Private Macro -------------------------------------------------------------*/
#define ECHO_SERVER_PORT 7
#define BUFFER_SIZE 1024

/* Private Variables ---------------------------------------------------------*/

/* Private Function prototypes -----------------------------------------------*/
static int create_socket(void);
static BaseType_t bind_socket(int sock, in_port_t port);
static BaseType_t listen_socket(int sock);
static void handle_client(int client_sock);

/* User code -----------------------------------------------------------------*/
/* Main Echo Server Task */
void vEchoServerTask(void *pvParameters)
{
  int server_sock = -1;
  BaseType_t xStatus = pdTRUE;

  LogInfo("%s started", __func__);
  (void) pvParameters;

  /* Wait until connected to the network */
  (void) xEventGroupWaitBits(xSystemEvents, EVT_MASK_NET_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);

  /* Create the server socket */
  server_sock = create_socket();

  if (server_sock < 0)
  {
    xStatus = pdFALSE;
  }

  /* Bind the server socket */
  if (pdTRUE == xStatus)
  {
    xStatus = bind_socket(server_sock, ECHO_SERVER_PORT);
  }

  /* Listen on the server socket */
  if (pdTRUE == xStatus)
  {
    xStatus = listen_socket(server_sock);
  }

  if (pdTRUE == xStatus)
  {
    LogInfo("Echo server is listening on port %d, using sock %d", ECHO_SERVER_PORT, server_sock);

    while (1)
    {
      struct sockaddr_in client_addr;
      socklen_t client_addr_len = sizeof(client_addr);

      int client_sock = lwip_accept(server_sock, (struct sockaddr*) &client_addr, &client_addr_len);

      if (client_sock < 0)
      {
        LogError("Failed to accept client connection");
        continue;
      }

      LogInfo("Client connected");
      handle_client(client_sock);
    }
  }

  /* Cleanup resources before exiting */
  if (server_sock >= 0)
  {
    lwip_close(server_sock);
  }

  vTaskSuspend(NULL); /* Single exit point */
}

/* Subroutine: Create a TCP socket */
static int create_socket(void)
{
  int sock = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0)
  {
    LogError("Socket creation failed");
  }
  return sock;
}

/* Subroutine: Bind the socket to the echo port */
static BaseType_t bind_socket(int sock, in_port_t port)
{
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (lwip_bind(sock, (struct sockaddr*) &address, sizeof(address)) < 0)
  {
    LogError("Failed to bind socket");
    return pdFALSE;
  }
  return pdTRUE;
}

/* Subroutine: Listen for incoming connections */
static BaseType_t listen_socket(int sock)
{
  if (lwip_listen(sock, 5) < 0)
  {
    LogError("Failed to listen on socket");
    return pdFALSE;
  }
  return pdTRUE;
}

/* Subroutine: Handle client connection */
static void handle_client(int client_sock)
{
  char buffer[BUFFER_SIZE];
  ssize_t bytes_received;

  while ((bytes_received = lwip_recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0)
  {
    buffer[bytes_received] = '\0'; /* Null-terminate the received data */
    LogInfo("Received : %s", buffer);

    LogInfo("Sending: %s", buffer);
    if (lwip_send(client_sock, buffer, bytes_received, 0) < 0)
    {
      LogError("Failed to send data to client");
      break;
    }
  }

  LogInfo("Client disconnected");
  lwip_close(client_sock);
}



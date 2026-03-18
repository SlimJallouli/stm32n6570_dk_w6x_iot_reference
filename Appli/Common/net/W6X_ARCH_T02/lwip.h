/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    lwip.h
  * @author  GPM Application Team
  * @brief   This file provides code for the configuration of the LWIP.
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LWIP_H
#define __LWIP_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/timeouts.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "w6x_api.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported constants --------------------------------------------------------*/
/**
  * @brief  Network interface type
  */
typedef enum
{
  NETIF_STA = 0,                   /*!< Station interface */
  NETIF_AP  = 1,                   /*!< Access Point interface */
  NETIF_MAX,                       /*!< Maximum number of interfaces */
} Netif_type_t;

/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported functions --------------------------------------------------------*/
/**
  * @brief  Initialize the LwIP stack and create a network task.
  * @return Operation status
  */
int32_t MX_LWIP_Init(void);

/**
  * @brief  Retrieve the network interface structure.
  * @param  link_id: Link identifier (0 for STA, 1 for AP)
  * @return Pointer to the network interface structure.
  */
struct netif *netif_get_interface(uint32_t link_id);

/**
  * @brief  Print IPv4 addresses assigned to the network interface.
  * @param  netif: Pointer to the network interface structure.
  * @return Operation status
  */
int32_t print_ipv4_addresses(struct netif *netif);

/**
  * @brief  Print all valid IPv6 addresses assigned to the network interface.
  * @param  netif: Pointer to the network interface structure.
  * @return Operation status
  */
int32_t print_ipv6_addresses(struct netif *netif);

/**
  * @brief  Get the list of connected stations in AP mode.
  * @param  ConnectedSta: Pointer to store the list of connected stations.
  * @return Operation status
  */
int32_t aplist_sta(W6X_WiFi_Connected_Sta_t *ConnectedSta);

/* USER CODE BEGIN EF */
#define EVT_APP_WIFI_CONNECTED                      (1<<0)/** Event when Wi-Fi is connected to an Access Point      */
#define EVT_APP_WIFI_DISCONNECTED                   (1<<1)/** Event when Wi-Fi is disconnected from the Access Point */
#define EVT_APP_WIFI_GOT_IP                         (1<<2)/** Event when Wi-Fi is got an IP address                  */

/** Application events bitmask */
#define EVT_APP_ALL_BIT         (EVT_APP_WIFI_CONNECTED  | EVT_APP_WIFI_DISCONNECTED | EVT_APP_WIFI_GOT_IP)
/* USER CODE END EF */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __LWIP_H */

/**
  ******************************************************************************
  * @file    w6x_default_config.h
  * @author  GPM Application Team
  * @brief   Header file for the W61 configuration module
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef W6X_DEFAULT_CONFIG_H
#define W6X_DEFAULT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "w6x_config.h"

/* Exported constants --------------------------------------------------------*/
/** @defgroup ST67W6X_API_System_Public_Constants ST67W6X System Constants
  * @ingroup  ST67W6X_API_System
  * @{
  */

#ifndef W6X_POWER_SAVE_AUTO
/** NCP will go by default in low power mode when NCP is in idle mode */
#define W6X_POWER_SAVE_AUTO                     1
#endif /* W6X_POWER_SAVE_AUTO */

/** @} */

/** @addtogroup ST67W6X_API_WiFi_Public_Constants
  * @{
  */

#ifndef W6X_AUTOCONNECT
/** Boolean to enable/disable autoconnect functionality. Requires W61_NCP_STORE_MODE enabled */
#define W6X_AUTOCONNECT                         0
#endif /* W61_AUTOCONNECT */

#ifndef W6X_DHCP
/** Define the DHCP configuration : 0: NO DHCP, 1: DHCP CLIENT STA, 2:DHCP SERVER AP, 3: DHCP STA+AP */
#define W6X_DHCP                                1
#endif /* W6X_DHCP */

#ifndef W6X_COUNTRY_CODE
/** Define the region code, supported values : CN, JP, US, EU, Wd */
#define W6X_COUNTRY_CODE                        "Wd"
#endif /* W6X_COUNTRY_CODE */

#ifndef W6X_COUNTRY_START_CHANNEL
/** Define the channel number to start */
#define W6X_COUNTRY_START_CHANNEL               1
#endif /* W6X_COUNTRY_START_CHANNEL */

#ifndef W6X_COUNTRY_TOTAL_CHANNELS
/** Define the total number of channels */
#define W6X_COUNTRY_TOTAL_CHANNELS              13
#endif /* W6X_COUNTRY_TOTAL_CHANNELS */

#ifndef W6X_ADAPTIVE_COUNTRY_CODE
/** Define if the country code will match AP's one. (0: static country code, 1: match AP's country code) */
#define W6X_ADAPTIVE_COUNTRY_CODE               0
#endif /* W6X_ADAPTIVE_COUNTRY_CODE */

#ifndef W6X_HOSTNAME
/** String defining Wi-Fi hostname */
#define W6X_HOSTNAME                            "W61_WiFi"
#endif /* W6X_HOSTNAME */

#ifndef W6X_MAX_CONNECTED_STATIONS
/** Define the max number of stations that can connect to the soft-AP */
#define W6X_MAX_CONNECTED_STATIONS              10
#endif /* W6X_MAX_CONNECTED_STATIONS */

#ifndef W6X_AP_IP_SUBNET
/** String defining soft-AP subnet to use.
  *  Last digit of IP address automatically set to 1 */
#define W6X_AP_IP_SUBNET                        {192, 168, 8}
#endif /* W6X_AP_IP_SUBNET */

#ifndef W6X_AP_IP_SUBNET_BACKUP
/** String defining soft-AP subnet to use in case of conflict with the AP the STA is connected to.
  *  Last digit of IP address automatically set to 1 */
#define W6X_AP_IP_SUBNET_BACKUP                 {192, 168, 9}
#endif /* W6X_AP_IP_SUBNET_BACKUP */
/** @} */

/** @addtogroup ST67W6X_API_BLE_Public_Constants
  * @{
  */

#ifndef W6X_BLE_NAME
/** String defining BLE name */
#define W6X_BLE_NAME                            "W61_BLE"
#endif /* W6X_BLE_NAME */

/** @} */

/** @addtogroup ST67W6X_API_HTTP_Public_Constants
  * @{
  */

/** Allocated buffer size for passive receive. 32 for the AT header */
#ifndef HTTP_PASSIVE_RECV_BUFFER_SIZE
#define HTTP_PASSIVE_RECV_BUFFER_SIZE       (2048+32)
#endif /* HTTP_PASSIVE_RECV_BUFFER_SIZE */

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* W6X_DEFAULT_CONFIG_H */

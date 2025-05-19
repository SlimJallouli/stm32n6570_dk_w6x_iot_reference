/**
  ******************************************************************************
  * @file    w61_driver_config.h
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
#ifndef W61_DRIVER_CONFIG_H
#define W61_DRIVER_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "main.h"
/* Exported constants --------------------------------------------------------*/
/** Maximum number of detected AP during the scan. Cannot be greater than 50 */
#define W61_WIFI_MAX_DETECTED_AP                50

/** Maximum number of BLE connections */
#define W61_BLE_MAX_CONN_NBR                    1

/** Maximum number of BLE services */
#define W61_BLE_MAX_SERVICE_NBR                 5

/** Maximum number of BLE characteristics per service */
#define W61_BLE_MAX_CHAR_NBR                    5

/** BLE Service/Characteristic UUID maximum size size */
#define W61_BLE_MAX_UUID_SIZE                   17

/** Maximum number of detected peripheral during the scan. Cannot be greater than 50 */
#define W61_BLE_MAX_DETECTED_PERIPHERAL         30

/** Timeout in ms for the AT command response */
#define W61_TIMEOUT                             2000

/* Logger usage */
/** Enable/Disable the AT command log */ // @SJ
#if !defined(W61_AT_LOG_ENABLE)
#define W61_AT_LOG_ENABLE                       0
#endif

#if !defined(SYS_DBG_ENABLE_TA4)
#define SYS_DBG_ENABLE_TA4                      0
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* W61_DRIVER_CONFIG_H */

/**
  ******************************************************************************
  * @file    w6x_config_template.h
  * @author  GPM Application Team
  * @brief   Header file for the W6X configuration module
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
#ifndef W6X_CONFIG_TEMPLATE_H
#define W6X_CONFIG_TEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/*
 * All available configuration defines can be found in
 * Middlewares\ST\ST67W6X_Network_Driver\Conf\w6x_config_template.h
 */

/** ============================
  * System
  *
  * All available configuration defines in
  * Middlewares\ST\ST67W6X_Network_Driver\Core\w6x_default_config.h
  * ============================
  */

/** NCP will go by default in low power mode when NCP is in idle mode */
#define W6X_POWER_SAVE_AUTO                     1

/** ============================
  * Wi-Fi
  *
  * All available configuration defines in
  * Middlewares\ST\ST67W6X_Network_Driver\Core\w6x_default_config.h
  * ============================
  */
#if 0
/** Boolean to enable/disable autoconnect functionality */
#define W6X_AUTOCONNECT                         0
#endif /* Auto connect unused while store mode disabled */

/** Define the DHCP configuration : 0: NO DHCP, 1: DHCP CLIENT STA, 2:DHCP SERVER AP, 3: DHCP STA+AP */
#define W6X_DHCP                                1

/** Define the max number of stations that can connect to the soft-AP */
#define W6X_MAX_CONNECTED_STATIONS              10

/** String defining soft-AP subnet to use.
  *  Last digit of IP address automatically set to 1 */
#define W6X_AP_IP_SUBNET                        {192, 168, 8}

/** String defining soft-AP subnet to use in case of conflict with the AP the STA is connected to.
  *  Last digit of IP address automatically set to 1 */
#define W6X_AP_IP_SUBNET_BACKUP                 {192, 168, 9}

/** Define the region code, supported values : [CN, JP, US, EU, 00] */
#define W6X_COUNTRY_CODE                        "00"

/** Define if the country code will match AP's one.
  * 0: match AP's country code,
  * 1: static country code */
#define W6X_ADAPTIVE_COUNTRY_CODE               0

/** String defining Wi-Fi hostname */
#define W6X_HOSTNAME                            "ST67W61_WiFi"

/** ============================
  * Net
  *
  * All available configuration defines in
  * Middlewares\ST\ST67W6X_Network_Driver\Core\w6x_default_config.h
  * ============================
  */
/** Timeout in ticks when calling W6X_Net_Recv() */
#define W6X_NET_RECV_TIMEOUT                    10000

/** ============================
  * BLE
  *
  * All available configuration defines in
  * Middlewares\ST\ST67W6X_Network_Driver\Core\w6x_default_config.h
  * ============================
  */
/** String defining BLE name */
#define W6X_BLE_NAME                            "ST67W61_BLE"

/** ============================
  * Utility Performance Iperf
  *
  * All available configuration defines in
  * Middlewares\ST\ST67W6X_Network_Driver\Utils\Performance\iperf.h
  * ============================
  */
/** Enable Iperf feature */
#define IPERF_ENABLE                            1

/** Enable IPv6 for Iperf */
#define IPERF_V6                                0

/** Iperf traffic task priority */
#define IPERF_TRAFFIC_TASK_PRIORITY             40

/** Iperf traffic task stack size */
#define IPERF_TRAFFIC_TASK_STACK                2048

/** Iperf report task stack size */
#define IPERF_REPORT_TASK_STACK                 2048

/** Iperf memory allocator */
#define IPERF_MALLOC                            pvPortMalloc

/** Iperf memory deallocator */
#define IPERF_FREE                              vPortFree

/** ============================
  * Utility Performance Memory usage
  *
  * All available configuration defines in
  * Middlewares\ST\ST67W6X_Network_Driver\Utils\Performance\util_mem_perf.h
  * ============================
  */
/** Enable memory performance measurement */
#define MEM_PERF_ENABLE                         1

/** Number of memory allocation to keep track of */
#define LEAKAGE_ARRAY                           100U

/** Allocator maximum iteration before to break */
#define ALLOC_BREAK                             0xFFFFFFFFU

/** ============================
  * Utility Performance Task usage
  *
  * All available configuration defines in
  * Middlewares\ST\ST67W6X_Network_Driver\Utils\Performance\util_task_perf.h
  * ============================
  */
/** Enable task performance measurement */
#define TASK_PERF_ENABLE                        1

/** Maximum number of thread to monitor */
#define PERF_MAXTHREAD                          8U

/** ============================
  * External service littlefs usage
  *
  * All available configuration defines in
  * ============================
  */
/** Enable LittleFS */
#define LFS_ENABLE                              0

#if (LFS_ENABLE == 1)
#include "easyflash.h"
#endif /* LFS_ENABLE */

/** ============================
  * Utility Performance WFA
  *
  * All available configuration defines in
  * Middlewares\ST\ST67W6X_Network_Driver\Utils\Performance\wfa_tg.h
  * ============================
  */
/** Enable Wi-Fi Alliance Traffic Generator */
#define WFA_TG_ENABLE                           0

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* W6X_CONFIG_TEMPLATE_H */

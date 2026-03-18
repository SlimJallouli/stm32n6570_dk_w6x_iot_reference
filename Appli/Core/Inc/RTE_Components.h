/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file
  * @author  MCD Application Team
  * @version V2.0.0
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#ifndef  __RTE_COMPONENTS_H__
#define  __RTE_COMPONENTS_H__

/* Defines ------------------------------------------------------------------*/
/* ARM.CMSIS-FreeRTOS.11.1.0 */
#define RTE_RTOS_FreeRTOS_CORE /* RTOS FreeRTOS Core */
#define RTE_RTOS_FreeRTOS_CONFIG /* RTOS FreeRTOS Config for FreeRTOS API */
#define RTE_RTOS_FreeRTOS_COROUTINE /* RTOS FreeRTOS Co-routines */
#define RTE_RTOS_FreeRTOS_EVENTGROUPS /* RTOS FreeRTOS Event Groups */
#define RTE_RTOS_FreeRTOS_HEAP_4 /* RTOS FreeRTOS Heap 4 */
#define RTE_RTOS_FreeRTOS_MESSAGE_BUFFER /* RTOS FreeRTOS Message Buffers */
#define RTE_RTOS_FreeRTOS_STREAM_BUFFER /* RTOS FreeRTOS Stream Buffers */
#define RTE_RTOS_FreeRTOS_TIMERS /* RTOS FreeRTOS Timers */
/* ARM.mbedTLS.3.1.1 */
#define RTE_PSA_API_CRYPTO
#define RTE_Security_mbedTLS /* Security mbed TLS */
/* lwIP.lwIP.2.3.0 */
<!-- the following content goes into file 'RTE_Components.h' -->
#define RTE_Network_Core /* Network Core */
#define RTE_Network_IPv4 /* Network IPv4 Stack */
#define RTE_Network_RTOS /* Network RTOS */
#define RTE_Network_FreeRTOS /* Network FreeRTOS */
#define RTE_Network_Interface_Ethernet /* Network Interface Ethernet */
#define RTE_Network_API /* Network API */

#endif /* __RTE_COMPONENTS_H__ */

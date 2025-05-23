/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined ( __ICCARM__ )
#  define CMSE_NS_CALL  __cmse_nonsecure_call
#  define CMSE_NS_ENTRY __cmse_nonsecure_entry
#else
#  define CMSE_NS_CALL  __attribute((cmse_nonsecure_call))
#  define CMSE_NS_ENTRY __attribute((cmse_nonsecure_entry))
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32n6xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* Function pointer declaration in non-secure*/
#if defined ( __ICCARM__ )
typedef void (CMSE_NS_CALL *funcptr)(void);
#else
typedef void CMSE_NS_CALL (*funcptr)(void);
#endif

/* typedef for non-secure callback functions */
typedef funcptr funcptr_NS;

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define RunTimeStats_Timer htim5
#define xConsoleHandle hlpuart1
#define w6x_spi hspi5
#define USE_SENSORS 0
#define ARD_D06_Pin GPIO_PIN_13
#define ARD_D06_GPIO_Port GPIOE
#define ARD_D13_Pin GPIO_PIN_15
#define ARD_D13_GPIO_Port GPIOE
#define ARD_D03_Pin GPIO_PIN_9
#define ARD_D03_GPIO_Port GPIOE
#define ARD_D03_EXTI_IRQn EXTI9_IRQn
#define ARD_D05_Pin GPIO_PIN_10
#define ARD_D05_GPIO_Port GPIOE
#define ARD_D12_Pin GPIO_PIN_8
#define ARD_D12_GPIO_Port GPIOH
#define USER_BUTTON_Pin GPIO_PIN_12
#define USER_BUTTON_GPIO_Port GPIOD
#define USER_BUTTON_EXTI_IRQn EXTI12_IRQn
#define LED_GREEN_Pin GPIO_PIN_0
#define LED_GREEN_GPIO_Port GPIOG
#define ARD_D10_Pin GPIO_PIN_3
#define ARD_D10_GPIO_Port GPIOA
#define LED_RED_Pin GPIO_PIN_10
#define LED_RED_GPIO_Port GPIOG
#define ARD_D11_Pin GPIO_PIN_2
#define ARD_D11_GPIO_Port GPIOG
#define LED_BLUE_Pin GPIO_PIN_8
#define LED_BLUE_GPIO_Port GPIOG

/* USER CODE BEGIN Private defines */
/********** X-NUCLEO-WW611M1 Pin definition ***********/
#define LP_WAKEUP_Pin                           ARD_D10_Pin
#define LP_WAKEUP_GPIO_Port                     ARD_D10_GPIO_Port

#define BOOT_Pin                                ARD_D06_Pin
#define BOOT_GPIO_Port                          ARD_D06_GPIO_Port

#define CHIP_EN_Pin                             ARD_D05_Pin
#define CHIP_EN_GPIO_Port                       ARD_D05_GPIO_Port

#define SPI_SLAVE_DATA_RDY_Pin                  ARD_D03_Pin
#define SPI_SLAVE_DATA_RDY_GPIO_Port            ARD_D03_GPIO_Port
#define SPI_SLAVE_DATA_RDY_EXTI_IRQn            ARD_D03_EXTI_IRQn

/************ Board LED Pin configuration *************/
#define LED_RED_ON                              GPIO_PIN_RESET
#define LED_RED_OFF                             GPIO_PIN_SET

/**************** MbedTLS debug config ****************/
#define MBEDTLS_DEBUG_NO_DEBUG                  0 /* No debug messages are displayed                                        */
#define MBEDTLS_DEBUG_ERROR                     1 /* Only error messages are shown                                          */
#define MBEDTLS_DEBUG_CHANGE                    2 /* Messages related to state changes in the SSL/TLS process are displayed */
#define MBEDTLS_DEBUG_INFO                      3 /* Provides general information about the SSL/TLS process                 */
#define MBEDTLS_DEBUG_VERBOSE                   4 /* Displays detailed debug information, including low-level operations    */

#define MBEDTLS_DEBUG_THRESHOLD                 MBEDTLS_DEBUG_INFO

/******************** Tasks config ********************/
#define DEMO_PUB_SUB                            1
#define DEMO_OTA                                1
#define DEMO_ENV_SENSOR                         1
#define DEMO_MOTION_SENSOR                      1
#define DEMO_SHADOW                             1
#define DEMO_DEFENDER                           0
#define DEMO_SNTP                               0

#define MQTT_ENABLED                            (DEMO_PUB_SUB || DEMO_OTA || DEMO_ENV_SENSOR || DEMO_MOTION_SENSOR || DEMO_SHADOW)

#define TASK_PRIO_OTA                           (tskIDLE_PRIORITY + 1)
#define TASK_PRIO_SNTP                          (tskIDLE_PRIORITY + 2)
#define TASK_PRIO_DEFENDER                      5
#define TASK_PRIO_SHADOW                        6
#define TASK_PRIO_PUBLISH                       7
#define TASK_PRIO_ENV                           8
#define TASK_PRIO_MOTION                        9
#define TASK_PRIO_CLI                           10
#define TASK_PRIO_MQTTA_AGENT                   11
#define TASK_PRIO_W6X                           (TASK_PRIO_MQTTA_AGENT + 1)
#define TASK_PRIO_SUBSCRIPTION                  25  /** Priority of the subscription process task        */

#define TASK_STACK_SIZE_OTA                     4096/** Stack size of the OAT process task               */
#define TASK_STACK_SIZE_SNTP                    2024/** Stack size of the vSNTPTask process task         */
#define TASK_STACK_SIZE_DEFENDER                2024/** Stack size of the AWSDefender process task       */
#define TASK_STACK_SIZE_SHADOW                  2024/** Stack size of the ShadowDevice process task      */
#define TASK_STACK_SIZE_PUBLISH                 2024/** Stack size of the publish process task           */
#define TASK_STACK_SIZE_ENV                     2024/** Stack size of the EnvSense process task          */
#define TASK_STACK_SIZE_MOTION                  2024/** Stack size of the MotionS process task           */
#define TASK_STACK_SIZE_CLI                     2048/** Stack size of the CLI process task               */
#define TASK_STACK_SIZE_MQTT_AGENT              2048/** Stack size of the MQTTAgent process task         */
#define TASK_STACK_SIZE_W6X                     1024/** Stack size of the W6X process task               */
#define TASK_STACK_SIZE_SUBSCRIPTION            1024/** Stack size of the MQTT subscription process task */

/******************** W6X debug config ********************/
#define W61_AT_LOG_ENABLE                       0         /* w61_driver_config.h */
#define SYS_DBG_ENABLE_TA4                      0         /* w61_driver_config.h */
#define W6X_TRACE_RECORDER_DBG_LEVEL            LOG_ERROR /* trcRecorder.h       */
/** Global verbosity level (LOG_NONE, LOG_ERROR, LOG_WARNING, LOG_INFO, LOG_DEBUG) */
#define W6X_LOG_LEVEL                           LOG_WARN

/******************** W6X Tasks config ********************/
#define W61_ATD_RX_TASK_PRIO                    20         /* w61_at_rx_parser.h, W61 AT Rx parser task priority, recommended to be higher than application tasks */
#define SPI_THREAD_PRIO                         17         /* spi_iface.c        */

/********************* Board config *********************/
#define democonfigMAX_THING_NAME_LENGTH         128

#define BOARD                                   "stm32n6570_dk"
#define democonfigDEVICE_PREFIX                 "stm32n6"
#define OTA_FILE_NAME                           "stm32n6570_dk_w6x_iot_reference_Appli.bin"
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

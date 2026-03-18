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
#define ARD_D14_Pin GPIO_PIN_1
#define ARD_D14_GPIO_Port GPIOC
#define ARD_D06_Pin GPIO_PIN_13
#define ARD_D06_GPIO_Port GPIOE
#define ARD_D13_Pin GPIO_PIN_15
#define ARD_D13_GPIO_Port GPIOE
#define ARD_D03_Pin GPIO_PIN_9
#define ARD_D03_GPIO_Port GPIOE
#define ARD_D03_EXTI_IRQn EXTI9_IRQn
#define ARD_D09_Pin GPIO_PIN_14
#define ARD_D09_GPIO_Port GPIOE
#define ARD_D08_Pin GPIO_PIN_7
#define ARD_D08_GPIO_Port GPIOE
#define ARD_D05_Pin GPIO_PIN_10
#define ARD_D05_GPIO_Port GPIOE
#define ARD_D12_Pin GPIO_PIN_8
#define ARD_D12_GPIO_Port GPIOH
#define USER_BUTTON_Pin GPIO_PIN_12
#define USER_BUTTON_GPIO_Port GPIOD
#define USER_BUTTON_EXTI_IRQn EXTI12_IRQn
#define ARD_D15_Pin GPIO_PIN_9
#define ARD_D15_GPIO_Port GPIOH
#define ARD_D07_Pin GPIO_PIN_6
#define ARD_D07_GPIO_Port GPIOD
#define ARD_D02_Pin GPIO_PIN_0
#define ARD_D02_GPIO_Port GPIOD
#define ARD_D01_Pin GPIO_PIN_5
#define ARD_D01_GPIO_Port GPIOD
#define ARD_D04_Pin GPIO_PIN_5
#define ARD_D04_GPIO_Port GPIOH
#define ARD_D00_Pin GPIO_PIN_6
#define ARD_D00_GPIO_Port GPIOF
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
#if 0
#define SPI_SLAVE_DATA_RDY_Pin                  ARD_D03_Pin
#define SPI_SLAVE_DATA_RDY_GPIO_Port            ARD_D03_GPIO_Port
#define SPI_SLAVE_DATA_RDY_EXTI_IRQn            ARD_D03_EXTI_IRQn

#define CHIP_EN_Pin                             ARD_D05_Pin
#define CHIP_EN_GPIO_Port                       ARD_D05_GPIO_Port

#define BOOT_Pin                                ARD_D06_Pin
#define BOOT_GPIO_Port                          ARD_D06_GPIO_Port


#define LP_WAKEUP_Pin                           ARD_D10_Pin
#define LP_WAKEUP_GPIO_Port                     ARD_D10_GPIO_Port

#else
/********** X-NUCLEO-WW611M1 Pin definition ***********/
#if !defined(SPI_RDY_Pin)
#define SPI_RDY_Pin                             ARD_D03_Pin
#define SPI_RDY_GPIO_Port                       ARD_D03_GPIO_Port
#define SPI_RDY_EXTI_IRQn                       ARD_D03_EXTI_IRQn
#endif

#if !defined(CHIP_EN_Pin)
#define CHIP_EN_Pin                             ARD_D05_Pin
#define CHIP_EN_GPIO_Port                       ARD_D05_GPIO_Port
#endif

#if !defined(BOOT_Pin)
#define BOOT_Pin                                ARD_D06_Pin
#define BOOT_GPIO_Port                          ARD_D06_GPIO_Port
#endif

#if !defined(SPI_CS_Pin)
#define SPI_CS_Pin                              ARD_D10_Pin
#define SPI_CS_GPIO_Port                        ARD_D10_GPIO_Port
#endif

#endif
/************ Board LED Pin configuration *************/
#define LED_RED_ON                              GPIO_PIN_RESET
#define LED_RED_OFF                             GPIO_PIN_SET

#define LED_GREEN_ON                            GPIO_PIN_RESET
#define LED_GREEN_OFF                           GPIO_PIN_SET

#if !defined(USER_Button_GPIO_Port)
#define USER_Button_GPIO_Port                   USER_BUTTON_GPIO_Port
#endif

#if !defined(USER_Button_Pin)
#define USER_Button_Pin                         USER_BUTTON_Pin
#endif

#define USER_BUTTON_ON                          GPIO_PIN_SET


/**************** MbedTLS debug config ****************/
#define MBEDTLS_DEBUG_NO_DEBUG                  0 /* No debug messages are displayed                                        */
#define MBEDTLS_DEBUG_ERROR                     1 /* Only error messages are shown                                          */
#define MBEDTLS_DEBUG_CHANGE                    2 /* Messages related to state changes in the SSL/TLS process are displayed */
#define MBEDTLS_DEBUG_INFO                      3 /* Provides general information about the SSL/TLS process                 */
#define MBEDTLS_DEBUG_VERBOSE                   4 /* Displays detailed debug information, including low-level operations    */

#define MBEDTLS_DEBUG_THRESHOLD                 MBEDTLS_DEBUG_NO_DEBUG

/******************** Tasks config ********************/
#define DEMO_PUB_SUB                            0   // Publish/Subscribe Example
#define DEMO_ENV_SENSOR                         0   // Environmental Sensor Example
#define DEMO_LIGHT_SENSOR                       0   // Light Sensor Example
#define DEMO_MOTION_SENSOR                      0   // Motion Sensor Example
#define DEMO_MOTION_IMU                         0   // IMU example
#define DEMO_RANGING_SENSOR                     0   // Ranging sensor Example
#define DEMO_LED                                0   // LED Control Example
#define DEMO_BUTTON                             0   // Button Status Example
#define DEMO_ECHO_SERVER                        0   // Echo server example
#define DEMO_ECHO_CLIENT                        0   // Echo Client example
#define DEMO_HOME_ASSISTANT                     0   // Home Assistant Discovery Example
#define DEMO_COVER                              0   // Garage door example
#define DEMO_AWS_SHADOW                         0   // AWS IoT Shadow Example
#define DEMO_AWS_OTA                            0   // OTA Update Example
//DEMO_AWS_FLEET_PROVISION                          // Defined in project properties->C C++ Build->Settings->Tool Settings->GCC Compiler

#define DEMO_PING                               1   // Ping example

#if (DEMO_MOTION_SENSOR && DEMO_MOTION_IMU)
#error Please select either DEMO_MOTION_SENSOR or DEMO_MOTION_IMU
#endif

#if !defined(ST67W6X_NCP)
#define DEMO_AWS_DEFENDER                       0   // AWS IoT Defender Example
#endif

#if defined(ST67W6X_NCP)
#define DEMO_SNTP                               0   // SNTP example
#endif

#define MQTT_ENABLED                            (DEMO_PUB_SUB       || \
                                                 DEMO_ENV_SENSOR    || \
												                         DEMO_LIGHT_SENSOR  || \
                                                 DEMO_MOTION_SENSOR || \
												                         DEMO_MOTION_IMU    || \
												                         DEMO_RANGING_SENSOR|| \
												                         DEMO_LED           || \
																		             DEMO_BUTTON        || \
																					       DEMO_HOME_ASSISTANT|| \
                                                 DEMO_AWS_OTA       || \
                                                 DEMO_AWS_SHADOW    || \
                                                 defined(DEMO_AWS_FLEET_PROVISION))

/******************** Tasks priority ********************/
#define TASK_PRIO_fleetProvisioning             (tskIDLE_PRIORITY      + 1 )
#define TASK_PRIO_SNTP                          (tskIDLE_PRIORITY      + 2 )
#define TASK_PRIO_PING                          (tskIDLE_PRIORITY      + 3 )
#define TASK_PRIO_ECHO_SERVER                   (tskIDLE_PRIORITY      + 4 )
#define TASK_PRIO_ECHO_CLIENT                   (tskIDLE_PRIORITY      + 5 )
#define TASK_PRIO_DEFENDER                      (tskIDLE_PRIORITY      + 6 )
#define TASK_PRIO_BUTTON                        (tskIDLE_PRIORITY      + 6 )
#define TASK_PRIO_RANGING                       (tskIDLE_PRIORITY      + 7 )
#define TASK_PRIO_SHADOW                        (tskIDLE_PRIORITY      + 8 )
#define TASK_PRIO_LED                           (tskIDLE_PRIORITY      + 9 )
#define TASK_PRIO_PUBLISH                       (tskIDLE_PRIORITY      + 10)
#define TASK_PRIO_LIGHT                         (tskIDLE_PRIORITY      + 11)
#define TASK_PRIO_ENV                           (tskIDLE_PRIORITY      + 12)
#define TASK_PRIO_MOTION                        (tskIDLE_PRIORITY      + 13)
#define TASK_PRIO_HOMEASSISTANT                 (tskIDLE_PRIORITY      + 14)
#define TASK_PRIO_OTA                           (tskIDLE_PRIORITY      + 1 )
#define TASK_PRIO_CLI                           (tskIDLE_PRIORITY      + 16)
#define TASK_PRIO_MQTTA_AGENT                   (tskIDLE_PRIORITY      + 17)
#define TASK_PRIO_W6X                           (TASK_PRIO_MQTTA_AGENT + 1 )
#define TASK_PRIO_MXCHIP                        (tskIDLE_PRIORITY      + 23)
#define TASK_PRIO_NET_ETH                       (TASK_PRIO_MQTTA_AGENT + 1 )
#define TASK_PRIO_SUBSCRIPTION                  (tskIDLE_PRIORITY      + 25)  /** Priority of the subscription process task        */

/******************** Tasks stack size ********************/
#define TASK_STACK_SIZE_OTA                     4096/** Stack size of the OAT process task               */
#define TASK_STACK_SIZE_SNTP                    2024/** Stack size of the vSNTPTask process task         */
#define TASK_STACK_SIZE_DEFENDER                2024/** Stack size of the AWSDefender process task       */
#define TASK_STACK_SIZE_SHADOW                  2024/** Stack size of the ShadowDevice process task      */
#define TASK_STACK_SIZE_LED                     1024/** Stack size of the LED process task               */
#define TASK_STACK_SIZE_BUTTON                  1024/** Stack size of the Button process task            */
#define TASK_STACK_SIZE_RANGING                 1024/** Stack size of the RANGING process task            */
#define TASK_STACK_SIZE_PUBLISH                 2024/** Stack size of the publish process task           */
#define TASK_STACK_SIZE_ENV                     1024/** Stack size of the EnvSense process task          */
#define TASK_STACK_SIZE_LIGHT                   1024/** Stack size of the LightSense process task        */
#define TASK_STACK_SIZE_MOTION                  1024/** Stack size of the MotionS process task           */
#define TASK_STACK_SIZE_HOMEASSISTANT           4096/** Stack size of the Home Assistant process task    */
#define TASK_STACK_SIZE_CLI                     2048/** Stack size of the CLI process task               */
#define TASK_STACK_SIZE_MQTT_AGENT              2048/** Stack size of the MQTTAgent process task         */
#define TASK_STACK_SIZE_PING                    2048/** Stack size of the PING process task              */
#define TASK_STACK_SIZE_W6X                     2048/** Stack size of the W6X process task               */
#define TASK_STACK_SIZE_MXCHIP                  1024/** Stack size of the MXCHIP process task            */
#define TASK_STACK_SIZE_SUBSCRIPTION            1024/** Stack size of the MQTT subscription process task */
#define TASK_STACK_SIZE_NET_ETH                 1024/** Stack size of the Ethernet task                  */
#define TASK_STACK_SIZE_fleetProvisioning       1024/** Stack size of the fleetProvisioning task         */

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

#if defined(ST67W6X_RCP)
#define CONNECTIVITY                            "ST67W6X_RCP"
#endif

/* Select where the certs and keys are located */
#if !defined(__USE_STSAFE__)
  #define KV_STORE_NVIMPL_LITTLEFS              1
  #define KV_STORE_NVIMPL_ARM_PSA               0
  #define KV_STORE_NVIMPL_STSAFE                0
#else
  #define KV_STORE_NVIMPL_LITTLEFS              0
  #define KV_STORE_NVIMPL_ARM_PSA               0
  #define KV_STORE_NVIMPL_STSAFE                1
#endif


#define DEFAULT_SSID "st_iot_demo"
#define DEFAULT_PSWD "stm32u585"
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

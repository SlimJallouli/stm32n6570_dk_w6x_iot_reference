/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : app_freertos.c
 * Description        : FreeRTOS applicative file
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

/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "logging_levels.h"
/* define LOG_LEVEL here if you want to modify the logging level from the default */
#if defined(LOG_LEVEL)
#undef LOG_LEVEL
#endif

#define LOG_LEVEL    LOG_INFO

#include "logging.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <string.h>

#include "kvstore.h"
#include "sys_evt.h"

#include "st67w6x_netconn.h"

#if MQTT_ENABLED
#include "mqtt_agent_task.h"
#endif

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
EventGroupHandle_t xSystemEvents = NULL;

/* USER CODE END Variables */


/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void vInitTask(void *pvArgs);

#if defined(LFS_CONFIG)
#include "lfs.h"
#include "lfs_port.h"

static lfs_t *pxLfsCtx = NULL;

static int fs_init(void);
lfs_t* pxGetDefaultFsCtx(void);
#endif /* LFS_CONFIG */

extern void vLEDTask                     ( void * pvParameters );
extern void vButtonTask                  ( void * pvParameters );
extern void vSubscribePublishTestTask    ( void * pvParameters );
/* USER CODE END FunctionPrototypes */

/* USER CODE BEGIN 5 */
void vApplicationMallocFailedHook(void)
{
  LogError("Malloc Fail\n");
  vDoSystemReset();
}
/* USER CODE END 5 */

/* USER CODE BEGIN 2 */
void vApplicationIdleHook(void)
{
  vPetWatchdog();
}
/* USER CODE END 2 */

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  taskENTER_CRITICAL();

  LogSys("Stack overflow in %s", pcTaskName);
  (void) xTask;

  vDoSystemReset();

  taskEXIT_CRITICAL();
}
/* USER CODE END 4 */

/* USER CODE BEGIN 1 */
/* Functions needed when configGENERATE_RUN_TIME_STATS is on */
__weak void configureTimerForRunTimeStats(void)
{

}

__weak unsigned long getRunTimeCounterValue(void)
{
  return 0;
}

static void checkAndClearResetFlags(void)
{
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET)
  {
    LogInfo("Reset source: PIN");
  }

  if ((__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != RESET) ||
      (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != RESET))
  {
    LogInfo("Reset source: BOR or POR/PDR");
  }

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET)
  {
    LogInfo("Reset source: Software");
  }

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
  {
    LogInfo("Reset source: IWDG");
  }

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET)
  {
    LogInfo("Reset source: WWDG");
  }

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != RESET)
  {
    LogInfo("Reset source: Low power");
  }

  /* Clear all reset flags */
  __HAL_RCC_CLEAR_RESET_FLAGS();
}
/* USER CODE END 1 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void)
{
  /* USER CODE BEGIN Init */
  BaseType_t xResult;

  /* Initialize uart for logging before cli is up and running */
  vInitLoggingEarly();

  vLoggingInit();

  LogInfo("HW Init Complete.");
  checkAndClearResetFlags();

  LogInfo("Build Date: %s\n", __DATE__);
  LogInfo("Build Time: %s\n", __TIME__);

#if defined(HW_CRYPTO)
  LogInfo("HW Crypto enabled");
#else
  LogInfo("Software Crypto");
#endif

#if defined(HAL_IWDG_MODULE_ENABLED)
  LogInfo("IWDG Enabled");
#endif

  xResult = xTaskCreate(StartDefaultTask, "DefaultTask", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY, NULL);
  configASSERT(xResult == pdTRUE);

  vTaskStartScheduler();
}
/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief Function implementing the defaultTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN defaultTask */

#if defined(LFS_CONFIG)
  int xMountStatus;
#endif
  (void) argument;

  xTaskCreate(Task_CLI, "cli", TASK_STACK_SIZE_CLI, NULL, TASK_PRIO_CLI, NULL);

  LogInfo("Task started: %s\n", __func__);

  xSystemEvents = xEventGroupCreate();

#if defined(LFS_CONFIG)
  xMountStatus = fs_init();

  if (xMountStatus == LFS_ERR_OK)
  {
    LogInfo("File System mounted.");
    (void) xEventGroupSetBits(xSystemEvents, EVT_MASK_FS_READY);

    KVStore_init();
  }
  else
  {
    LogError("Failed to mount file system.");

    while(1)
    {
    	vTaskDelay(1000);
    }
  }
#endif

  (void) xEventGroupClearBits( xSystemEvents, EVT_MASK_NET_CONNECTED );

#if defined(LFS_CONFIG)
#if !defined(__USE_STSAFE__)
  size_t xLength;

  KVStore_getStringHeap(CS_CORE_THING_NAME, &xLength);

  if ((xLength == 0) || (xLength == -1))
  {
    char *democonfigFP_DEMO_ID = pvPortMalloc(democonfigMAX_THING_NAME_LENGTH);

#if defined(HAL_ICACHE_MODULE_ENABLED)
  HAL_ICACHE_Disable();
#endif

    uint32_t uid0 = HAL_GetUIDw0();
    uint32_t uid1 = HAL_GetUIDw1();
    uint32_t uid2 = HAL_GetUIDw2();

#if defined(HAL_ICACHE_MODULE_ENABLED)
  HAL_ICACHE_Enable();
#endif

    snprintf(democonfigFP_DEMO_ID, democonfigMAX_THING_NAME_LENGTH, democonfigDEVICE_PREFIX"-%08X%08X%08X", (int)uid0, (int)uid1, (int)uid2);

    /* Update the KV Store */
    KVStore_setString(CS_CORE_THING_NAME, democonfigFP_DEMO_ID);

    /* Update the KV Store */
    KVStore_xCommitChanges();

    vPortFree(democonfigFP_DEMO_ID);
  }
#endif /* __USE_STSAFE__ */
#endif /* LFS_CONFIG */

#if defined(ST67W6X_RCP)
  xTaskCreate(net_main, "W6xNet", TASK_STACK_SIZE_W6X, NULL, TASK_PRIO_W6X, NULL);
#endif

#if MQTT_ENABLED
  xTaskCreate(vMQTTAgentTask, "MQTTAgent", TASK_STACK_SIZE_MQTT_AGENT, NULL, TASK_PRIO_MQTTA_AGENT, NULL);
#endif

#if DEMO_LED
      xTaskCreate(vLEDTask, "LEDTask", TASK_STACK_SIZE_LED, NULL, TASK_PRIO_LED, NULL);
#endif

#if DEMO_BUTTON
      xTaskCreate(vButtonTask, "ButtonTask", TASK_STACK_SIZE_BUTTON, NULL, TASK_PRIO_BUTTON, NULL);
#endif

#if DEMO_PUB_SUB
  xTaskCreate(vSubscribePublishTestTask, "PubSub", TASK_STACK_SIZE_PUBLISH, NULL, TASK_PRIO_PUBLISH, NULL);
#endif

  /* Infinite loop */
  for (;;)
  {
    vTaskDelete( NULL);
  }
  /* USER CODE END defaultTask */
}


/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
#if 1//defined(LFS_CONFIG)
lfs_t* pxGetDefaultFsCtx(void)
{
  while (pxLfsCtx == NULL)
  {
    LogDebug( "Waiting for FS Initialization." );
    /* Wait for FS to be initialized */
    vTaskDelay(1000);
    /*TODO block on an event group bit instead */
  }

  return pxLfsCtx;
}

static int fs_init(void)
{
  static lfs_t xLfsCtx = { 0 };
  struct lfs_info xDirInfo = { 0 };

  /* Block time of up to 1 s for filesystem to initialize */
#if defined(LFS_USE_INTERNAL_NOR)
  const struct lfs_config *pxCfg = pxInitializeInternalFlashFs (pdMS_TO_TICKS(30 * 1000));
#elif defined(HAL_OSPI_MODULE_ENABLED)
  const struct lfs_config *pxCfg = pxInitializeOSPIFlashFs     (pdMS_TO_TICKS(30 * 1000));
#elif defined(HAL_XSPI_MODULE_ENABLED)
  const struct lfs_config *pxCfg = pxInitializeXSPIFlashFs     (pdMS_TO_TICKS(30 * 1000));
#endif

  /* mount the filesystem */
  int err = lfs_mount(&xLfsCtx, pxCfg);

  /* format if we can't mount the filesystem
   * this should only happen on the first boot
   */
  if (err != LFS_ERR_OK)
  {
    LogError("Failed to mount partition. Formatting...");
    err = lfs_format(&xLfsCtx, pxCfg);

    if (err == 0)
    {
      err = lfs_mount(&xLfsCtx, pxCfg);
    }

    if (err != LFS_ERR_OK)
    {
      LogError("Failed to format littlefs device.");
    }
  }

  if (lfs_stat(&xLfsCtx, "/cfg", &xDirInfo) == LFS_ERR_NOENT)
  {
    err = lfs_mkdir(&xLfsCtx, "/cfg");

    if (err != LFS_ERR_OK)
    {
      LogError("Failed to create /cfg directory.");
    }
  }

  if (lfs_stat(&xLfsCtx, "/ota", &xDirInfo) == LFS_ERR_NOENT)
  {
    err = lfs_mkdir(&xLfsCtx, "/ota");

    if (err != LFS_ERR_OK)
    {
      LogError("Failed to create /ota directory.");
    }
  }

  if (err == 0)
  {
    /* Export the FS context */
    pxLfsCtx = &xLfsCtx;
  }

  return err;
}
#endif
static uint32_t DisableSuppressTicksAndSleepBm;

void DisableSuppressTicksAndSleep(uint32_t bitmask)
{
  taskENTER_CRITICAL();

  DisableSuppressTicksAndSleepBm |= bitmask;

  taskEXIT_CRITICAL();
}

void EnableSuppressTicksAndSleep(uint32_t bitmask)
{
  taskENTER_CRITICAL();

  DisableSuppressTicksAndSleepBm  &= (~bitmask);

  taskEXIT_CRITICAL();
}
/* USER CODE END Application */


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
#if defined(LFS_CONFIG)
#include "lfs.h"
#include "lfs_port.h"
#endif
#include "kvstore.h"
#include "sys_evt.h"

#include "w6x_wifi_netconn.h"

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
#if defined(LFS_CONFIG)
static lfs_t *pxLfsCtx = NULL;
#endif
/* USER CODE END Variables */


/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void vInitTask(void *pvArgs);
#if defined(LFS_CONFIG)
static int fs_init(void);
lfs_t* pxGetDefaultFsCtx(void);
#endif

extern void otaPal_EarlyInit(void);

extern void vSubscribePublishTestTask    ( void * pvParameters );
extern void vDefenderAgentTask           ( void * pvParameters );
extern void vShadowDeviceTask            ( void * pvParameters );
extern void vOTAUpdateTask               ( void * pvParameters );
extern void vEnvironmentSensorPublishTask( void * pvParameters );
extern void vMotionSensorsPublish        ( void * pvParameters );
extern void vSNTPTask                    ( void * pvParameters );
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

//  LogInfo("Build Date: %s\n", __DATE__);
//  LogInfo("Build Time: %s\n", __TIME__);

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
  BaseType_t xResult;
#if defined(LFS_CONFIG)
  int xMountStatus;
#endif
  (void) argument;

  LogInfo("Task started: %s\n", __func__);

  xSystemEvents = xEventGroupCreate();

  xResult = xTaskCreate(Task_CLI, "cli", TASK_STACK_SIZE_CLI, NULL, TASK_PRIO_CLI, NULL);

#if defined(LFS_CONFIG)
  xMountStatus = fs_init();

  if (xMountStatus == LFS_ERR_OK)
  {
    LogInfo("File System mounted.");
    (void) xEventGroupSetBits(xSystemEvents, EVT_MASK_FS_READY);

#if DEMO_OTA
    otaPal_EarlyInit();
#endif

    KVStore_init();
  }
  else
  {
    LogError("Failed to mount file system.");
  }
#endif
  (void) xEventGroupClearBits( xSystemEvents, EVT_MASK_NET_CONNECTED );

#if defined(ST67W6X)
  xResult = xTaskCreate(W6X_WiFi_Task, "w6x_wifi", TASK_STACK_SIZE_W6X, NULL, TASK_PRIO_W6X, NULL);
#endif

#if MQTT_ENABLED
  xResult = xTaskCreate(vMQTTAgentTask, "MQTTAgent", TASK_STACK_SIZE_MQTT_AGENT, NULL, TASK_PRIO_MQTTA_AGENT, NULL);
#endif

#if DEMO_PUB_SUB
  xResult = xTaskCreate(vSubscribePublishTestTask, "PubSub", TASK_STACK_SIZE_PUBLISH, NULL, TASK_PRIO_PUBLISH, NULL);
#endif

#if DEMO_ENV_SENSOR
  xResult = xTaskCreate(vEnvironmentSensorPublishTask, "EnvSense", TASK_STACK_SIZE_ENV, NULL, TASK_PRIO_ENV, NULL);
#endif

#if DEMO_MOTION_SENSOR
  xResult = xTaskCreate(vMotionSensorsPublish, "MotionS", TASK_STACK_SIZE_MOTION, NULL, TASK_PRIO_MOTION, NULL);
#endif

#if (DEMO_OTA || DEMO_SHADOW)
  char * pucMqttEndpoint;
  size_t uxMqttEndpointLen;

  pucMqttEndpoint = KVStore_getStringHeap( CS_CORE_MQTT_ENDPOINT, &uxMqttEndpointLen );

  if ((uxMqttEndpointLen>0) && (uxMqttEndpointLen < 0xffffffff))
  {
    /* If we are connecting to AWS */
    if (strstr(pucMqttEndpoint, "amazonaws") != NULL)
    {
#if DEMO_OTA
      xResult = xTaskCreate(vOTAUpdateTask, "OTAUpdate", TASK_STACK_SIZE_OTA, NULL, TASK_PRIO_OTA, NULL);
#endif

#if DEMO_SHADOW
      xResult = xTaskCreate(vShadowDeviceTask, "ShadowDevice", TASK_STACK_SIZE_SHADOW, NULL, TASK_PRIO_SHADOW, NULL);
#endif
    }

    vPortFree(pucMqttEndpoint);
  }
#endif /* (DEMO_OTA || DEMO_SHADOW) */

#if DEMO_SNTP
  xResult = xTaskCreate(vSNTPTask, "vSNTPTask", TASK_STACK_SIZE_SNTP, NULL, TASK_PRIO_SNTP, NULL);
#endif

  /* Infinite loop */
  for (;;)
  {
    vTaskDelete( NULL);
    vTaskDelay(1);
  }
  /* USER CODE END defaultTask */
}


/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
#if defined(LFS_CONFIG)
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


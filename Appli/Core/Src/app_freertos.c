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
#if 1//defined(LFS_CONFIG)

#endif
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

#if 1//defined(LFS_CONFIG)
#include "lfs.h"
#include "lfs_port.h"

static lfs_t *pxLfsCtx = NULL;

static int fs_init(void);
lfs_t* pxGetDefaultFsCtx(void);
#endif /* LFS_CONFIG */


extern void otaPal_EarlyInit(void);

extern void vSubscribePublishTestTask    ( void * pvParameters );
extern void vDefenderAgentTask           ( void * pvParameters );
extern void vShadowDeviceTask            ( void * pvParameters );
extern void vOTAUpdateTask               ( void * pvParameters );
extern void vEnvironmentSensorPublishTask( void * pvParameters );
extern void vMotionSensorsPublish        ( void * pvParameters );
extern void vSNTPTask                    ( void * pvParameters );
extern void prvFleetProvisioningTask     ( void * pvParameters );
extern void vDefenderAgentTask           ( void * pvParameters );
extern void vLEDTask                     ( void * pvParameters );
extern void vButtonTask                  ( void * pvParameters );
extern void vHAConfigPublishTask         ( void * pvParameters );
extern void vCoverTask                   ( void * pvParameters );
extern void vRangingSensorTask           ( void * pvParameters );
extern void vIMUTask                     ( void * pvParameters );
extern void ping_task                    ( void * pvParameters );
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
#if (DEMO_AWS_OTA || AWS_DEMO_AWS_SHADOW)
  char * pucMqttEndpoint;
  size_t uxMqttEndpointLen = -1;
#endif

#if defined(DEMO_AWS_FLEET_PROVISION) && !defined(__USE_STSAFE__)
  BaseType_t xSuccess = pdTRUE;
  uint32_t provisioned = 0;
#endif

#if defined(LFS_CONFIG)
  int xMountStatus;
#endif
  (void) argument;

  LogInfo("Task started: %s\n", __func__);

  xSystemEvents = xEventGroupCreate();

  xTaskCreate(Task_CLI, "cli", TASK_STACK_SIZE_CLI, NULL, TASK_PRIO_CLI, NULL);

#if defined(LFS_CONFIG)
  xMountStatus = fs_init();

  if (xMountStatus == LFS_ERR_OK)
  {
    LogInfo("File System mounted.");
    (void) xEventGroupSetBits(xSystemEvents, EVT_MASK_FS_READY);

    KVStore_init();

#if (DEMO_AWS_OTA || AWS_DEMO_AWS_SHADOW)
    pucMqttEndpoint = KVStore_getStringHeap( CS_CORE_MQTT_ENDPOINT, &uxMqttEndpointLen );
#endif

#if DEMO_OTA
    if ((uxMqttEndpointLen>0) && (uxMqttEndpointLen < 0xffffffff))
    {
      /* If we are connecting to AWS */
      if (strstr(pucMqttEndpoint, "amazonaws") != NULL)
      {
        otaPal_EarlyInit();
      }
    }
#endif
  }
  else
  {
    LogError("Failed to mount file system.");
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

#if defined(DEMO_AWS_FLEET_PROVISION) && !defined(__USE_STSAFE__)
    KVStore_setUInt32(CS_PROVISIONED, 0);
    KVStore_setString(CS_CORE_THING_NAME, democonfigFP_DEMO_ID);
#endif

    /* Update the KV Store */
    KVStore_xCommitChanges();

    vPortFree(democonfigFP_DEMO_ID);
  }
#endif /* __USE_STSAFE__ */
#endif /* LFS_CONFIG */

#if defined(ST67W6X_RCP)
  xTaskCreate(net_main, "W6xNet", TASK_STACK_SIZE_W6X, NULL, TASK_PRIO_W6X, NULL);
#endif

#if DEMO_ECHO_SERVER
  xTaskCreate(vEchoServerTask, "EchoServer", TASK_STACK_SIZE_MQTT_AGENT, NULL, TASK_PRIO_MQTTA_AGENT, NULL);
#endif

#if DEMO_ECHO_CLIENT
  xTaskCreate(vEchoClientTask, "EchoClient", TASK_STACK_SIZE_MQTT_AGENT, NULL, TASK_PRIO_MQTTA_AGENT, NULL);
#endif

#if (DEMO_PING && !defined(ST67W6X_NCP))
  xTaskCreate(ping_task, "PingTask", TASK_STACK_SIZE_PING, NULL, TASK_PRIO_PING, NULL);
#endif

#if MQTT_ENABLED
  xTaskCreate(vMQTTAgentTask, "MQTTAgent", TASK_STACK_SIZE_MQTT_AGENT, NULL, TASK_PRIO_MQTTA_AGENT, NULL);
#endif

#if defined(DEMO_AWS_FLEET_PROVISION) && !defined(__USE_STSAFE__)
  provisioned = KVStore_getUInt32( CS_PROVISIONED, &( xSuccess ) );

  if(provisioned == 0)
  {
    xTaskCreate(prvFleetProvisioningTask, "FleetProv", TASK_STACK_SIZE_fleetProvisioning, NULL, TASK_PRIO_fleetProvisioning, NULL);

    vTaskDelete( NULL);
  }
#endif

#if DEMO_PUB_SUB
  xTaskCreate(vSubscribePublishTestTask, "PubSub", TASK_STACK_SIZE_PUBLISH, NULL, TASK_PRIO_PUBLISH, NULL);
#endif

#if DEMO_ENV_SENSOR
  xTaskCreate(vEnvironmentSensorPublishTask, "EnvSense", TASK_STACK_SIZE_ENV, NULL, TASK_PRIO_ENV, NULL);
#endif

#if DEMO_MOTION_SENSOR
  xTaskCreate(vMotionSensorsPublish, "MotionS", TASK_STACK_SIZE_MOTION, NULL, TASK_PRIO_MOTION, NULL);
#endif

#if DEMO_MOTION_IMU
  xTaskCreate(vIMUTask, "IMU", TASK_STACK_SIZE_MOTION, NULL, TASK_PRIO_MOTION, NULL);
#endif

#if DEMO_HOME_ASSISTANT
      xTaskCreate(vHAConfigPublishTask, "HomeAssistant", TASK_STACK_SIZE_HOMEASSISTANT, NULL, TASK_PRIO_HOMEASSISTANT, NULL);
#endif

#if DEMO_LED
      xTaskCreate(vLEDTask, "LEDTask", TASK_STACK_SIZE_LED, NULL, TASK_PRIO_LED, NULL);
#endif

#if DEMO_BUTTON
      xTaskCreate(vButtonTask, "ButtonTask", TASK_STACK_SIZE_BUTTON, NULL, TASK_PRIO_BUTTON, NULL);
#endif

#if DEMO_COVER
      xTaskCreate(vCoverTask, "CoverTask", TASK_STACK_SIZE_BUTTON, NULL, TASK_PRIO_BUTTON, NULL);
#endif

#if (DEMO_RANGING_SENSOR || USE_RANGING_SENSOR)
      xTaskCreate(vRangingSensorTask, "RangingTask", TASK_STACK_SIZE_RANGING, NULL, TASK_PRIO_RANGING, NULL);
#endif

#if (DEMO_AWS_OTA || AWS_DEMO_AWS_SHADOW)
  if ((uxMqttEndpointLen>0) && (uxMqttEndpointLen < 0xffffffff))
  {
    /* If we are connecting to AWS */
    if (strstr(pucMqttEndpoint, "amazonaws") != NULL)
    {
#if DEMO_AWS_OTA
      xTaskCreate(vOTAUpdateTask, "OTAUpdate", TASK_STACK_SIZE_OTA, NULL, TASK_PRIO_OTA, NULL);
#endif

#if DEMO_AWS_SHADOW
      xTaskCreate(vShadowDeviceTask, "ShadowDevice", TASK_STACK_SIZE_SHADOW, NULL, TASK_PRIO_SHADOW, NULL);
#endif

#if DEMO_AWS_DEFENDER && !defined(ST67W6X_NCP)
      xTaskCreate(vDefenderAgentTask, "AWSDefender", TASK_STACK_SIZE_DEFENDER, NULL, TASK_PRIO_DEFENDER, NULL);
#endif
    }
  }

  if(pucMqttEndpoint != NULL)
  {
    vPortFree(pucMqttEndpoint);
  }
#endif

#if DEMO_SNTP
  xTaskCreate(vSNTPTask, "vSNTPTask", TASK_STACK_SIZE_SNTP, NULL, TASK_PRIO_SNTP, NULL);
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


/*
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 * Derived from simple_sub_pub_demo.c
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#include "logging_levels.h"
/* define LOG_LEVEL here if you want to modify the logging level from the default */

#define LOG_LEVEL    LOG_INFO

#include "logging.h"

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "kvstore.h"

/* MQTT library includes. */
#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "sys_evt.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

/* Sensor includes */
#if USE_SENSORS
#include "hts221.h"
#include "lps22hh.h"
#endif

#if USE_SENSORS
#include "custom_bus_os.h"
#include "custom_errno.h"
static HTS221_Object_t HTS221_Obj;
static LPS22HH_Object_t LPS22HH_Obj;
#else
#define BSP_ERROR_NONE 0
#endif

typedef struct
{
  float_t fTemperature0;
  float_t fTemperature1;
  float_t fHumidity;
  float_t fBarometricPressure;
} EnvironmentalSensorData_t;

#define MQTT_PUBLISH_MAX_LEN                 ( 512 )
#define MQTT_PUBLISH_TIME_BETWEEN_MS         ( 1000 )
#define MQTT_PUBLISH_TOPIC                   "env_sensor_data"
#define MQTT_PUBLICH_TOPIC_STR_LEN           ( 256 )
#define MQTT_PUBLISH_BLOCK_TIME_MS           ( 1000 )
#define MQTT_PUBLISH_NOTIFICATION_WAIT_MS    ( 1000 )

#define MQTT_NOTIFY_IDX                      ( 1 )
#define MQTT_PUBLISH_QOS                     ( MQTTQoS0 )

extern UBaseType_t uxRand(void);

/*-----------------------------------------------------------*/

/**
 * @brief Defines the structure to use as the command callback context in this
 * demo.
 */
struct MQTTAgentCommandContext
{
  MQTTStatus_t xReturnStatus;
  TaskHandle_t xTaskToNotify;
};

/*-----------------------------------------------------------*/

static void prvPublishCommandCallback(MQTTAgentCommandContext_t *pxCommandContext, MQTTAgentReturnInfo_t *pxReturnInfo)
{
  configASSERT(pxCommandContext != NULL);
  configASSERT(pxReturnInfo != NULL);

  pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;

  if (pxCommandContext->xTaskToNotify != NULL)
  {
    /* Send the context's ulNotificationValue as the notification value so
     * the receiving task can check the value it set in the context matches
     * the value it receives in the notification. */
    (void) xTaskNotifyGiveIndexed(pxCommandContext->xTaskToNotify, MQTT_NOTIFY_IDX);
  }
}

/*-----------------------------------------------------------*/

static BaseType_t prvPublishAndWaitForAck(MQTTAgentHandle_t xAgentHandle, const char *pcTopic, const void *pvPublishData, size_t xPublishDataLen)
{
  BaseType_t xResult = pdFALSE;
  MQTTStatus_t xStatus;

  configASSERT(pcTopic != NULL);
  configASSERT(pvPublishData != NULL);
  configASSERT(xPublishDataLen > 0);

  MQTTPublishInfo_t xPublishInfo =
  { .qos = MQTT_PUBLISH_QOS, .retain = 0, .dup = 0, .pTopicName = pcTopic, .topicNameLength = strlen(pcTopic), .pPayload = pvPublishData, .payloadLength = xPublishDataLen };

  MQTTAgentCommandContext_t xCommandContext =
  { .xTaskToNotify = xTaskGetCurrentTaskHandle(), .xReturnStatus = MQTTIllegalState, };

  MQTTAgentCommandInfo_t xCommandParams =
  { .blockTimeMs = MQTT_PUBLISH_BLOCK_TIME_MS, .cmdCompleteCallback = prvPublishCommandCallback, .pCmdCompleteCallbackContext = &xCommandContext, };

  /* Clear the notification index */
  xTaskNotifyStateClearIndexed(NULL, MQTT_NOTIFY_IDX);

  xStatus = MQTTAgent_Publish(xAgentHandle, &xPublishInfo, &xCommandParams);

  if (xStatus == MQTTSuccess)
  {
    xResult = ulTaskNotifyTakeIndexed(MQTT_NOTIFY_IDX, pdTRUE, pdMS_TO_TICKS( MQTT_PUBLISH_NOTIFICATION_WAIT_MS ));

    if (xResult == 0)
    {
      LogError("Timed out while waiting for publish ACK or Sent event. xTimeout = %d", pdMS_TO_TICKS( MQTT_PUBLISH_NOTIFICATION_WAIT_MS ));
      xResult = pdFALSE;
    }
    else if (xCommandContext.xReturnStatus != MQTTSuccess)
    {
      LogError("MQTT Agent returned error code: %d during publish operation.", xCommandContext.xReturnStatus);
      xResult = pdFALSE;
    }
  }
  else
  {
    LogError("MQTTAgent_Publish returned error code: %d.", xStatus);
  }

  return xResult;
}

/*-----------------------------------------------------------*/

static BaseType_t xIsMqttConnected(void)
{
  /* Wait for MQTT to be connected */
  EventBits_t uxEvents = xEventGroupWaitBits(xSystemEvents,
  EVT_MASK_MQTT_CONNECTED,
  pdFALSE,
  pdTRUE, 0);

  return ((uxEvents & EVT_MASK_MQTT_CONNECTED) == EVT_MASK_MQTT_CONNECTED);
}

/*-----------------------------------------------------------*/

static BaseType_t xInitSensors(void)
{
#if USE_SENSORS
  uint8_t HTS221_Id;
  uint8_t Status;
  HTS221_IO_t HTS221_io_ctx =
  { 0 };

  uint8_t LPS22HH_Id;
  LPS22HH_IO_t LPS22HH_io_ctx =
  { 0 };

  /* Configure the driver */
  HTS221_io_ctx.BusType = HTS221_I2C_BUS; /* I2C */
  HTS221_io_ctx.Address = HTS221_I2C_ADDRESS;
  HTS221_io_ctx.Init = BSP_I2C2_Init_OS;
  HTS221_io_ctx.DeInit = BSP_I2C2_DeInit_OS;
  HTS221_io_ctx.ReadReg = BSP_I2C2_ReadReg_OS;
  HTS221_io_ctx.WriteReg = BSP_I2C2_WriteReg_OS;

  HTS221_RegisterBusIO(&HTS221_Obj, &HTS221_io_ctx);
  HTS221_Init(&HTS221_Obj);
  HTS221_ReadID(&HTS221_Obj, &HTS221_Id);

  if (HTS221_Id != HTS221_ID)
  {
    return HTS221_ERROR;
  }

  HTS221_HUM_Enable(&HTS221_Obj);

  do
  {
    vTaskDelay(5);
    HTS221_HUM_Get_DRDY_Status(&HTS221_Obj, &Status);
  } while (Status != 1);

  do
  {
    vTaskDelay(5);
    HTS221_TEMP_Get_DRDY_Status(&HTS221_Obj, &Status);
  } while (Status != 1);

#define LPS22HH_I2C_ADDRESS 0xBB
  /* Configure the driver */
  LPS22HH_io_ctx.BusType = LPS22HH_I2C_BUS; /* I2C */
  LPS22HH_io_ctx.Address = LPS22HH_I2C_ADDRESS;
  LPS22HH_io_ctx.Init = BSP_I2C2_Init_OS;
  LPS22HH_io_ctx.DeInit = BSP_I2C2_DeInit_OS;
  LPS22HH_io_ctx.ReadReg = BSP_I2C2_ReadReg_OS;
  LPS22HH_io_ctx.WriteReg = BSP_I2C2_WriteReg_OS;

  LPS22HH_RegisterBusIO(&LPS22HH_Obj, &LPS22HH_io_ctx);
  LPS22HH_Init(&LPS22HH_Obj);
  LPS22HH_ReadID(&LPS22HH_Obj, &LPS22HH_Id);

  if (LPS22HH_Id != LPS22HH_ID)
  {
    return LPS22HH_ERROR;
  }

  LPS22HH_TEMP_Enable(&LPS22HH_Obj);
  LPS22HH_PRESS_Enable(&LPS22HH_Obj);

  do
  {
    vTaskDelay(5);
    LPS22HH_PRESS_Get_DRDY_Status(&LPS22HH_Obj, &Status);
  } while (Status != 1);

  do
  {
    vTaskDelay(5);
    LPS22HH_TEMP_Get_DRDY_Status(&LPS22HH_Obj, &Status);
  } while (Status != 1);

#endif
  return pdTRUE;
}

/*-----------------------------------------------------------*/

static BaseType_t xUpdateSensorData(EnvironmentalSensorData_t *pxData)
{
  int32_t lBspError = BSP_ERROR_NONE;

#if USE_SENSORS
  lBspError  = HTS221_HUM_GetHumidity    (&HTS221_Obj, &pxData->fHumidity);
  lBspError += HTS221_TEMP_GetTemperature(&HTS221_Obj, &pxData->fTemperature0);

  lBspError += LPS22HH_PRESS_GetPressure  (&LPS22HH_Obj, &pxData->fBarometricPressure);
  lBspError += LPS22HH_TEMP_GetTemperature(&LPS22HH_Obj, &pxData->fTemperature1);
#else
  pxData->fHumidity           += 5.0f;
  pxData->fTemperature0       += 7.0f;
  pxData->fBarometricPressure += 100.0f;
  pxData->fTemperature1       += 4.0f;

  pxData->fHumidity           = fmod(pxData->fHumidity          , 100.0f);
  pxData->fTemperature0       = fmod(pxData->fTemperature0      , 50.0f);
  pxData->fBarometricPressure = fmod(pxData->fBarometricPressure, 100.0f);
  pxData->fTemperature1       = fmod(pxData->fTemperature1      , 50.0f);
#endif

  return lBspError == BSP_ERROR_NONE;
}

/*-----------------------------------------------------------*/

void vEnvironmentSensorPublishTask(void *pvParameters)
{
  BaseType_t xResult   = pdFALSE;
  BaseType_t xExitFlag = pdFALSE;

  MQTTAgentHandle_t xAgentHandle = NULL;
  char * pcPayloadBuf            = NULL;
  char * pcTopicString           = NULL;
  char * pcDeviceId              = NULL;

  size_t uxTopicLen   = 0;
  EnvironmentalSensorData_t xSensorData = { 0 };

  pcPayloadBuf  = pvPortMalloc(MQTT_PUBLISH_MAX_LEN      );
  pcTopicString = pvPortMalloc(MQTT_PUBLICH_TOPIC_STR_LEN);

  (void) pvParameters;

  xResult = xInitSensors();

  if (xResult != pdTRUE)
  {
    LogError("Error while initializing sensors.");
    vTaskDelete( NULL);
  }

  pcDeviceId = KVStore_getStringHeap( CS_CORE_THING_NAME, NULL );

  if( pcDeviceId == NULL )
  {
      xExitFlag = pdTRUE;
  }
  else
  {
    uxTopicLen = snprintf( pcTopicString, ( size_t ) MQTT_PUBLICH_TOPIC_STR_LEN, "%s/%s", pcDeviceId, MQTT_PUBLISH_TOPIC );
    vPortFree( pcDeviceId );
  }

  if ((uxTopicLen == 0) || (uxTopicLen >= MQTT_PUBLICH_TOPIC_STR_LEN))
  {
    LogError("Failed to construct topic string.");
    xExitFlag = pdTRUE;
  }

  vSleepUntilMQTTAgentReady();

  xAgentHandle = xGetMqttAgentHandle();

  while (xExitFlag == pdFALSE)
  {
    TickType_t xTicksToWait = pdMS_TO_TICKS(MQTT_PUBLISH_TIME_BETWEEN_MS);
    TimeOut_t xTimeOut;

    vTaskSetTimeOutState(&xTimeOut);

    xResult = xUpdateSensorData(&xSensorData);

    if (xResult != pdTRUE)
    {
      LogError("Error while reading sensor data.");
    }
    else if (xIsMqttConnected() == pdTRUE)
    {
      int lbytesWritten = 0;

      /* Write to */
      lbytesWritten = snprintf(pcPayloadBuf,
                              MQTT_PUBLISH_MAX_LEN,
                              "{ \"temp_0_c\": %f, \"rh_pct\": %f, \"temp_1_c\": %f, \"baro_mbar\": %f }",
                              xSensorData.fTemperature0,
                              xSensorData.fHumidity,
                              xSensorData.fTemperature1,
                              xSensorData.fBarometricPressure);

      if( ( lbytesWritten < MQTT_PUBLISH_MAX_LEN ) && ( xIsMqttAgentConnected() == pdTRUE ) )
      {
        xResult = prvPublishAndWaitForAck(xAgentHandle,
                                          pcTopicString,
                                          pcPayloadBuf,
                                          ( size_t ) lbytesWritten );

        if( xResult != pdPASS )
        {
            LogError( "Failed to publish motion sensor data" );
        }
      }
    }

    /* Adjust remaining tick count */
    if (xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait) == pdFALSE)
    {
      /* Wait until its time to poll the sensors again */
      vTaskDelay(xTicksToWait);
    }
  }
}

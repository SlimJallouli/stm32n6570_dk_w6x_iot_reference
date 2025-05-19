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

#define LOG_LEVEL    LOG_ERROR

#include "logging.h"


/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "kvstore.h"

#include "sys_evt.h"


/* MQTT library includes. */
#include "core_mqtt.h"

/* MQTT agent include. */
#include "core_mqtt_agent.h"
#include "mqtt_agent_task.h"

/* Subscription manager header include. */
#include "subscription_manager.h"

#if USE_SENSORS
/* Sensor includes */
#include "ism330dhcx.h"
#include "iis2mdc.h"

#include "custom_bus_os.h"
#include "custom_errno.h"
static ISM330DHCX_Object_t ISM330DHCX_Obj;
static IIS2MDC_Object_t    IIS2MDC_Obj;

typedef struct
{
  ISM330DHCX_Axes_t xAcceleroAxes;
  ISM330DHCX_Axes_t xGyroAxes;
  IIS2MDC_Axes_t    xMagnetoAxes;
} MotionSensorData_t;

#else

#define BSP_ERROR_NONE 0

typedef struct
{
  int32_t x;
  int32_t y;
  int32_t z;
} SensorData_t;

typedef struct
{
  SensorData_t xAcceleroAxes;
  SensorData_t xGyroAxes;
  SensorData_t xMagnetoAxes;
} MotionSensorData_t;
#endif

/**
 * @brief Size of statically allocated buffers for holding topic names and
 * payloads.
 */
#define MQTT_PUBLISH_MAX_LEN                 ( 200 )
#define MQTT_PUBLISH_TIME_BETWEEN_MS         ( 500 )
#define MQTT_PUBLISH_TOPIC                   "motion_sensor_data"
#define MQTT_PUBLICH_TOPIC_STR_LEN           ( 256 )
#define MQTT_PUBLISH_BLOCK_TIME_MS           ( 200 )
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
    uint32_t ulNotificationValue;
};

/*-----------------------------------------------------------*/
static void prvPublishCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                       MQTTAgentReturnInfo_t * pxReturnInfo )
{
    TaskHandle_t xTaskHandle = ( TaskHandle_t ) pxCommandContext;

    configASSERT( pxReturnInfo != NULL );

    uint32_t ulNotifyValue = pxReturnInfo->returnCode;

    if( xTaskHandle != NULL )
    {
        /* Send the context's ulNotificationValue as the notification value so
         * the receiving task can check the value it set in the context matches
         * the value it receives in the notification. */
        ( void ) xTaskNotifyIndexed( xTaskHandle,
                                     MQTT_NOTIFY_IDX,
                                     ulNotifyValue,
                                     eSetValueWithOverwrite );
    }
}

/*-----------------------------------------------------------*/

static BaseType_t prvPublishAndWaitForAck( MQTTAgentHandle_t xAgentHandle,
                                           const char * pcTopic,
                                           const void * pvPublishData,
                                           size_t xPublishDataLen )
{
    MQTTStatus_t xStatus;
    size_t uxTopicLen = 0;

    configASSERT( pcTopic != NULL );
    configASSERT( pvPublishData != NULL );
    configASSERT( xPublishDataLen > 0 );

    uxTopicLen = strnlen( pcTopic, UINT16_MAX );

    MQTTPublishInfo_t xPublishInfo =
    {
        .qos             = MQTT_PUBLISH_QOS,
        .retain          = 0,
        .dup             = 0,
        .pTopicName      = pcTopic,
        .topicNameLength = ( uint16_t ) uxTopicLen,
        .pPayload        = pvPublishData,
        .payloadLength   = xPublishDataLen
    };

    MQTTAgentCommandInfo_t xCommandParams =
    {
        .blockTimeMs                 = MQTT_PUBLISH_BLOCK_TIME_MS,
        .cmdCompleteCallback         = prvPublishCommandCallback,
        .pCmdCompleteCallbackContext = ( void * ) xTaskGetCurrentTaskHandle(),
    };

    if( xPublishInfo.qos > MQTTQoS0 )
    {
        xCommandParams.pCmdCompleteCallbackContext = ( void * ) xTaskGetCurrentTaskHandle();
    }

    /* Clear the notification index */
    xTaskNotifyStateClearIndexed( NULL, MQTT_NOTIFY_IDX );


    xStatus = MQTTAgent_Publish( xAgentHandle,
                                 &xPublishInfo,
                                 &xCommandParams );

    if( xStatus == MQTTSuccess )
    {
        uint32_t ulNotifyValue = 0;
        BaseType_t xResult = pdFALSE;

        xResult = xTaskNotifyWaitIndexed( MQTT_NOTIFY_IDX,
                                          0xFFFFFFFF,
                                          0xFFFFFFFF,
                                          &ulNotifyValue,
                                          pdMS_TO_TICKS( MQTT_PUBLISH_NOTIFICATION_WAIT_MS ) );

        if( xResult )
        {
            xStatus = ( MQTTStatus_t ) ulNotifyValue;

            if( xStatus != MQTTSuccess )
            {
                LogError( "MQTT Agent returned error code: %d during publish operation.",
                          xStatus );
                xResult = pdFALSE;
            }
        }
        else
        {
            LogError( "Timed out while waiting for publish ACK or Sent event. xTimeout = %d",
                      pdMS_TO_TICKS( MQTT_PUBLISH_NOTIFICATION_WAIT_MS ) );
            xResult = pdFALSE;
        }
    }
    else
    {
        LogError( "MQTTAgent_Publish returned error code: %d.", xStatus );
    }

    return( xStatus == MQTTSuccess );
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
  uint8_t ISM330DHCX_Id;
  uint8_t Status;
  ISM330DHCX_IO_t ISM330DHCX_io_ctx = { 0 };

  uint8_t IIS2MDC_Id;
  IIS2MDC_IO_t IIS2MDC_io_ctx = { 0 };

  /* Configure the accelerometer driver */
  ISM330DHCX_io_ctx.BusType = ISM330DHCX_I2C_BUS; /* I2C */
  ISM330DHCX_io_ctx.Address = ISM330DHCX_I2C_ADD_H;
  ISM330DHCX_io_ctx.Init = BSP_I2C2_Init_OS;
  ISM330DHCX_io_ctx.DeInit = BSP_I2C2_DeInit_OS;
  ISM330DHCX_io_ctx.ReadReg = BSP_I2C2_ReadReg_OS;
  ISM330DHCX_io_ctx.WriteReg = BSP_I2C2_WriteReg_OS;

  ISM330DHCX_RegisterBusIO(&ISM330DHCX_Obj, &ISM330DHCX_io_ctx);
  ISM330DHCX_Init(&ISM330DHCX_Obj);
  ISM330DHCX_ReadID(&ISM330DHCX_Obj, &ISM330DHCX_Id);

  if (ISM330DHCX_Id != ISM330DHCX_ID)
  {
    return ISM330DHCX_ERROR;
  }

  ISM330DHCX_ACC_Enable (&ISM330DHCX_Obj);
  ISM330DHCX_GYRO_Enable(&ISM330DHCX_Obj);

  do
  {
    vTaskDelay(5);
    ISM330DHCX_ACC_Get_DRDY_Status(&ISM330DHCX_Obj, &Status);
  }
  while (Status != 1);

  do
  {
    vTaskDelay(5);
    ISM330DHCX_GYRO_Get_DRDY_Status(&ISM330DHCX_Obj, &Status);
  }
  while (Status != 1);


  /* Configure the MAG driver */
  IIS2MDC_io_ctx.BusType = IIS2MDC_I2C_BUS; /* I2C */
  IIS2MDC_io_ctx.Address = IIS2MDC_I2C_ADD;
  IIS2MDC_io_ctx.Init = BSP_I2C2_Init_OS;
  IIS2MDC_io_ctx.DeInit = BSP_I2C2_DeInit_OS;
  IIS2MDC_io_ctx.ReadReg = BSP_I2C2_ReadReg_OS;
  IIS2MDC_io_ctx.WriteReg = BSP_I2C2_WriteReg_OS;

  IIS2MDC_RegisterBusIO(&IIS2MDC_Obj, &IIS2MDC_io_ctx);
  IIS2MDC_Init(&IIS2MDC_Obj);
  IIS2MDC_ReadID(&IIS2MDC_Obj, &IIS2MDC_Id);

  if (IIS2MDC_Id != IIS2MDC_ID)
  {
    return ISM330DHCX_ERROR;
  }

  IIS2MDC_MAG_Enable (&IIS2MDC_Obj);

  do
  {
    vTaskDelay(5);
    IIS2MDC_MAG_Get_DRDY_Status(&IIS2MDC_Obj, &Status);
  }
  while (Status != 1);
#endif

  return pdTRUE;
}

/*-----------------------------------------------------------*/

static BaseType_t xUpdateSensorData(MotionSensorData_t *pxData)
{
  int32_t lBspError = BSP_ERROR_NONE;

#if USE_SENSORS
  lBspError  = ISM330DHCX_ACC_GetAxes (&ISM330DHCX_Obj, &pxData->xAcceleroAxes);
  lBspError += ISM330DHCX_GYRO_GetAxes(&ISM330DHCX_Obj, &pxData->xGyroAxes    );
  lBspError += IIS2MDC_MAG_GetAxes    (&IIS2MDC_Obj   , &pxData->xMagnetoAxes );
#else
  pxData->xAcceleroAxes.x += uxRand();
  pxData->xAcceleroAxes.y += uxRand();
  pxData->xAcceleroAxes.z += uxRand();

  pxData->xGyroAxes.x += uxRand();
  pxData->xGyroAxes.y += uxRand();
  pxData->xGyroAxes.z += uxRand();

  pxData->xMagnetoAxes.x += uxRand();
  pxData->xMagnetoAxes.y += uxRand();
  pxData->xMagnetoAxes.z += uxRand();

  pxData->xAcceleroAxes.x %= 1000;
  pxData->xAcceleroAxes.y %= 1000;
  pxData->xAcceleroAxes.z %= 1000;

  pxData->xGyroAxes.x %= 1000;
  pxData->xGyroAxes.y %= 1000;
  pxData->xGyroAxes.z %= 1000;

  pxData->xMagnetoAxes.x %= 1000;
  pxData->xMagnetoAxes.y %= 1000;
  pxData->xMagnetoAxes.z %= 1000;
#endif

  return lBspError == BSP_ERROR_NONE;
}

/*-----------------------------------------------------------*/
void vMotionSensorsPublish( void * pvParameters )
{
  BaseType_t xResult   = pdFALSE;
  BaseType_t xExitFlag = pdFALSE;

  MQTTAgentHandle_t xAgentHandle = NULL;
  char * pcPayloadBuf            = NULL;
  char * pcTopicString           = NULL;
  char * pcDeviceId              = NULL;

  size_t uxTopicLen   = 0;
  MotionSensorData_t xSensorData = { 0 };

  pcPayloadBuf  = pvPortMalloc(MQTT_PUBLISH_MAX_LEN      );
  pcTopicString = pvPortMalloc(MQTT_PUBLICH_TOPIC_STR_LEN);

    ( void ) pvParameters;

    xResult = xInitSensors();

    if( xResult != pdTRUE )
    {
        LogError( "Error while initializing sensors." );
        vTaskDelete( NULL );
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

    if( ( uxTopicLen <= 0 ) || ( uxTopicLen > MQTT_PUBLICH_TOPIC_STR_LEN ) )
    {
        LogError( "Error while constructing topic string." );
        xExitFlag = pdTRUE;
    }

    vSleepUntilMQTTAgentReady();

    xAgentHandle = xGetMqttAgentHandle();

    while (xExitFlag == pdFALSE)
    {
      xResult = xUpdateSensorData(&xSensorData);

      if (xResult != pdTRUE)
      {
        LogError("Error while reading sensor data.");
      }
      else if (xIsMqttConnected() == pdTRUE)
      {
        int lbytesWritten = 0;

        lbytesWritten = snprintf( pcPayloadBuf,
                                  MQTT_PUBLISH_MAX_LEN,
                                   "{"
                                   "\"acceleration_mG\":"
                                   "{"
                                   "\"x\": %ld,"
                                   "\"y\": %ld,"
                                   "\"z\": %ld"
                                   "},"
                                   "\"gyro_mDPS\":"
                                   "{"
                                   "\"x\": %ld,"
                                   "\"y\": %ld,"
                                   "\"z\": %ld"
                                   "},"
                                   "\"magnetometer_mGauss\":"
                                   "{"
                                   "\"x\": %ld,"
                                   "\"y\": %ld,"
                                   "\"z\": %ld"
                                   "}"
                                   "}",
                                   xSensorData.xAcceleroAxes.x, xSensorData.xAcceleroAxes.y, xSensorData.xAcceleroAxes.z,
                                   xSensorData.xGyroAxes.x, xSensorData.xGyroAxes.y, xSensorData.xGyroAxes.z,
                                   xSensorData.xMagnetoAxes.x, xSensorData.xMagnetoAxes.y, xSensorData.xMagnetoAxes.z );

            if( ( lbytesWritten < MQTT_PUBLISH_MAX_LEN ) && ( xIsMqttAgentConnected() == pdTRUE ) )
            {
                xResult = prvPublishAndWaitForAck( xAgentHandle,
                                                   pcTopicString,
                                                   pcPayloadBuf,
                                                   ( size_t ) lbytesWritten );

                if( xResult != pdPASS )
                {
                    LogError( "Failed to publish motion sensor data" );
                }
            }
        }

        /* Wait until its time to poll the sensors again */
        vTaskDelay(MQTT_PUBLISH_TIME_BETWEEN_MS);
    }
}

/**
 ******************************************************************************
 * @file    main_app.c
 * @author  GPM Application Team
 * @brief   main_app program body
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

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Application */
#include "main.h"
#include "app_config.h"

#include "w6x_api.h"
#include "w61_at_api.h"
#include "common_parser.h" /* Common Parser functions */
#include "spi.h" /* spi falling/rising_callback */
#include "logging.h"

#include "util_mem_perf.h"
#include "util_task_perf.h"

#include "app_freertos.h"
#include "queue.h"
#include "event_groups.h"

#include "kvstore.h"

#include "sys_evt.h"

#include "interrupt_handlers.h"
#if MQTT_ENABLED
#include "core_mqtt_serializer.h"
#endif
/* Global variables ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private defines -----------------------------------------------------------*/
#define EVENT_FLAG_SCAN_DONE   (1<<1)             /*!< Scan done event bitmask */

#define WIFI_SCAN_TIMEOUT      10000              /*!< Delay before to declare the scan in failure */

/* Private macros ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Event bitmask flag used for asynchronous execution */
static EventGroupHandle_t scan_event_flags = NULL; /*!< Wi-Fi scan event flags */
#if MQTT_ENABLED
/* MQTT structure to receive subscribed message from the ST67W6X Driver */
extern W6X_MQTT_Data_t *pxMQTTRecvData;

extern QueueHandle_t xSubMsgQueue;
#endif
/* Private function prototypes -----------------------------------------------*/
/**
 * @brief Wi-Fi event callback
 * @param event_id: Event ID
 * @param event_args: Event arguments
 */
static void APP_wifi_cb(W6X_event_id_t event_id, void *event_args);

/**
 * @brief Network event callback
 * @param event_id: Event ID
 * @param event_args: Event arguments
 */
static void APP_net_cb(W6X_event_id_t event_id, void *event_args);

/**
 * @brief MQTT event callback
 * @param event_id: Event ID
 * @param event_args: Event arguments
 */
static void APP_mqtt_cb(W6X_event_id_t event_id, void *event_args);

/**
 * @brief BLE event callback
 * @param event_id: Event ID
 * @param event_args: Event arguments
 */
static void APP_ble_cb(W6X_event_id_t event_id, void *event_args);

/**
 * @brief Wi-Fi scan callback
 * @param status: Scan status
 * @param Scan_results: Scan results
 */
static void APP_wifi_scan_cb(int32_t status, W6X_Scan_Result_t *Scan_results);

/* Functions Definition ------------------------------------------------------*/
static int32_t APP_Init(void)
{
  int32_t ret = 0;

  /* Register the application callback to received events from ST67W6X Driver */
  W6X_RegisterAppCb(APP_wifi_cb, APP_net_cb, APP_mqtt_cb, APP_ble_cb);

  GPIO_EXTI_Register_Rising_Callback(SPI_SLAVE_DATA_RDY_Pin, spi_on_txn_data_ready, NULL);
  GPIO_EXTI_Register_Falling_Callback(SPI_SLAVE_DATA_RDY_Pin, spi_on_header_ack, NULL);

  /* Initialize the ST67W6X Driver */
  ret = W6X_Init();

  if (ret != W6X_STATUS_OK)
  {
    LogError("failed to initialize ST67W6X Driver, %ld", ret);
  }

  ( void ) xEventGroupClearBits( xSystemEvents, EVT_MASK_NET_CONNECTED );

  return ret;
}

static int32_t APP_WiFi_Init(void)
{
  int32_t ret = W6X_STATUS_OK;

  if (ret == W6X_STATUS_OK)
  {
    /* Initialize the ST67W6X Wi-Fi module */
    ret = W6X_WiFi_Init();

    if (ret != W6X_STATUS_OK)
    {
      LogError("failed to initialize ST67W6X Wi-Fi component, %ld", ret);
    }
  }

  if (ret == W6X_STATUS_OK)
  {
    LogInfo("Wi-Fi init is done");

    /* Set DTIM value (dtim * 100ms). 0: Disabled, 1: 100ms, 10: 1s */
    ret = W6X_WiFi_SetDTIM(0);

    if (ret != W6X_STATUS_OK)
    {
      LogError("failed to initialize the DTIM, %d", ret);
    }
  }

  if (ret == W6X_STATUS_OK)
  {
    /* Initialize the ST67W6X Network module */
    ret = W6X_Net_Init();

    if (ret != W6X_STATUS_OK)
    {
      LogError("failed to initialize ST67W6X Net component, %d", ret);
    }
  }

  if (ret == W6X_STATUS_OK)
  {
    LogInfo("Net init is done");

    (void) xEventGroupSetBits(xSystemEvents, EVT_MASK_NET_INIT);
  }

  return ret;
}

static int32_t APP_WiFi_Scan(void)
{
  int32_t ret = W6X_STATUS_OK;

  /* Wi-Fi variables */
  W6X_Scan_Opts_t Opts = { 0 };

  /* Run a Wi-Fi scan to retrieve the list of all nearby Access Points */
  scan_event_flags = xEventGroupCreate();
  W6X_WiFi_Scan(&Opts, &APP_wifi_scan_cb);

  /* Wait to receive the EVENT_FLAG_SCAN_DONE event. The scan is declared as failed after 'ScanTimeout' delay */
  if ((int32_t) xEventGroupWaitBits(scan_event_flags, EVENT_FLAG_SCAN_DONE, pdTRUE, pdFALSE, pdMS_TO_TICKS(WIFI_SCAN_TIMEOUT)) != EVENT_FLAG_SCAN_DONE)
  {
    ret = W6X_STATUS_ERROR;
  }

  if (ret != W6X_STATUS_OK)
  {
    LogError("Scan Failed");
  }

  return ret;
}

static int32_t APP_WiFi_Connect(void)
{
  int32_t ret = W6X_STATUS_OK;
  W6X_Connect_Opts_t ConnectOpts = { 0 };

  /* Connect the device to the pre-defined Access Point */
  LogInfo("Connecting to Local Access Point");

#if defined(LFS_CONFIG)
  KVStore_getString(CS_WIFI_SSID, (char*) ConnectOpts.SSID, W6X_WIFI_MAX_SSID_SIZE);
  KVStore_getString(CS_WIFI_CREDENTIAL, (char*) ConnectOpts.Password, W6X_WIFI_MAX_PASSWORD_SIZE);
#else
  snprintf((char *)ConnectOpts.SSID    , W6X_WIFI_MAX_SSID_SIZE    , "%s", DEFAULT_SSID);
  snprintf((char *)ConnectOpts.Password, W6X_WIFI_MAX_PASSWORD_SIZE, "%s", DEFAULT_PSWD);
#endif

  ret = W6X_WiFi_Connect(&ConnectOpts);

  if (ret != W6X_STATUS_OK)
  {
    LogError("failed to connect, %d", ret);
  }

  return ret;
}

static void APP_WiFi_GetIpAddress(void)
{
  char ip_str[17] = { '\0' };
  uint8_t ip_addr     [4] = { 0 };
  uint8_t gateway_addr[4] = { 0 };
  uint8_t netmask_addr[4] = { 0 };

  /* Get the IP Address of the Access Point */
  if (W6X_STATUS_OK == W6X_WiFi_GetStaIpAddress(ip_addr, gateway_addr, netmask_addr))
  {
    /* Convert the IP Address array into string */
    snprintf(ip_str, 16, IPSTR, IP2STR(ip_addr));
    LogInfo("IP Address: %s", ip_str);
  }
}

void W6X_WiFi_Task(void *pvParameters)
{
  int32_t ret = 0;

  LogInfo("%s started\n", __func__);

  ret = APP_Init();

  if (ret == W6X_STATUS_OK)
  {
    ret = APP_WiFi_Init();
  }

  EventBits_t xEvent = 0;

  while (1)
  {
    if(xEvent != EVT_MASK_NET_CONNECTED)
    {
      ret = W6X_STATUS_OK;

      if (ret == W6X_STATUS_OK)
      {
        ret = APP_WiFi_Scan();
      }

      if (ret == W6X_STATUS_OK)
      {
        ret = APP_WiFi_Connect();
      }

      xEvent = xEventGroupWaitBits(xSystemEvents, EVT_MASK_NET_CONNECTED, pdFALSE, pdTRUE, pdMS_TO_TICKS(5000));
      xEvent &= EVT_MASK_NET_CONNECTED;
    }
    else
    {
      xEvent = xEventGroupWaitBits(xSystemEvents, EVT_MASK_NET_CONNECTED, pdFALSE, pdTRUE, pdMS_TO_TICKS(100));
      xEvent &= EVT_MASK_NET_CONNECTED;
      vTaskDelay(100);
    }
  }
}

/* Private Functions Definition ----------------------------------------------*/
static void APP_wifi_scan_cb(int32_t status, W6X_Scan_Result_t *Scan_results)
{
  xEventGroupSetBits(scan_event_flags, EVENT_FLAG_SCAN_DONE);
  LogInfo("SCAN DONE");
  LogDebug("Cb informed APP that WIFI SCAN DONE.");
  W6X_WiFi_PrintScan(Scan_results);
}

static void APP_wifi_cb(W6X_event_id_t event_id, void *event_args)
{
  W6X_Connect_t connectData = { 0 };
  W6X_StaStateType_e state = W6X_WIFI_STATE_STA_OFF;

  switch (event_id)
  {
  case W6X_WIFI_EVT_CONNECTED_ID:
    if (W6X_WiFi_GetStaState(&state, &connectData) != W6X_STATUS_OK)
    {
      LogError("Could not connected to an Access Point");

      break;
    }

    LogInfo("Connected to the following Access Point : [" MACSTR "] Channel: %d | RSSI: %d | SSID: %s", MAC2STR(connectData.MAC), connectData.Channel, connectData.Rssi, connectData.SSID);
    break;

  case W6X_WIFI_EVT_DISCONNECTED_ID:
    LogInfo("Station disconnected from Access Point");
    (void) xEventGroupClearBits(xSystemEvents, EVT_MASK_NET_CONNECTED);
    break;

  case W6X_WIFI_EVT_GOT_IP_ID:
    APP_WiFi_GetIpAddress();
    xEventGroupSetBits(xSystemEvents, EVT_MASK_NET_CONNECTED);
    break;

  case W6X_WIFI_EVT_CONNECTING_ID:
    break;

  case W6X_WIFI_EVT_REASON_ID:
    LogInfo("Reason: %s", W6X_WiFi_ReasonToStr(event_args));

  default:
    break;
  }
}

static void APP_net_cb(W6X_event_id_t event_id, void *event_args)
{
  W6X_CbParamNetData_t *p_param_app_net_cb;

  switch (event_id)
  {
  case W6X_NET_EVT_SOCK_DATA_ID:
    p_param_app_net_cb = (W6X_CbParamNetData_t*) event_args;
    LogDebug("Cb informed app that Wi-Fi %d bytes available on socket %d.", p_param_app_net_cb->available_data_length, p_param_app_net_cb->socket_id);
    xEventGroupSetBits(xSystemEvents, EVT_MASK_NET_READABLE);
    break;

  default:
    break;
  }
}

static void APP_mqtt_cb(W6X_event_id_t event_id, void *event_args)
{
#if MQTT_ENABLED
  W6X_CbParamMqttData_t *p_param_mqtt_data = (W6X_CbParamMqttData_t *) event_args;
  MQTTPublishInfo_t mqtt_data;

  switch (event_id)
  {
    case W6X_MQTT_EVT_CONNECTED_ID:
      LogInfo("MQTT Connected");
      break;

    case W6X_MQTT_EVT_DISCONNECTED_ID:
      LogInfo("MQTT Disconnected");
      break;

    case W6X_MQTT_EVT_SUBSCRIPTION_RECEIVED_ID:
      mqtt_data.qos    = 0;
      mqtt_data.retain = false;
      mqtt_data.dup    = false;

      /* Get the received topic length */
      mqtt_data.topicNameLength = p_param_mqtt_data->topic_length - 2;

      /* Allocate a memory buffer to store the topic string in the sub_msg_queue */
      mqtt_data.pTopicName = pvPortMalloc(mqtt_data.topicNameLength);

      /* Copy the received topic in allocated buffer */
      memcpy((char *)mqtt_data.pTopicName, pxMQTTRecvData->p_recv_data +1,  mqtt_data.topicNameLength);

      /* Get the received message length */
      mqtt_data.payloadLength = p_param_mqtt_data->message_length;

      /* Allocate a memory buffer to store the message string in the sub_msg_queue */
      mqtt_data.pPayload = pvPortMalloc(mqtt_data.payloadLength);

      /* Copy the received message in allocated buffer */
      memcpy(mqtt_data.pPayload, pxMQTTRecvData->p_recv_data + mqtt_data.topicNameLength + 3, mqtt_data.payloadLength);

      /* Push the new mqtt_data into the xSubMsgQueue */
      xQueueSendToBack(xSubMsgQueue, &mqtt_data, 0);
      break;

    default:
      break;
  }
#endif
}

static void APP_ble_cb(W6X_event_id_t event_id, void *event_args)
{

}



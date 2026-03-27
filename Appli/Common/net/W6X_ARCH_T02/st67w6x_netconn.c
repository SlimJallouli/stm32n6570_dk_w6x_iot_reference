/* USER CODE BEGIN Header */
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
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Application */
#include "main.h"
#include "lwip.h"

#include "w6x_api.h"
#include "common_parser.h" /* Common Parser functions */
#include "spi_iface.h" /* SPI falling/rising_callback */

#include "logging_levels.h"
#if defined(LOG_LEVEL)
#undef LOG_LEVEL
#endif

#define LOG_LEVEL    LOG_DEBUG

#include "logging.h"

#ifndef REDEFINE_FREERTOS_INTERFACE
/* Depending on the version of FreeRTOS the inclusion might need to be redefined in app_config.h */
#include "app_freertos.h"
#include "queue.h"
#include "event_groups.h"
#endif /* REDEFINE_FREERTOS_INTERFACE */

#include "kvstore.h"
#include "interrupt_handlers.h"
#include "sys_evt.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Global variables ----------------------------------------------------------*/
/* USER CODE BEGIN GV */

/* USER CODE END GV */

/* Private typedef -----------------------------------------------------------*/


/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private defines -----------------------------------------------------------*/
/* Application events */
#define EVT_APP_WIFI_CONNECTED      (1 << 0)
#define EVT_APP_WIFI_DISCONNECTED   (1 << 1)
#define EVT_APP_WIFI_GOT_IP         (1 << 2)
#define EVT_APP_RECONNECT_REQUEST   (1 << 3)
#define EVT_APP_ALL_BIT             (EVT_APP_WIFI_CONNECTED | \
                                     EVT_APP_WIFI_DISCONNECTED | \
                                     EVT_APP_WIFI_GOT_IP | \
                                     EVT_APP_RECONNECT_REQUEST)

/* Network connection timeouts (milliseconds) */
#define NET_IP_WAIT_TIMEOUT         pdMS_TO_TICKS(30 * 1000)   /* 30 seconds */
#define NET_RETRY_BACKOFF           pdMS_TO_TICKS(5 * 1000)    /* 5 seconds */

/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macros ------------------------------------------------------------*/

/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Network state machine enumeration */
typedef enum {
    W6X_NET_STATE_NONE = 0,        /**< No connection, initial state */
    W6X_NET_STATE_STA_DOWN,        /**< Station offline */
    W6X_NET_STATE_STA_UP,          /**< Station connected to AP */
    W6X_NET_STATE_STA_GOT_IP       /**< Station obtained IP address */
} W6xNetStatus_t;

/* Private variables ---------------------------------------------------------*/
/** BLE data buffer to receive message from the ST67W6X Driver */
static uint8_t a_APP_AvailableData[247] = {0};

/** Application event group */
extern EventGroupHandle_t xSystemEvents;
EventGroupHandle_t app_evt_current = NULL;

/** Network task handle - used for external reconnect requests */
static TaskHandle_t xNetTaskHandle = NULL;

/** Current and previous network state for state machine tracking */
static W6xNetStatus_t current_status = W6X_NET_STATE_NONE;
static W6xNetStatus_t previous_status = W6X_NET_STATE_NONE;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/**
  * @brief  Wi-Fi event callback
  * @param  event_id: Event ID
  * @param  event_args: Event arguments
  */
static void APP_wifi_cb(W6X_event_id_t event_id, void *event_args);

/**
  * @brief  Network event callback
  * @param  event_id: Event ID
  * @param  event_args: Event arguments
  */
static void APP_net_cb(W6X_event_id_t event_id, void *event_args);

/**
  * @brief  MQTT event callback
  * @param  event_id: Event ID
  * @param  event_args: Event arguments
  */
static void APP_mqtt_cb(W6X_event_id_t event_id, void *event_args);

/**
  * @brief  BLE event callback
  * @param  event_id: Event ID
  * @param  event_args: Event arguments
  */
static void APP_ble_cb(W6X_event_id_t event_id, void *event_args);

/**
  * @brief  W6X error callback
  * @param  ret_w6x: W6X status
  * @param  func_name: function name
  */
static void APP_error_cb(W6X_Status_t ret_w6x, char const *func_name);

/**
  * @brief  Set event group to release the waiting task
  * @param  app_event: Event group
  * @param  evt: Event to set
  */
void APP_setevent(EventGroupHandle_t *app_event, uint32_t evt);

#if (SHELL_ENABLE == 1)
/**
  * @brief  Shell command to display the application information
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval ::SHELL_STATUS_OK on success
  */
int32_t APP_shell_info(int32_t argc, char **argv);

/**
  * @brief  Shell command to quit the application
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval ::SHELL_STATUS_OK on success
  */
int32_t APP_shell_quit(int32_t argc, char **argv);
#endif /* SHELL_ENABLE */

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Helper Functions ----------------------------------------------------------*/

/**
  * @brief  Convert network state to string for logging
  * @param  status: Network state
  * @return String representation of the state
  */
static const char* net_status_to_string(W6xNetStatus_t status)
{
  switch(status)
  {
    case W6X_NET_STATE_NONE:
      return "None";
    case W6X_NET_STATE_STA_DOWN:
      return "Station Down";
    case W6X_NET_STATE_STA_UP:
      return "Station Up (no IP)";
    case W6X_NET_STATE_STA_GOT_IP:
      return "Station Got IP";
    default:
      return "Unknown";
  }
}

/**
  * @brief  Update network state and log transition
  * @param  new_status: New network state
  */
static void net_update_status(W6xNetStatus_t new_status)
{
  previous_status = current_status;
  current_status = new_status;

  if(previous_status != current_status)
  {
    LogDebug("Network status: %s -> %s\n",
            net_status_to_string(previous_status),
            net_status_to_string(current_status));
  }
}

/**
  * @brief  Request network reconnection from external task
  * @return pdTRUE on success, pdFALSE if net_main is not running
  * @note   This API allows other tasks (e.g., MQTT) to trigger Wi-Fi reconnection
  *         Example: net_request_reconnect() when MQTT connection fails
  */
BaseType_t net_request_reconnect(void)
{
  BaseType_t xReturn = pdFALSE;

  LogDebug("External reconnection request\n");

  if(xNetTaskHandle != NULL)
  {
    xReturn = xEventGroupSetBits(app_evt_current, EVT_APP_RECONNECT_REQUEST);
  }

  return xReturn;
}
void net_main(void *argument)
{
  W6X_Status_t ret = 0;
  EventBits_t eventBits = 0;
  W6X_WiFi_Connect_t connectData = {0};
  W6X_WiFi_StaStateType_e state = W6X_WIFI_STATE_STA_OFF;
  W6X_WiFi_Connect_Opts_t connect_opts = {0};
  uint8_t mac_addr[6] = {0};
  uint32_t connect_attempts = 0;
  const uint32_t MAX_CONNECT_ATTEMPTS = 100;

  argument = argument;

  /* Store this task's handle for external reconnection requests */
  xNetTaskHandle = xTaskGetCurrentTaskHandle();
  LogDebug("Network task started (handle=%p)\n", xNetTaskHandle);

  app_evt_current = xEventGroupCreate();

  /* Register the application callback to received events from ST67W6X Driver */
  W6X_App_Cb_t App_cb = {0};

  App_cb.APP_wifi_cb  = APP_wifi_cb;
  App_cb.APP_net_cb   = APP_net_cb;
  App_cb.APP_ble_cb   = APP_ble_cb;
  App_cb.APP_mqtt_cb  = APP_mqtt_cb;
  App_cb.APP_error_cb = APP_error_cb;

  W6X_RegisterAppCb(&App_cb);

  GPIO_EXTI_Register_Rising_Callback(SPI_RDY_Pin, spi_on_txn_data_ready, NULL);
  GPIO_EXTI_Register_Falling_Callback(SPI_RDY_Pin, spi_on_header_ack, NULL);

#if defined(LFS_CONFIG)
  KVStore_getString(CS_WIFI_SSID, (char*) connect_opts.SSID, W6X_WIFI_MAX_SSID_SIZE);
  KVStore_getString(CS_WIFI_CREDENTIAL, (char*) connect_opts.Password, W6X_WIFI_MAX_PASSWORD_SIZE);
#else
  snprintf((char *)connect_opts.SSID    , W6X_WIFI_MAX_SSID_SIZE    , "%s", DEFAULT_SSID);
  snprintf((char *)connect_opts.Password, W6X_WIFI_MAX_PASSWORD_SIZE, "%s", DEFAULT_PSWD);
#endif

  /* Initialize the ST67W6X Driver */
  ret = W6X_Init();

  if (W6X_STATUS_OK != ret)
  {
    LogError("Failed to initialize ST67W6X Driver, %" PRIi32 "\n", ret);
  }

  if(W6X_STATUS_OK == ret)
  {
  /* Initialize the ST67W6X Wi-Fi module */
    ret = W6X_WiFi_Init();

    if (W6X_STATUS_OK != ret)
    {
      LogError("Failed to initialize ST67W6X Wi-Fi component, %" PRIi32 "\n", ret);
    }
  }

  if(W6X_STATUS_OK == ret)
  {
    LogInfo("Wi-Fi init is done\n");

    /* Initialize the LWIP stack */
    ret = MX_LWIP_Init();

    if (W6X_STATUS_OK != ret)
    {
      LogError("Failed to initialize LWIP stack %" PRIi32 "\n", ret);
    }
    else
    {
      LogInfo("W6X is ready\n");
    }
  }

  /* Query and log MAC address */
  if(W6X_STATUS_OK == ret)
  {
    if(W6X_WiFi_Station_GetMACAddress(mac_addr) == W6X_STATUS_OK)
    {
      LogInfo("Wi-Fi MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);
    }
    else
    {
      LogWarn("Failed to query MAC address\n");
    }
  }

  if(W6X_STATUS_OK == ret)
  {
//    ret = W6X_WiFi_Connect(&connect_opts);
    APP_setevent(&app_evt_current, EVT_APP_WIFI_DISCONNECTED);
    connect_attempts = 1;
  }

  /* Initialize state machine */
  net_update_status(W6X_NET_STATE_STA_DOWN);

  while(W6X_STATUS_OK == ret)
  {
    /* Wait for events with timeout instead of indefinite wait */
    eventBits = xEventGroupWaitBits(app_evt_current,
                                    EVT_APP_ALL_BIT,
                                    pdTRUE,           /* Auto-clear events */
                                    pdFALSE,          /* OR logic (any bit) */
                                    NET_IP_WAIT_TIMEOUT);

    if (eventBits & EVT_APP_WIFI_CONNECTED)
    {
      net_update_status(W6X_NET_STATE_STA_UP);

      uint32_t ps_mode = 0;
      if ((W6X_GetPowerMode(&ps_mode) != W6X_STATUS_OK) || (W6X_SetPowerMode(ps_mode) != W6X_STATUS_OK))
      {
        LogError("Failed to restore the power save state\n");
        continue;
      }

      if (W6X_WiFi_Station_GetState(&state, &connectData) != W6X_STATUS_OK)
      {
        LogWarn("Connected to an unknown Access Point\n");
        continue;
      }

      LogInfo("Connected to following Access Point:\n");
      LogInfo("[" MACSTR "] Channel: %" PRIu32 " | RSSI: %" PRIi32 " | SSID: %s\n",
              MAC2STR(connectData.MAC),
              connectData.Channel,
              connectData.Rssi,
              connectData.SSID);

      connect_attempts = 0;  /* Reset retry counter on successful connection */
    }

    if (eventBits & EVT_APP_WIFI_DISCONNECTED)
    {
      net_update_status(W6X_NET_STATE_STA_DOWN);
      LogInfo("Station disconnected from Access Point\n");
      xEventGroupSetBits(xSystemEvents, EVT_MASK_NET_DISCONNECTED);

      /* If disconnected unexpectedly, schedule automatic retry */
//      if(connect_attempts > 0 && connect_attempts < MAX_CONNECT_ATTEMPTS)
      {
//        LogInfo("Automatic reconnection attempt %u/%u\n", connect_attempts + 1, MAX_CONNECT_ATTEMPTS);
        LogInfo("Reconnection\n");
//        vTaskDelay(NET_RETRY_BACKOFF);
        W6X_WiFi_Connect(&connect_opts);
        connect_attempts++;
      }
    }

    if (eventBits & EVT_APP_WIFI_GOT_IP)
    {
      net_update_status(W6X_NET_STATE_STA_GOT_IP);
      xEventGroupSetBits(xSystemEvents, EVT_MASK_NET_CONNECTED);
    }

    if (eventBits & EVT_APP_RECONNECT_REQUEST)
    {
      /* External reconnection request (e.g., from MQTT agent) */
      LogInfo("External reconnection requested - attempting reconnect\n");

      if(current_status != W6X_NET_STATE_STA_GOT_IP)
      {
        LogInfo("Initiating Wi-Fi reconnection\n");
        vTaskDelay(NET_RETRY_BACKOFF);
        W6X_WiFi_Connect(&connect_opts);
        connect_attempts = 1;
      }
      else
      {
        LogWarn("Already connected, ignoring reconnect request\n");
      }
    }

    /* Handle event timeout - no events received within the timeout period */
    if(eventBits == 0)
    {
      /* Periodic status check on timeout */
      if(W6X_WiFi_Station_GetState(&state, &connectData) == W6X_STATUS_OK)
      {
        LogDebug("Status check: state=%d\n", state);
      }
    }
  }

  LogInfo("##### Quitting the application\n");

  W6X_Ble_DeInit();  /* De-initialize the ST67W6X BLE module   */
  W6X_WiFi_DeInit(); /* De-initialize the ST67W6X Wi-Fi module */
  W6X_DeInit();      /* De-initialize the ST67W6X Driver       */

  LogInfo("##### Application end\n");

  xNetTaskHandle = NULL;  /* Clear handle when task exits */
  vTaskDelete(NULL);
}
#if 0
void HAL_GPIO_EXTI_Callback(uint16_t pin);
void HAL_GPIO_EXTI_Rising_Callback(uint16_t pin);
void HAL_GPIO_EXTI_Falling_Callback(uint16_t pin);

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
  /* USER CODE BEGIN HAL_GPIO_EXTI_Callback_1 */

  /* USER CODE END HAL_GPIO_EXTI_Callback_1 */
  /* Callback when data is available in Network CoProcessor to enable SPI Clock */
  if (pin == SPI_RDY_Pin)
  {
    if (HAL_GPIO_ReadPin(SPI_RDY_GPIO_Port, SPI_RDY_Pin) == GPIO_PIN_SET)
    {
      HAL_GPIO_EXTI_Rising_Callback(pin);
    }
    else
    {
      HAL_GPIO_EXTI_Falling_Callback(pin);
    }
  }
  /* USER CODE BEGIN HAL_GPIO_EXTI_Callback_End */

  /* USER CODE END HAL_GPIO_EXTI_Callback_End */
}

void HAL_GPIO_EXTI_Rising_Callback(uint16_t pin)
{
  /* USER CODE BEGIN EXTI_Rising_Callback_1 */

  /* USER CODE END EXTI_Rising_Callback_1 */
  /* Callback when data is available in Network CoProcessor to enable SPI Clock */
  if (pin == SPI_RDY_Pin)
  {
    spi_on_txn_data_ready();
  }
  /* USER CODE BEGIN EXTI_Rising_Callback_End */

  /* USER CODE END EXTI_Rising_Callback_End */
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t pin)
{
  /* USER CODE BEGIN EXTI_Falling_Callback_1 */

  /* USER CODE END EXTI_Falling_Callback_1 */
  /* Callback when data is available in Network CoProcessor to enable SPI Clock */
  if (pin == SPI_RDY_Pin)
  {
    spi_on_header_ack();
  }

  /* USER CODE BEGIN EXTI_Falling_Callback_End */

  /* USER CODE END EXTI_Falling_Callback_End */
}
#endif
/* USER CODE BEGIN FD */

/* USER CODE END FD */

/* Private Functions Definition ----------------------------------------------*/
static void APP_wifi_cb(W6X_event_id_t event_id, void *event_args)
{
  /* USER CODE BEGIN APP_wifi_cb_1 */

  /* USER CODE END APP_wifi_cb_1 */

  W6X_WiFi_CbParamData_t *cb_data = {0};

  switch (event_id)
  {
    case W6X_WIFI_EVT_CONNECTED_ID:
      LogDebug("WiFi event: Connected\n");
      APP_setevent(&app_evt_current, EVT_APP_WIFI_CONNECTED);
      break;

    case W6X_WIFI_EVT_DISCONNECTED_ID:
      LogDebug("WiFi event: Disconnected\n");
      APP_setevent(&app_evt_current, EVT_APP_WIFI_DISCONNECTED);
      break;

    case W6X_WIFI_EVT_REASON_ID:
      LogInfo("Disconnection reason: %s\n", W6X_WiFi_ReasonToStr(event_args));
      APP_setevent(&app_evt_current, EVT_APP_WIFI_DISCONNECTED);
      break;

    case W6X_WIFI_EVT_DIST_STA_IP_ID:
      break;

    case W6X_WIFI_EVT_STA_CONNECTED_ID:
      cb_data = (W6X_WiFi_CbParamData_t *)event_args;
      LogInfo("Station connected to soft-AP: [" MACSTR "]\n", MAC2STR(cb_data->MAC));
      break;

    case W6X_WIFI_EVT_STA_DISCONNECTED_ID:
      cb_data = (W6X_WiFi_CbParamData_t *)event_args;
      LogInfo("Station disconnected from soft-AP: [" MACSTR "]\n", MAC2STR(cb_data->MAC));
      break;

    default:
      break;
  }
  /* USER CODE BEGIN APP_wifi_cb_End */

  /* USER CODE END APP_wifi_cb_End */
}

static void APP_net_cb(W6X_event_id_t event_id, void *event_args)
{
  /* USER CODE BEGIN APP_net_cb_1 */

  /* USER CODE END APP_net_cb_1 */
}

static void APP_mqtt_cb(W6X_event_id_t event_id, void *event_args)
{
  /* USER CODE BEGIN APP_mqtt_cb_1 */

  /* USER CODE END APP_mqtt_cb_1 */
}

static void APP_ble_cb(W6X_event_id_t event_id, void *event_args)
{
  /* USER CODE BEGIN APP_ble_cb_1 */

  /* USER CODE END APP_ble_cb_1 */
  uint8_t service_index = 0;
  uint8_t charac_index = 0;
  uint32_t charac_handle = 0;
  uint32_t charac_value_handle = 0;

  W6X_Ble_Service_t *service = NULL;
  char tmp_UUID[33];
  uint8_t uuid_size = 0;

  W6X_Ble_CbParamData_t *p_param_ble_data = (W6X_Ble_CbParamData_t *) event_args;

  switch (event_id)
  {
    case W6X_BLE_EVT_CONNECTED_ID:
      LogInfo(" -> BLE CONNECTED: Conn_Handle: %" PRIu16 "\n", p_param_ble_data->remote_ble_device.conn_handle);
      W6X_Ble_SetRecvDataPtr(a_APP_AvailableData, sizeof(a_APP_AvailableData));
      break;

    case W6X_BLE_EVT_CONNECTION_PARAM_ID:
      LogInfo(" -> BLE CONNECTION PARAM UPDATE\n");
      break;

    case W6X_BLE_EVT_DISCONNECTED_ID:
      LogInfo(" -> BLE DISCONNECTED.\n");
      break;

    case W6X_BLE_EVT_INDICATION_STATUS_ENABLED_ID:
      LogInfo(" -> BLE INDICATION ENABLED [Service: %" PRIu16 ", Charac: %" PRIu16 "]\n",
              p_param_ble_data->service_idx, p_param_ble_data->charac_idx);
      break;

    case W6X_BLE_EVT_INDICATION_STATUS_DISABLED_ID:
      LogInfo(" -> BLE INDICATION DISABLED [Service: %" PRIu16 ", Charac: %" PRIu16 "]\n",
              p_param_ble_data->service_idx, p_param_ble_data->charac_idx);
      break;

    case W6X_BLE_EVT_NOTIFICATION_STATUS_ENABLED_ID:
      LogInfo(" -> BLE NOTIFICATION ENABLED [Service: %" PRIu16 ", Charac: %" PRIu16 "]\n",
              p_param_ble_data->service_idx, p_param_ble_data->charac_idx);
      break;

    case W6X_BLE_EVT_NOTIFICATION_STATUS_DISABLED_ID:
      LogInfo(" -> BLE NOTIFICATION DISABLED [Service: %" PRIu16 ", Charac: %" PRIu16 "]\n",
              p_param_ble_data->service_idx, p_param_ble_data->charac_idx);
      break;

    case W6X_BLE_EVT_NOTIFICATION_DATA_ID:
      LogInfo(" -> BLE NOTIFICATION [Connection: %" PRIu16 ", Charac value handle: %" PRIu16 "]\n",
              p_param_ble_data->remote_ble_device.conn_handle, p_param_ble_data->charac_value_handle);
      for (uint32_t i = 0; i < p_param_ble_data->available_data_length; i++)
      {
        LogInfo("0x%02" PRIX16 "\n", a_APP_AvailableData[i]);
      }
      memset(a_APP_AvailableData, 0, sizeof(a_APP_AvailableData));
      break;

    case W6X_BLE_EVT_WRITE_ID:
      LogInfo(" -> BLE WRITE [Connection: %" PRIu16 ", Service: %" PRIu16 ", Charac: %" PRIu16 "]\n",
              p_param_ble_data->remote_ble_device.conn_handle, p_param_ble_data->service_idx,
              p_param_ble_data->charac_idx);
      for (uint32_t i = 0; i < p_param_ble_data->available_data_length; i++)
      {
        LogInfo("0x%02" PRIX16 "\n", a_APP_AvailableData[i]);
      }
      memset(a_APP_AvailableData, 0, sizeof(a_APP_AvailableData));
      break;

    case W6X_BLE_EVT_READ_ID:
      LogInfo(" -> BLE READ [Connection: %" PRIu16 ", Service: %" PRIu16 ", Charac: %" PRIu16 "]\n",
              p_param_ble_data->remote_ble_device.conn_handle, p_param_ble_data->service_idx,
              p_param_ble_data->charac_idx);
      for (uint32_t i = 0; i < p_param_ble_data->available_data_length; i++)
      {
        LogInfo("0x%02" PRIX16 "\n", a_APP_AvailableData[i]);
      }
      memset(a_APP_AvailableData, 0, sizeof(a_APP_AvailableData));
      break;

    case W6X_BLE_EVT_SERVICE_FOUND_ID:
      service_index = p_param_ble_data->Service.service_idx;

      service = &p_param_ble_data->Service;
      memset(tmp_UUID, 0x20, 33);

      uuid_size = service->uuid_type == W6X_BLE_UUID_TYPE_16 ? 4 : 16;
      for (int32_t i = 0; i < uuid_size; i++)
      {
        sprintf(&tmp_UUID[i * 2], "%02" PRIx16, service->service_uuid[i]);
      }

      LogInfo(" -> BLE SERVICE DISCOVERED:\nidx = %" PRIu16 ", UUID = %s\n",
              service_index, tmp_UUID);
      break;

    case W6X_BLE_EVT_CHAR_FOUND_ID:
      service_index = p_param_ble_data->Service.service_idx;
      charac_index = p_param_ble_data->Service.charac[0].char_idx;
      charac_handle = p_param_ble_data->Service.charac[0].char_handle;
      charac_value_handle = p_param_ble_data->Service.charac[0].char_value_handle;

      memset(tmp_UUID, 0x20, 33);

      uuid_size = p_param_ble_data->Service.charac[0].uuid_type == W6X_BLE_UUID_TYPE_16 ? 4 : 16;
      for (int32_t i = 0; i < uuid_size; i++)
      {
        sprintf(&tmp_UUID[i * 2], "%02" PRIx16, p_param_ble_data->Service.charac[0].char_uuid[i]);
      }

      LogInfo(" -> BLE CHARACTERISTIC DISCOVERED:\nService idx = %" PRIu16 ", Charac idx = %" PRIu16
              ", UUID = %s, \r\nChar Handle = %" PRIu32 ",Char Value Handle = %" PRIu32 "\n",
              service_index, charac_index, tmp_UUID, charac_handle, charac_value_handle);
      break;

    case W6X_BLE_EVT_PASSKEY_ENTRY_ID:
      LogInfo(" -> BLE PassKey Entry: Conn_Handle: %" PRIu16 "\n", p_param_ble_data->remote_ble_device.conn_handle);
      LogInfo("    BD Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
              p_param_ble_data->remote_ble_device.BDAddr[0], p_param_ble_data->remote_ble_device.BDAddr[1],
              p_param_ble_data->remote_ble_device.BDAddr[2], p_param_ble_data->remote_ble_device.BDAddr[3],
              p_param_ble_data->remote_ble_device.BDAddr[4], p_param_ble_data->remote_ble_device.BDAddr[5]);
      LogInfo("    BD Addr type: %" PRIu32 "\n", p_param_ble_data->remote_ble_device.bd_addr_type);
      break;

    case W6X_BLE_EVT_PASSKEY_CONFIRM_ID:
      LogInfo(" -> BLE PassKey received = %06" PRIu32 ", Conn_Handle: %" PRIu16 "\n", p_param_ble_data->PassKey,
              p_param_ble_data->remote_ble_device.conn_handle);
      LogInfo("    BD Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
              p_param_ble_data->remote_ble_device.BDAddr[0], p_param_ble_data->remote_ble_device.BDAddr[1],
              p_param_ble_data->remote_ble_device.BDAddr[2], p_param_ble_data->remote_ble_device.BDAddr[3],
              p_param_ble_data->remote_ble_device.BDAddr[4], p_param_ble_data->remote_ble_device.BDAddr[5]);
      LogInfo("    BD Addr type: %" PRIu32 "\n", p_param_ble_data->remote_ble_device.bd_addr_type);
      break;

    case W6X_BLE_EVT_PAIRING_CONFIRM_ID:
      LogInfo(" -> BLE Pairing Confirm: Conn_Handle: %" PRIu16 "\n", p_param_ble_data->remote_ble_device.conn_handle);
      LogInfo("    BD Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
              p_param_ble_data->remote_ble_device.BDAddr[0], p_param_ble_data->remote_ble_device.BDAddr[1],
              p_param_ble_data->remote_ble_device.BDAddr[2], p_param_ble_data->remote_ble_device.BDAddr[3],
              p_param_ble_data->remote_ble_device.BDAddr[4], p_param_ble_data->remote_ble_device.BDAddr[5]);
      LogInfo("    BD Addr type: %" PRIu32 "\n", p_param_ble_data->remote_ble_device.bd_addr_type);
      break;

    case W6X_BLE_EVT_PAIRING_COMPLETED_ID:
      LogInfo(" -> BLE Pairing Completed\n\n");
      LogInfo("    BD Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
              p_param_ble_data->remote_ble_device.BDAddr[0], p_param_ble_data->remote_ble_device.BDAddr[1],
              p_param_ble_data->remote_ble_device.BDAddr[2], p_param_ble_data->remote_ble_device.BDAddr[3],
              p_param_ble_data->remote_ble_device.BDAddr[4], p_param_ble_data->remote_ble_device.BDAddr[5]);
      LogInfo("    BD Addr type: %" PRIu32 "\n", p_param_ble_data->remote_ble_device.bd_addr_type);
      LogInfo("    LTK: %s\n", p_param_ble_data->LongTermKey);
      break;

    case W6X_BLE_EVT_PASSKEY_DISPLAY_ID:
      LogInfo(" -> BLE PASSKEY  = %06" PRIu32 "\n", p_param_ble_data->PassKey);
      break;

    case W6X_BLE_EVT_PAIRING_FAILED_ID:
      LogInfo(" -> BLE Pairing Failed: Conn_Handle: %" PRIu16 "\n", p_param_ble_data->remote_ble_device.conn_handle);
      LogInfo("    BD Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
              p_param_ble_data->remote_ble_device.BDAddr[0], p_param_ble_data->remote_ble_device.BDAddr[1],
              p_param_ble_data->remote_ble_device.BDAddr[2], p_param_ble_data->remote_ble_device.BDAddr[3],
              p_param_ble_data->remote_ble_device.BDAddr[4], p_param_ble_data->remote_ble_device.BDAddr[5]);
      LogInfo("    BD Addr type: %" PRIu32 "\n", p_param_ble_data->remote_ble_device.bd_addr_type);
      break;

    case W6X_BLE_EVT_PAIRING_CANCELED_ID:
      LogInfo(" -> BLE Pairing Canceled: Conn_Handle: %" PRIu16 "\n", p_param_ble_data->remote_ble_device.conn_handle);
      LogInfo("    BD Addr: %02X:%02X:%02X:%02X:%02X:%02X\n",
              p_param_ble_data->remote_ble_device.BDAddr[0], p_param_ble_data->remote_ble_device.BDAddr[1],
              p_param_ble_data->remote_ble_device.BDAddr[2], p_param_ble_data->remote_ble_device.BDAddr[3],
              p_param_ble_data->remote_ble_device.BDAddr[4], p_param_ble_data->remote_ble_device.BDAddr[5]);
      LogInfo("    BD Addr type: %" PRIu32 "\n", p_param_ble_data->remote_ble_device.bd_addr_type);
      break;

    default:
      break;
  }
  /* USER CODE BEGIN APP_ble_cb_End */

  /* USER CODE END APP_ble_cb_End */
}

static void APP_error_cb(W6X_Status_t ret_w6x, char const *func_name)
{
  /* USER CODE BEGIN APP_error_cb_1 */

  /* USER CODE END APP_error_cb_1 */
  LogError("W6X Error in [%s]: %s (code=%" PRIi32 ")\n",
           func_name, W6X_StatusToStr(ret_w6x), ret_w6x);
  /* USER CODE BEGIN APP_error_cb_2 */

  /* USER CODE END APP_error_cb_2 */
}

void APP_setevent(EventGroupHandle_t *app_event, uint32_t evt)
{
  /* USER CODE BEGIN APP_setevent_1 */

  /* USER CODE END APP_setevent_1 */
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (xPortIsInsideInterrupt())
  {
    xEventGroupSetBitsFromISR(*app_event, evt, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken)
    {
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
  else
  {
    xEventGroupSetBits(*app_event, evt);
  }
  /* USER CODE BEGIN APP_setevent_End */

  /* USER CODE END APP_setevent_End */
}

/* USER CODE BEGIN PFD */

/* USER CODE END PFD */

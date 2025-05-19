/**
  ******************************************************************************
  * @file    w6x_wifi.c
  * @author  GPM Application Team
  * @brief   This file provides code for W6x WiFi API
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
#include <string.h>
#include "w6x_api.h"       /* Prototypes of the functions implemented in this file */
#include "w6x_internal.h"
#include "w61_at_api.h"    /* Prototypes of the functions called by this file */
#include "w61_io.h"        /* Prototypes of the BUS functions to be registered */
#include "common_parser.h" /* Common Parser functions */
#include "event_groups.h"

/* Global variables ----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/** @defgroup ST67W6X_Private_WiFi_Types ST67W6X Wi-Fi Types
  * @ingroup  ST67W6X_Private_WiFi
  * @{
  */
/**
  * @brief  Internal Wi-Fi context
  */
typedef struct
{
  EventGroupHandle_t Wifi_event;            /*!< Wi-Fi event group */
  W6X_StaStateType_e StaState;              /*!< Wi-Fi station state */
  W6X_ApStateType_e ApState;                /*!< Wi-Fi access point state */
  uint32_t Expected_event_connect;          /*!< Expected event for connection */
  uint32_t Expected_event_gotip;            /*!< Expected event for IP address */
  uint32_t Expected_event_disconnect;       /*!< Expected event for disconnection */
  struct
  {
    uint32_t Expected_event_sta_disconnect; /*!< Expected event for station disconnection */
    uint8_t MAC[6];                         /*!< MAC address of the station */
  } evt_sta_disconnect;                     /*!< Station disconnection event structure */
} W6X_WiFiCtx_t;

/** @} */

/* Private defines -----------------------------------------------------------*/
/** @defgroup ST67W6X_Private_WiFi_Constants ST67W6X Wi-Fi Constants
  * @ingroup  ST67W6X_Private_WiFi
  * @{
  */
#define EVENT_FLAG_CONNECT        (1<<1)    /*!< Connected event bitmask */
#define EVENT_FLAG_GOT_IP         (1<<2)    /*!< Got IP event bitmask */
#define EVENT_FLAG_DISCONNECT     (1<<3)    /*!< Disconnected event bitmask */
#define EVENT_FLAG_REASON         (1<<4)    /*!< Reason event bitmask */
#define EVENT_FLAG_STA_DISCONNECT (1<<5)    /*!< Station disconnected event bitmask */

/** Delay before to declare the connect in failure */
#define CONNECT_TIMEOUT_MS        10000

/** Delay before to declare the IP acquisition in failure */
#define GOT_IP_TIMEOUT_MS         15000

/** Delay before to declare the disconnect in failure */
#define DISCONNECT_TIMEOUT_MS     5000

/** Delay before to declare the station disconnect in failure */
#define STA_DISCONNECT_TIMEOUT_MS 5000

/** @} */

/* Private macros ------------------------------------------------------------*/
/** @defgroup ST67W6X_Private_WiFi_Macros ST67W6X Wi-Fi Macros
  * @ingroup  ST67W6X_Private_WiFi
  * @{
  */
/* -------------------------------------------------------------------- */
/** Status Codes
  * when an error action is taking place the status code can indicate what the status code.
  */
#define W6X_WIFI_LIST(X) \
  X(WLAN_FW_SUCCESSFUL, 0) \
  X(WLAN_FW_TX_AUTH_FRAME_ALLOCATE_FAILURE, 1) \
  X(WLAN_FW_AUTHENTICATION_FAILURE, 2) \
  X(WLAN_FW_AUTH_ALGO_FAILURE, 3) \
  X(WLAN_FW_TX_ASSOC_FRAME_ALLOCATE_FAILURE, 4) \
  X(WLAN_FW_ASSOCIATE_FAILURE, 5) \
  X(WLAN_FW_DEAUTH_BY_AP_WHEN_NOT_CONNECTION, 6) \
  X(WLAN_FW_DEAUTH_BY_AP_WHEN_CONNECTION, 7) \
  X(WLAN_FW_4WAY_HANDSHAKE_ERROR_PSK_TIMEOUT_FAILURE, 8) \
  X(WLAN_FW_4WAY_HANDSHAKE_TX_DEAUTH_FRAME_TRANSMIT_FAILURE, 9) \
  X(WLAN_FW_4WAY_HANDSHAKE_TX_DEAUTH_FRAME_ALLOCATE_FAILURE, 10) \
  X(WLAN_FW_AUTH_OR_ASSOC_RESPONSE_TIMEOUT_FAILURE, 11) \
  X(WLAN_FW_SCAN_NO_BSSID_AND_CHANNEL, 12) \
  X(WLAN_FW_CREATE_CHANNEL_CTX_FAILURE_WHEN_JOIN_NETWORK, 13) \
  X(WLAN_FW_JOIN_NETWORK_FAILURE, 14) \
  X(WLAN_FW_ADD_STA_FAILURE, 15) \
  X(WLAN_FW_BEACON_LOSS, 16) \
  X(WLAN_FW_NETWORK_SECURITY_NOMATCH, 17) \
  X(WLAN_FW_NETWORK_WEPLEN_ERROR, 18) \
  X(WLAN_FW_DISCONNECT_BY_USER_WITH_DEAUTH, 19) \
  X(WLAN_FW_DISCONNECT_BY_USER_NO_DEAUTH, 20) \
  X(WLAN_FW_DISCONNECT_BY_FW_PS_TX_NULLFRAME_FAILURE, 21) \
  X(WLAN_FW_TRAFFIC_LOSS, 22) \
  X(WLAN_FW_SWITCH_CHANNEL_FAILURE, 23) \
  X(WLAN_FW_AUTH_OR_ASSOC_RESPONSE_CFM_FAILURE, 24) \
  X(WLAN_FW_REASSOCIATE_STARING, 25) \
  X(WLAN_FW_LAST, 26)

/* -------------------------------------------------------------------- */
/** AP Mode Status Codes
  * when an error action is taking place the status code can indicate what the status code.
  */
#define W6X_WIFI_AP_LIST(X) \
  X(WLAN_FW_APM_SUCCESSFUL, 0) \
  X(WLAN_FW_APM_DELETESTA_BY_USER, 1) \
  X(WLAN_FW_APM_DEATUH_BY_STA, 2) \
  X(WLAN_FW_APM_DISASSOCIATE_BY_STA, 3) \
  X(WLAN_FW_APM_DELETECONNECTION_TIMEOUT, 4) \
  X(WLAN_FW_APM_DELETESTA_FOR_NEW_CONNECTION, 5) \
  X(WLAN_FW_APM_DEAUTH_BY_AUTHENTICATOR, 6) \
  X(WLAN_FW_APM_LAST, 7)

/** Generate enum */
#define ENUM_ENTRY(name, value) name = value,

/** Generate string array */
#define STRING_ENTRY(name, value) #name,

/** @} */

/* Private variables ---------------------------------------------------------*/
/** @defgroup ST67W6X_Private_WiFi_Variables ST67W6X Wi-Fi Variables
  * @ingroup  ST67W6X_Private_WiFi
  * @{
  */
static W61_Object_t *p_DrvObj; /*!< Global W61 context pointer */

/** Wi-Fi private context */
static W6X_WiFiCtx_t *p_wifi_ctx = NULL;

/** Wi-Fi security string */
static const char *const W6X_WiFi_Security_str[] =
{
  "OPEN", "WEP", "WPA", "WPA2", "WPA-WPA2", "WPA-EAP", "WPA3-SAE", "WPA2-WPA3-SAE", "UNKNOWN"
};

/** Wi-Fi state string */
static const char *const W6X_WiFi_State_str[] =
{
  "NO STARTED CONNECTION", "STA CONNECTED", "STA GOT IP", "STA CONNECTING", "STA DISCONNECTED", "STA OFF"
};

/** Wi-Fi context pointer error string */
static const char W6X_WiFi_Ctx_Null_str[] = "WiFi context not initialized";

/** Wi-Fi state error string */
typedef enum
{
  W6X_WIFI_LIST(ENUM_ENTRY)
} W6X_WiFi_Status_e;

/** Wi-Fi state string */
static const char *W6X_WiFi_Status_str[] =
{
  W6X_WIFI_LIST(STRING_ENTRY)
};

#if 0
/** Wi-Fi AP status error string */
typedef enum
{
  W6X_WIFI_AP_LIST(ENUM_ENTRY)
} W6X_WiFi_AP_Status_e;

/** Wi-Fi AP status string */
static const char *W6X_WiFi_AP_Status_str[] =
{
  W6X_WIFI_AP_LIST(STRING_ENTRY)
};
#endif /* Not used */

/** @} */

/* Private function prototypes -----------------------------------------------*/
/** @defgroup ST67W6X_Private_WiFi_Functions ST67W6X Wi-Fi Functions
  * @ingroup  ST67W6X_Private_WiFi
  * @{
  */
/**
  * @brief  WIFI station callback function
  * @param  event_id: event ID
  * @param  event_args: event arguments
  */
static void W6X_WiFi_Sta_cb(W61_event_id_t event_id, void *event_args);

/**
  * @brief  WIFI Access Point callback function
  * @param  event_id: event ID
  * @param  event_args: event arguments
  */
static void W6X_WiFi_Ap_cb(W61_event_id_t event_id, void *event_args);

/**
  * @brief  Check if the STA default gateway and the soft-AP IP address are in the same subnet
  * @retval W6X_Status_t
  */
static W6X_Status_t W6X_WiFi_CheckSubnetIp(void);
/** @} */

/* Functions Definition ------------------------------------------------------*/
/** @addtogroup ST67W6X_API_WiFi_Public_Functions
  * @{
  */
W6X_Status_t W6X_WiFi_Init(void)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  W6X_App_Cb_t *p_cb_handler;
  W61_DhcpType_e State = W61_WIFI_DHCP_STA_AP_ENABLED;
  uint32_t zero = 0;
  uint32_t one = 1;
  uint32_t policy = W6X_ADAPTIVE_COUNTRY_CODE;          /* Set the default policy */
  char *code = W6X_COUNTRY_CODE;                        /* Set the default country code */
  uint8_t hostname[33] = {0};
  uint8_t Ipaddr[4] = W6X_AP_IP_SUBNET;
  uint8_t Netmask_addr[4] = {255, 255, 255, 0};
  Ipaddr[3] = 1; /* Set the last digit of the soft-AP IP address to 1 */

  /* Get the global W61 context pointer */
  p_DrvObj = W61_ObjGet();
  if (p_DrvObj == NULL)
  {
    goto _err;
  }

  /* Allocate the Wi-Fi context */
  p_wifi_ctx = pvPortCalloc(sizeof(W6X_WiFiCtx_t), 1);
  if (p_wifi_ctx == NULL)
  {
    LogError("Could not initialize WiFi context structure");
    goto _err;
  }

  /* Create the Wi-Fi event handle */
  p_wifi_ctx->Wifi_event = xEventGroupCreate();

  /* Check that application callback is registered */
  p_cb_handler = W6X_GetCbHandler();
  if ((p_cb_handler == NULL) || (p_cb_handler->APP_wifi_cb == NULL))
  {
    LogError("ERR: Please register the APP callback before initializing the module");
    goto _err;
  }

  /* Register W61 driver callbacks */
  W61_RegisterULcb(p_DrvObj,
                   W6X_WiFi_Sta_cb,
                   W6X_WiFi_Ap_cb,
                   NULL,
                   NULL,
                   NULL);

  if ((uint32_t)W6X_DHCP > W61_WIFI_DHCP_STA_AP_ENABLED)
  {
    LogError("Invalid DHCP configuration");
    goto _err;
  }

  /* Start the Wi-Fi as station */
  if (W6X_WiFi_StartSta() != W6X_STATUS_OK)
  {
    goto _err;
  }

#if 0
  /* Set auto connect to value defined in w61_driver_config.h */
  ret = TranslateErrorStatus(W61_WiFi_SetAutoConnect(p_DrvObj, W6X_AUTOCONNECT));
  if (ret != W6X_STATUS_OK)
  {
    goto _err;
  }
#endif /* Auto connect unused while store mode is disabled */

  /* Reset the DHCP global configuration */
  ret = TranslateErrorStatus(W61_WiFi_SetDhcpConfig(p_DrvObj, &State, &zero));
  if (ret != W6X_STATUS_OK)
  {
    goto _err;
  }

  /* Set the defined DHCP global configuration */
  State = (W61_DhcpType_e)W6X_DHCP;
  ret = TranslateErrorStatus(W61_WiFi_SetDhcpConfig(p_DrvObj, &State, &one));
  if (ret != W6X_STATUS_OK)
  {
    goto _err;
  }

  /* Set the country code */
  ret = TranslateErrorStatus(W61_WiFi_SetCountryCode(p_DrvObj, &policy, code));
  if (ret != W6X_STATUS_OK)
  {
    goto _err;
  }

  /* Check the length of the defined hostname */
  if (strlen(W6X_HOSTNAME) >= sizeof(hostname))
  {
    LogError("Hostname too long");
    goto _err;
  }

  strncpy((char *)hostname, W6X_HOSTNAME, sizeof(hostname) - 1);
  /* Set the hostname */
  ret = TranslateErrorStatus(W61_WiFi_SetHostname(p_DrvObj, hostname));
  if (ret != W6X_STATUS_OK)
  {
    goto _err;
  }

  /* Set default IP of the soft-AP */
  ret = TranslateErrorStatus(W61_WiFi_SetApIpAddress(p_DrvObj, Ipaddr, Netmask_addr));

_err:
  return ret;
}

void W6X_WiFi_DeInit(void)
{
  if (p_wifi_ctx != NULL)
  {
    /* Delete the Wi-Fi event handle */
    vEventGroupDelete(p_wifi_ctx->Wifi_event);
    p_wifi_ctx->Wifi_event = NULL;

    /* Free the Wi-Fi context */
    vPortFree(p_wifi_ctx);
    p_wifi_ctx = NULL;
  }
}

W6X_Status_t W6X_WiFi_StartSta(void)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);
  NULL_ASSERT(p_wifi_ctx, W6X_WiFi_Ctx_Null_str);

  /* Start the Wi-Fi as station */
  ret = TranslateErrorStatus(W61_WiFi_ActivateSta(p_DrvObj));

  if (ret == W6X_STATUS_OK)
  {
    /* Set the station state */
    p_wifi_ctx->StaState = W6X_WIFI_STATE_STA_DISCONNECTED;
    p_DrvObj->WifiCtx.StaState = W61_WIFI_STATE_STA_DISCONNECTED;
    /* Set the access point state */
    p_wifi_ctx->ApState = W6X_WIFI_STATE_AP_OFF;
    p_DrvObj->WifiCtx.ApState = W61_WIFI_STATE_AP_OFF;
  }

  return ret;
}

W6X_Status_t W6X_WiFi_Scan(W6X_Scan_Opts_t *Opts, scan_result_cb_t cb)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);
  NULL_ASSERT(cb, "Invalid callback");

  /* Set the scan callback */
  p_DrvObj->WifiCtx.scan_done_cb = (W61_scan_result_cb_t)cb;

  /* Set the scan options */
  ret = TranslateErrorStatus(W61_WiFi_SetScanOpts(p_DrvObj, (W61_Scan_Opts_t *)Opts));

  if (ret == W6X_STATUS_OK)
  {
    /* Start the scan */
    ret = TranslateErrorStatus(W61_WiFi_Scan(p_DrvObj));
  }

  /* Save the scan command status for callback */
  p_DrvObj->WifiCtx.scan_status = ret;
  return ret;
}

void W6X_WiFi_PrintScan(W6X_Scan_Result_t *Scan_results)
{
  if (Scan_results->Count == 0)
  {
    LogInfo("No scan results");
  }
  else
  {
    /* Print the scan results */
    for (uint32_t count = 0; count < Scan_results->Count; count++)
    {
      /* Print the mandatory fields from the scan results */
      LogInfo("MAC : [" MACSTR "] | Channel: %2d | %13.13s | RSSI: %4d | SSID: %32.32s",
              MAC2STR(Scan_results->AP[count].MAC),
              Scan_results->AP[count].Channel,
              W6X_WiFi_SecurityToStr(Scan_results->AP[count].Security),
              Scan_results->AP[count].RSSI,
              Scan_results->AP[count].SSID);

      /* Wait few ms to avoid logging buffer overflow */
      vTaskDelay(5);
    }
  }
}

W6X_Status_t W6X_WiFi_Connect(W6X_Connect_Opts_t *ConnectOpts)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  W61_Connect_Opts_t *connect_opts = (W61_Connect_Opts_t *)ConnectOpts;
  EventBits_t eventBits;
  W6X_App_Cb_t *p_cb_handler = W6X_GetCbHandler();
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);
  NULL_ASSERT(p_wifi_ctx, W6X_WiFi_Ctx_Null_str);

  if ((p_cb_handler == NULL) || (p_cb_handler->APP_wifi_cb == NULL))
  {
    LogError("ERR: Please register the APP callback before initializing the module");
    return ret;
  }

  /* Start the Wi-Fi connection to the Access Point */
  ret = TranslateErrorStatus(W61_WiFi_Connect(p_DrvObj, connect_opts, CONNECT_TIMEOUT_MS - 1000));
  if (ret == W6X_STATUS_OK)
  {
    p_wifi_ctx->Expected_event_connect = 1;
    LogInfo("NCP is treating the connection request");

    /* Wait for the connection to be done */
    eventBits = xEventGroupWaitBits(p_wifi_ctx->Wifi_event, EVENT_FLAG_CONNECT | EVENT_FLAG_REASON, pdTRUE,
                                    pdFALSE, pdMS_TO_TICKS(CONNECT_TIMEOUT_MS));

    p_wifi_ctx->Expected_event_connect = 0;

    /* Check the event bits */
    /* If the connection is successful, the CONNECT event is expected */
    if (eventBits & EVENT_FLAG_CONNECT)
    {
      /* Expected case */
    }
    /* If an error occurred, the Reason event is expected */
    else if (eventBits & EVENT_FLAG_REASON)
    {
      LogError("/!\\ WIFI CONNECT ERROR");
      /* Reset the station state */
      p_wifi_ctx->StaState = W6X_WIFI_STATE_STA_DISCONNECTED;
      return W6X_STATUS_ERROR;
    }
    /* If the connection event is not done in time */
    else
    {
      LogError("/!\\ WIFI CONNECT TIMEOUT");
      /* Reset the station state */
      p_wifi_ctx->StaState = W6X_WIFI_STATE_STA_DISCONNECTED;
      return W6X_STATUS_ERROR;
    }

    /* If station is not in static IP mode, GOT_IP event is expected */
    if (p_DrvObj->WifiCtx.DeviceConfig.DHCP_STA_IsEnabled == 1)
    {
      LogInfo("DHCP client start, this may take few seconds");
      p_wifi_ctx->Expected_event_gotip = 1;

      /* Wait for the IP address to be acquired */
      if ((int32_t)xEventGroupWaitBits(p_wifi_ctx->Wifi_event, EVENT_FLAG_GOT_IP, pdTRUE, pdFALSE,
                                       pdMS_TO_TICKS(GOT_IP_TIMEOUT_MS)) != EVENT_FLAG_GOT_IP)
      {
        LogError("/!\\ WIFI GOT IP TIMEOUT");
        p_wifi_ctx->Expected_event_gotip = 0;
        return W6X_STATUS_ERROR;
      }
    }

  }
  return ret;
}

/* TODO: Removed until reconnect / autoconnect feature fully functional */
#if 0
W6X_Status_t W6X_WiFi_SetAutoConnect(uint32_t OnOff)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Set auto connect state */
  ret = TranslateErrorStatus(W61_WiFi_SetAutoConnect(p_DrvObj, OnOff));
  if (ret == W6X_STATUS_OK)
  {
    LogInfo("Auto Connect state set succeed");
  }
  return ret;
}

W6X_Status_t W6X_WiFi_GetAutoConnect(uint32_t *OnOff)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Get auto Connect state */
  ret = TranslateErrorStatus(W61_WiFi_GetAutoConnect(p_DrvObj, OnOff));
  if (ret == W6X_STATUS_OK)
  {
    LogInfo("Get auto Connect state succeed");
  }
  return ret;
}
#endif /* Disable autoconnect feature */

W6X_Status_t W6X_WiFi_SetHostname(uint8_t Hostname[33])
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);
  NULL_ASSERT(p_wifi_ctx, W6X_WiFi_Ctx_Null_str);

  /* Check the station state */
  if (p_wifi_ctx->StaState == W6X_WIFI_STATE_STA_OFF)
  {
    LogError("Device is not in the appropriate state to run this command");
    return ret;
  }

  /* Set the host name */
  return TranslateErrorStatus(W61_WiFi_SetHostname(p_DrvObj, Hostname));
}

W6X_Status_t W6X_WiFi_GetHostname(uint8_t Hostname[33])
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);
  NULL_ASSERT(p_wifi_ctx, W6X_WiFi_Ctx_Null_str);

  /* Check the station state */
  if (p_wifi_ctx->StaState == W6X_WIFI_STATE_STA_OFF)
  {
    LogError("Device is not in the appropriate state to run this command");
    return ret;
  }

  /* Get the host name */
  return TranslateErrorStatus(W61_WiFi_GetHostname(p_DrvObj, Hostname));
}

W6X_Status_t W6X_WiFi_GetStaMacAddress(uint8_t Mac[6])
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Get the MAC address */
  return TranslateErrorStatus(W61_WiFi_GetStaMacAddress(p_DrvObj, Mac));
}

W6X_Status_t W6X_WiFi_GetStaIpAddress(uint8_t Ip_addr[4], uint8_t Gateway_addr[4], uint8_t Netmask_addr[4])
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);
  NULL_ASSERT(p_wifi_ctx, W6X_WiFi_Ctx_Null_str);

  /* Check if station is connected */
  if (!((p_wifi_ctx->StaState == W6X_WIFI_STATE_STA_CONNECTED) || (p_wifi_ctx->StaState == W6X_WIFI_STATE_STA_GOT_IP)))
  {
    LogError("Station is not connected. Connect to an Access Point before querying IPs");
    return ret;
  }

  /* Get the IP address */
  ret = TranslateErrorStatus(W61_WiFi_GetStaIpAddress(p_DrvObj));
  if (ret == W6X_STATUS_OK)
  {
    memcpy(Ip_addr, p_DrvObj->WifiCtx.NetSettings.IP_Addr, W6X_WIFI_SIZEOF_IPV4_BYTES);
    memcpy(Gateway_addr, p_DrvObj->WifiCtx.NetSettings.Gateway_Addr, W6X_WIFI_SIZEOF_IPV4_BYTES);
    memcpy(Netmask_addr, p_DrvObj->WifiCtx.NetSettings.IP_Mask, W6X_WIFI_SIZEOF_IPV4_BYTES);
  }
  return ret;
}

W6X_Status_t W6X_WiFi_SetStaIpAddress(uint8_t Ipaddr[4], uint8_t Gateway_addr[4], uint8_t Netmask_addr[4])
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);
  NULL_ASSERT(p_wifi_ctx, W6X_WiFi_Ctx_Null_str);

  /* Check if station is connected */
  if (!((p_wifi_ctx->StaState == W6X_WIFI_STATE_STA_CONNECTED) || (p_wifi_ctx->StaState == W6X_WIFI_STATE_STA_GOT_IP)))
  {
    LogError("Station is not connected. Connect to an Access Point before setting IPs");
    return ret;
  }

  /* Set the IP address */
  return TranslateErrorStatus(W61_WiFi_SetStaIpAddress(p_DrvObj, Ipaddr, Gateway_addr, Netmask_addr));
}

W6X_Status_t W6X_WiFi_GetDnsAddress(uint32_t *Dns_enable, uint8_t Dns1_addr[4], uint8_t Dns2_addr[4],
                                    uint8_t Dns3_addr[4])
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Get the DNS status and DNS addresses */
  ret = TranslateErrorStatus(W61_WiFi_GetDnsAddress(p_DrvObj));
  if (ret == W6X_STATUS_OK)
  {
    *Dns_enable = p_DrvObj->WifiCtx.NetSettings.DNS_isset;
    memcpy(Dns1_addr, p_DrvObj->WifiCtx.NetSettings.DNS1, W6X_WIFI_SIZEOF_IPV4_BYTES);
    memcpy(Dns2_addr, p_DrvObj->WifiCtx.NetSettings.DNS2, W6X_WIFI_SIZEOF_IPV4_BYTES);
    memcpy(Dns3_addr, p_DrvObj->WifiCtx.NetSettings.DNS3, W6X_WIFI_SIZEOF_IPV4_BYTES);
  }
  else
  {
    LogError("Station is not connected. Connect to an Access Point before querying  DNS IPs.");
  }
  return ret;
}

W6X_Status_t W6X_WiFi_SetDnsAddress(uint32_t *Dns_enable, uint8_t Dns1_addr[4], uint8_t Dns2_addr[4],
                                    uint8_t Dns3_addr[4])
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Set the DNS mode and DNS addresses */
  return TranslateErrorStatus(W61_WiFi_SetDnsAddress(p_DrvObj, Dns_enable, Dns1_addr, Dns2_addr, Dns3_addr));
}

W6X_Status_t W6X_WiFi_GetStaState(W6X_StaStateType_e *State, W6X_Connect_t *ConnectData)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  int32_t rssi = 0;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);
  NULL_ASSERT(p_wifi_ctx, W6X_WiFi_Ctx_Null_str);

  *State = p_wifi_ctx->StaState;

  /* Get the connection information if the Wi-Fi station is connected */
  if ((p_wifi_ctx->StaState == W6X_WIFI_STATE_STA_GOT_IP) || (p_wifi_ctx->StaState == W6X_WIFI_STATE_STA_CONNECTED))
  {
    ret = TranslateErrorStatus(W61_WiFi_GetConnectInfo(p_DrvObj, &rssi));
    if (ret == W6X_STATUS_OK)
    {
      memcpy(ConnectData->SSID, p_DrvObj->WifiCtx.NetSettings.SSID, W6X_WIFI_MAX_SSID_SIZE + 1);
      memcpy(ConnectData->MAC, p_DrvObj->WifiCtx.APSettings.MAC_Addr, 6);
      ConnectData->Rssi = rssi;
      ConnectData->Channel = p_DrvObj->WifiCtx.STASettings.Channel;
      ConnectData->Reconnection_interval = p_DrvObj->WifiCtx.STASettings.ReconnInterval;
    }
    else
    {
      LogWarn("Get connection information failed");
    }
  }
  else
  {
    ret = W6X_STATUS_OK;
  }
  return ret;
}

W6X_Status_t W6X_WiFi_GetCountryCode(uint32_t *Policy, char *CountryString)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Get the country code */
  return TranslateErrorStatus(W61_WiFi_GetCountryCode(p_DrvObj, Policy, CountryString));
}

W6X_Status_t W6X_WiFi_SetCountryCode(uint32_t *Policy, char *CountryString)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Set the country code */
  return TranslateErrorStatus(W61_WiFi_SetCountryCode(p_DrvObj, Policy, CountryString));
}

W6X_Status_t W6X_WiFi_Disconnect(void)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  W6X_App_Cb_t *p_cb_handler = W6X_GetCbHandler();
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);
  NULL_ASSERT(p_wifi_ctx, W6X_WiFi_Ctx_Null_str);

  if ((p_cb_handler == NULL) || (p_cb_handler->APP_wifi_cb == NULL))
  {
    LogError("ERR: Please register the APP callback before initializing the module");
    return ret;
  }

  /* Check if station is connected */
  if (!((p_wifi_ctx->StaState == W6X_WIFI_STATE_STA_CONNECTED) || (p_wifi_ctx->StaState == W6X_WIFI_STATE_STA_GOT_IP)))
  {
    LogError("Device is not in the appropriate state to run this command");
    return ret;
  }

  /* Disconnect the Wi-Fi station */
  p_wifi_ctx->Expected_event_disconnect = 1;
  ret = TranslateErrorStatus(W61_WiFi_Disconnect(p_DrvObj));
  if (ret == W6X_STATUS_OK)
  {
    /* Wait for the disconnection to be done */
    if ((int32_t)xEventGroupWaitBits(p_wifi_ctx->Wifi_event, EVENT_FLAG_DISCONNECT, pdTRUE, pdFALSE,
                                     pdMS_TO_TICKS(DISCONNECT_TIMEOUT_MS)) != EVENT_FLAG_DISCONNECT)
    {
      LogError("WIFI DISCONNECT TIMEOUT.");
      ret = W6X_STATUS_ERROR;
    }
  }

  p_wifi_ctx->Expected_event_disconnect = 0;

  return ret;
}

W6X_Status_t W6X_WiFi_StartAp(W6X_ApConfig_t *ap_config)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  W61_StaStateType_e previous_sta_state = W61_WIFI_STATE_STA_NO_STARTED_CONNECTION;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);
  NULL_ASSERT(p_wifi_ctx, W6X_Obj_Null_str);

  previous_sta_state = (W61_StaStateType_e)p_wifi_ctx->StaState;

  ret = TranslateErrorStatus(W61_WiFi_SetDualMode(p_DrvObj));
  if (ret == W6X_STATUS_OK)
  {
    if ((previous_sta_state == W61_WIFI_STATE_STA_CONNECTED) || (previous_sta_state == W61_WIFI_STATE_STA_GOT_IP))
    {
      p_wifi_ctx->Expected_event_connect = 1;
      /* Wait for the station to be connected */
      if ((int32_t)xEventGroupWaitBits(p_wifi_ctx->Wifi_event, EVENT_FLAG_CONNECT, pdTRUE, pdFALSE,
                                       pdMS_TO_TICKS(CONNECT_TIMEOUT_MS)) != EVENT_FLAG_CONNECT)
      {
        LogWarn("WIFI CLIENT RECONNECT TIMEOUT.");
      }
      else
      {
        LogInfo("Station reconnected to the Access Point");
      }
      p_wifi_ctx->Expected_event_connect = 0;
    }

    /* Activate the Access Point */
    ret = TranslateErrorStatus(W61_WiFi_ActivateAp(p_DrvObj, (W61_ApConfig_t *)ap_config));
  }
  return ret;
}

W6X_Status_t W6X_WiFi_StopAp(void)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  W61_Connected_Sta_t Stations = {0};
  uint8_t Reconnect = 1;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  ret = TranslateErrorStatus(W61_WiFi_ListConnectedSta(p_DrvObj, &Stations));

  if (ret == W6X_STATUS_OK)
  {
    if (Stations.Count != 0)
    {
      LogWarn("Soft AP is still connected to a station, they will be disconnected before stopping the soft-AP");
      for (int32_t i = 0; i < Stations.Count; i++)
      {
        ret = TranslateErrorStatus(W61_WiFi_DisconnectSta(p_DrvObj, Stations.STA[i].MAC));
        if (ret != W6X_STATUS_OK)
        {
          LogError("Failed to disconnect station");
          goto _err;
        }
      }
    }
  }

  /* Deactivate the Access Point */
  ret = TranslateErrorStatus(W61_WiFi_DeactivateAp(p_DrvObj, Reconnect));

_err:
  return ret;
}

W6X_Status_t W6X_WiFi_GetApConfig(W6X_ApConfig_t *ap_config)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  if (p_DrvObj->WifiCtx.ApState != W61_WIFI_STATE_AP_RUNNING)
  {
    LogError("Soft AP is not started");
    return ret;
  }

  /* Get the Access Point configuration */
  return TranslateErrorStatus(W61_WiFi_GetApConfig(p_DrvObj, (W61_ApConfig_t *)ap_config));
}

W6X_Status_t W6X_WiFi_ListConnectedSta(W6X_Connected_Sta_t *ConnectedSta)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Get the connected station list */
  return TranslateErrorStatus(W61_WiFi_ListConnectedSta(p_DrvObj, (W61_Connected_Sta_t *)ConnectedSta));
}

W6X_Status_t W6X_WiFi_DisconnectSta(uint8_t MAC[6])
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  p_wifi_ctx->evt_sta_disconnect.Expected_event_sta_disconnect = 1;
  /* Assign the MAC address to the event structure */
  for (int32_t i = 0; i < 6; i++)
  {
    p_wifi_ctx->evt_sta_disconnect.MAC[i] = MAC[i];
  }

  /* Disconnect the station */
  ret = TranslateErrorStatus(W61_WiFi_DisconnectSta(p_DrvObj, MAC));
  if (ret == W6X_STATUS_OK)
  {
    /* Wait for the disconnection to be done */
    if ((int32_t)xEventGroupWaitBits(p_wifi_ctx->Wifi_event, EVENT_FLAG_STA_DISCONNECT, pdTRUE, pdFALSE,
                                     pdMS_TO_TICKS(STA_DISCONNECT_TIMEOUT_MS)) != EVENT_FLAG_STA_DISCONNECT)
    {
      LogError("WIFI CLIENT DISCONNECT TIMEOUT.");
      ret = W6X_STATUS_ERROR;
    }
    else
    {
      LogInfo("WIFI CLIENT DISCONNECTED SUCCESSFULLY");
    }
  }
  p_wifi_ctx->evt_sta_disconnect.Expected_event_sta_disconnect = 0;
  return ret;
}

W6X_Status_t W6X_WiFi_GetApIpAddress(uint8_t Ipaddr[4], uint8_t Netmask_addr[4])
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Get the IP address */
  ret = TranslateErrorStatus(W61_WiFi_GetApIpAddress(p_DrvObj));
  if (ret == W6X_STATUS_OK)
  {
    memcpy(Ipaddr, p_DrvObj->WifiCtx.APSettings.IP_Addr, W6X_WIFI_SIZEOF_IPV4_BYTES);
    memcpy(Netmask_addr, p_DrvObj->WifiCtx.APSettings.IP_Mask, W6X_WIFI_SIZEOF_IPV4_BYTES);
  }
  return ret;
}

W6X_Status_t W6X_WiFi_GetDhcp(W6X_DhcpType_e *State, uint32_t *lease_time, uint8_t start_ip[4], uint8_t end_ip[4])
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Get the DHCP server configuration */
  ret = TranslateErrorStatus(W61_WiFi_GetDhcpsConfig(p_DrvObj, lease_time, start_ip, end_ip));
  if (ret == W6X_STATUS_OK)
  {
    /* Get the DHCP configuration */
    ret = TranslateErrorStatus(W61_WiFi_GetDhcpConfig(p_DrvObj, (W61_DhcpType_e *)State));
  }
  return ret;
}

W6X_Status_t W6X_WiFi_SetDhcp(W6X_DhcpType_e *State, uint32_t *Operate, uint32_t lease_time)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  W61_ApStateType_e previous_ap_state = p_DrvObj->WifiCtx.ApState;
  uint8_t Reconnect = 1;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Set the DHCP global configuration */
  ret = TranslateErrorStatus(W61_WiFi_SetDhcpConfig(p_DrvObj, (W61_DhcpType_e *)State, Operate));
  if (ret != W6X_STATUS_OK)
  {
    goto _err;
  }

  if ((*Operate == 1) && ((*State == W6X_WIFI_DHCP_AP_ENABLED) || (*State == W6X_WIFI_DHCP_STA_AP_ENABLED)))
  {
    /** We need to switch the soft-AP OFF before modifying DHCPS configuration */
    if (previous_ap_state == W61_WIFI_STATE_AP_RUNNING)
    {
      ret = TranslateErrorStatus(W61_WiFi_DeactivateAp(p_DrvObj, Reconnect));
      if (ret != W6X_STATUS_OK)
      {
        goto _err;
      }
    }

    /** Set the DHCP server configuration */
    ret = TranslateErrorStatus(W61_WiFi_SetDhcpsConfig(p_DrvObj, lease_time));
    if (ret != W6X_STATUS_OK)
    {
      LogError("DHCPS config set failed");
      goto _err;
    }

    if (previous_ap_state == W61_WIFI_STATE_AP_RUNNING)
    {
      ret = TranslateErrorStatus(W61_WiFi_SetDualMode(p_DrvObj));
      if (ret != W6X_STATUS_OK)
      {
        LogError("Soft AP failed to restart after DHCPS config change");
      }
    }
  }

_err:
  return ret;
}

W6X_Status_t W6X_WiFi_GetApMacAddress(uint8_t Mac[6])
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Get the MAC address */
  return TranslateErrorStatus(W61_WiFi_GetApMacAddress(p_DrvObj, Mac));
}

const char *W6X_WiFi_StateToStr(uint32_t state)
{
  /* Check if the state is unknown */
  if (state > W6X_WIFI_STATE_STA_OFF)
  {
    return "Unknown";
  }
  /* Return the state string */
  return W6X_WiFi_State_str[state];
}

const char *W6X_WiFi_SecurityToStr(uint32_t security)
{
  /* Check if the security is unknown */
  if (security > W6X_SECURITY_UNKNOWN)
  {
    return "Unknown";
  }
  /* Return the security string */
  return W6X_WiFi_Security_str[security];
}

const char *W6X_WiFi_ReasonToStr(void *reason)
{
  /* Check if the reason is unknown */
  if (*(uint32_t *)reason >= WLAN_FW_LAST)
  {
    return "Unknown";
  }
  /* Return the reason string */
  return W6X_WiFi_Status_str[*(uint32_t *)reason];
}

W6X_Status_t W6X_WiFi_SetDTIM(uint32_t dtim)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  return TranslateErrorStatus(W61_WiFi_SetDTIM(p_DrvObj, dtim));
}

W6X_Status_t W6X_WiFi_GetDTIM(uint32_t *dtim)
{
  return TranslateErrorStatus(W61_WiFi_GetDTIM(p_DrvObj, dtim));
}

/** @} */

/* Private Functions Definition ----------------------------------------------*/
/* =================== Callbacks ===================================*/
/** @addtogroup ST67W6X_Private_WiFi_Functions
  * @{
  */
static void W6X_WiFi_Sta_cb(W61_event_id_t event_id, void *event_args)
{
  W6X_App_Cb_t *p_cb_handler = W6X_GetCbHandler();
  uint32_t reason_code;

  if ((p_cb_handler == NULL) || (p_cb_handler->APP_wifi_cb == NULL))
  {
    LogError("ERR: Please register the APP callback before initializing the module");
    return;
  }

  if (p_DrvObj == NULL)
  {
    LogError("%s", W6X_Obj_Null_str);
    return;
  }
  if (p_wifi_ctx == NULL)
  {
    LogError("%s", W6X_WiFi_Ctx_Null_str);
    return;
  }

  switch (event_id) /* Check the event ID and call the appropriate callback */
  {
    case W61_WIFI_EVT_SCAN_DONE_ID:
      /* Call the scan done callback */
      p_DrvObj->WifiCtx.scan_done_cb(p_DrvObj->WifiCtx.scan_status, &p_DrvObj->WifiCtx.ScanResults);
      break;

    case W61_WIFI_EVT_CONNECTED_ID:
      /* Set the station state */
      p_wifi_ctx->StaState = W6X_WIFI_STATE_STA_CONNECTED;
      p_DrvObj->WifiCtx.StaState = W61_WIFI_STATE_STA_CONNECTED;

      if (p_wifi_ctx->Expected_event_connect == 1)
      {
        /* If the connected event was expected, set the event bit to release the wait */
        xEventGroupSetBits(p_wifi_ctx->Wifi_event, EVENT_FLAG_CONNECT);
      }

      /* Call the application callback to inform that the station is connected */
      p_cb_handler->APP_wifi_cb(W6X_WIFI_EVT_CONNECTED_ID, NULL);

      break;

    case W61_WIFI_EVT_GOT_IP_ID:
      /* Set the station state */
      p_wifi_ctx->StaState = W6X_WIFI_STATE_STA_GOT_IP;
      p_DrvObj->WifiCtx.StaState = W61_WIFI_STATE_STA_GOT_IP;

      /* Call the application callback to inform that the station got an IP */
      p_cb_handler->APP_wifi_cb(W6X_WIFI_EVT_GOT_IP_ID, (void *)NULL);

      if (p_DrvObj->WifiCtx.ApState == W61_WIFI_STATE_AP_RUNNING)
      {
        if (W6X_WiFi_CheckSubnetIp() != W6X_STATUS_OK)
        {
          LogError("Failed to check the subnet IP");
        }
      }
      if (p_wifi_ctx->Expected_event_gotip == 1)
      {
        /* If the IP address event was expected, set the event bit to release the wait */
        xEventGroupSetBits(p_wifi_ctx->Wifi_event, EVENT_FLAG_GOT_IP);
        p_wifi_ctx->Expected_event_gotip = 0;
      }

      break;

    case W61_WIFI_EVT_DISCONNECTED_ID:
      /* Set the station state */
      p_wifi_ctx->StaState = W6X_WIFI_STATE_STA_DISCONNECTED;
      p_DrvObj->WifiCtx.StaState = W61_WIFI_STATE_STA_DISCONNECTED;

      if (p_wifi_ctx->Expected_event_disconnect == 1)
      {
        /* If the disconnected event was expected, set the event bit to release the wait */
        xEventGroupSetBits(p_wifi_ctx->Wifi_event, EVENT_FLAG_DISCONNECT);
        p_wifi_ctx->Expected_event_disconnect = 0;
      }

      p_cb_handler->APP_wifi_cb(W6X_WIFI_EVT_DISCONNECTED_ID, NULL);

      break;

    case W61_WIFI_EVT_CONNECTING_ID:
      /* Set the station state */
      p_wifi_ctx->StaState = W6X_WIFI_STATE_STA_CONNECTING;
      p_DrvObj->WifiCtx.StaState = W61_WIFI_STATE_STA_CONNECTING;

      /* Call the application callback to inform that the station is connecting */
      p_cb_handler->APP_wifi_cb(W6X_WIFI_EVT_CONNECTING_ID, (void *)NULL);
      break;

    case W61_WIFI_EVT_REASON_ID:
      reason_code = *(uint32_t *)event_args;
      if (p_wifi_ctx->Expected_event_connect == 1)
      {
        /* If the error event was expected, set the event bit to release the wait */
        xEventGroupSetBits(p_wifi_ctx->Wifi_event, EVENT_FLAG_REASON);
      }

      /* Call the application callback to inform that an error occurred */
      p_cb_handler->APP_wifi_cb(W6X_WIFI_EVT_REASON_ID, (void *)&reason_code);

      break;

    default:
      break;
  }
}

static void W6X_WiFi_Ap_cb(W61_event_id_t event_id, void *event_args)
{
  W6X_App_Cb_t *p_cb_handler = W6X_GetCbHandler();

  if ((p_cb_handler == NULL) || (p_cb_handler->APP_wifi_cb == NULL))
  {
    LogError("ERR: Please register the APP callback before initializing the module");
    return;
  }

  if (p_wifi_ctx == NULL)
  {
    LogError("%s", W6X_WiFi_Ctx_Null_str);
    return;
  }

  switch (event_id) /* Check the event ID and call the appropriate callback */
  {
    case W61_WIFI_EVT_STA_CONNECTED_ID:
      /* Call the application callback to inform that a station is connected to the Access Point */
      p_cb_handler->APP_wifi_cb(W6X_WIFI_EVT_STA_CONNECTED_ID, event_args);
      break;

    case W61_WIFI_EVT_STA_DISCONNECTED_ID:
      if ((p_wifi_ctx->evt_sta_disconnect.Expected_event_sta_disconnect == 1) &&
          (strncmp((char *)p_wifi_ctx->evt_sta_disconnect.MAC,
                   (char *)((W61_CbParamWiFiData_t *)event_args)->MAC, 6) == 0))
      {
        /* If the disconnected event was expected, set the event bit to release the wait */
        xEventGroupSetBits(p_wifi_ctx->Wifi_event, EVENT_FLAG_STA_DISCONNECT);
        p_wifi_ctx->evt_sta_disconnect.Expected_event_sta_disconnect = 0;
      }
      /* Call the application callback to inform that a station is disconnected from the Access Point */
      p_cb_handler->APP_wifi_cb(W6X_WIFI_EVT_STA_DISCONNECTED_ID, event_args);
      break;

    case W61_WIFI_EVT_DIST_STA_IP_ID:
      /* Call the application callback to inform that a station has an IP address */
      p_cb_handler->APP_wifi_cb(W6X_WIFI_EVT_DIST_STA_IP_ID, event_args);
      break;

    default:
      break;
  }
}

static W6X_Status_t W6X_WiFi_CheckSubnetIp(void)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  uint8_t Ipaddr[4] = {0};
  uint8_t Gateway_addr[4] = {0};
  uint8_t Netmask_addr[4] = {0};
  uint8_t ApIpaddr[4] = {0};
  uint8_t ApIpaddrBackup[4] = W6X_AP_IP_SUBNET_BACKUP;
  uint8_t ApNetmask_addr[4] = {0};
  uint8_t Reconnect = 0;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  ApIpaddrBackup[3] = 1;

  /* Get the station IP address */
  ret = W6X_WiFi_GetStaIpAddress(Ipaddr, Gateway_addr, Netmask_addr);
  if (ret != W6X_STATUS_OK)
  {
    goto _err;
  }

  /* Get the AP IP address */
  ret = W6X_WiFi_GetApIpAddress(ApIpaddr, ApNetmask_addr);
  if (ret != W6X_STATUS_OK)
  {
    goto _err;
  }

  /* Check if the station and soft AP have the same subnet IP */
  if (memcmp(Gateway_addr, ApIpaddr, W6X_WIFI_SIZEOF_IPV4_BYTES) == 0)
  {
    LogWarn("Station and Soft AP have the same subnet IP, soft-AP IP will be changed");
    LogWarn("SOFT_AP AND STA WILL BE SHUTDOWN AND RESTARTED, SOFT_AP WITH NEW IP");

    ret = TranslateErrorStatus(W61_WiFi_DeactivateAp(p_DrvObj, Reconnect));
    if (ret != W6X_STATUS_OK)
    {
      goto _err;
    }

    /* Set the new IP address for the soft AP */
    ret = TranslateErrorStatus(W61_WiFi_SetApIpAddress(p_DrvObj, ApIpaddrBackup, ApNetmask_addr));
    if (ret != W6X_STATUS_OK)
    {
      goto _err;
    }

    /* Restart the soft AP */
    ret = TranslateErrorStatus(W61_WiFi_SetDualMode(p_DrvObj));
  }

_err:
  return ret;
}

/** @} */

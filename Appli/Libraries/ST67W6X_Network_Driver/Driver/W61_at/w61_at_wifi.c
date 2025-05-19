/**
  ******************************************************************************
  * @file    w61_at_wifi.c
  * @author  GPM Application Team
  * @brief   This file provides code for W61 WiFi AT module
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
#include <stdio.h>
#include "w61_at_api.h"
#include "w61_at_common.h"
#include "w61_at_internal.h"
#include "w61_at_rx_parser.h"
#include "common_parser.h" /* Common Parser functions */

#if (SYS_DBG_ENABLE_TA4 >= 1)
#include "trcRecorder.h"
#endif /* SYS_DBG_ENABLE_TA4 */

/* Global variables ----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/** @addtogroup ST67W61_AT_WiFi_Constants
  * @{
  */

#define RECONNECTION_INTERVAL       7200                                  /*!< Reconnection interval in seconds */
#define RECONNECTION_ATTEMPTS       1000                                  /*!< Reconnection attempts */

#define W61_WIFI_COUNTRY_CODE_MAX  5                                      /*!< Maximum number of country codes */
#define W61_WIFI_MAC_LENGTH        17                                     /*!< Length of a complete MAC address string */

#define W61_WIFI_EVT_SCAN_RESULT_KEYWORD          "+CWLAP:"               /*!< Scan result event keyword */
#define W61_WIFI_EVT_SCAN_DONE_KEYWORD            "+CW:SCAN_DONE"         /*!< Scan done event keyword */
#define W61_WIFI_EVT_CONNECTED_KEYWORD            "+CW:CONNECTED"         /*!< Connected event keyword */
#define W61_WIFI_EVT_DISCONNECTED_KEYWORD         "+CW:DISCONNECTED"      /*!< Disconnected event keyword */
#define W61_WIFI_EVT_GOT_IP_KEYWORD               "+CW:GOTIP"             /*!< Got IP event keyword */
#define W61_WIFI_EVT_CONNECTING_KEYWORD           "+CW:CONNECTING"        /*!< Connecting event keyword */
#define W61_WIFI_EVT_ERROR_KEYWORD                "+CW:ERROR,"            /*!< Error event keyword */

#define W61_WIFI_EVT_AP_STA_CONNECTED_KEYWORD     "+CW:STA_CONNECTED"     /*!< Station connected event keyword */
#define W61_WIFI_EVT_AP_STA_DISCONNECTED_KEYWORD  "+CW:STA_DISCONNECTED"  /*!< Station disconnected event keyword */
#define W61_WIFI_EVT_AP_STA_IP_KEYWORD            "+CW:DIST_STA_IP"       /*!< Station IP event keyword */

/** @} */

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/** @defgroup ST67W61_AT_WiFi_Variables ST67W61 AT Driver Wi-Fi Variables
  * @ingroup  ST67W61_AT_WiFi
  * @{
  */

/** @brief  Scan options structure */
W61_Scan_Opts_t ScanOptions = {"\0", "\0", WIFI_SCAN_ACTIVE, 0, 50};

/** @brief  Array of Alpha-2 country codes */
static const char *const Country_code_str[] = {"CN", "JP", "US", "EU", "00"};

/** sscanf parsing error string */
static const char W61_Parsing_Error_str[] = "Parsing of the result failed";

/** @} */

/* Private function prototypes -----------------------------------------------*/
/** @addtogroup ST67W61_AT_WiFi_Functions
  * @{
  */

/**
  * @brief  Parses Access point configuration.
  * @param  pdata: pointer to the data
  * @param  len: length of the data
  * @param  APs: Scan results structure containing the received information of beacons from Access Points
  */
static void W61_WiFi_AtParseAp(char *pdata, int32_t len, W61_Scan_Result_t *APs);

/* Functions Definition ------------------------------------------------------*/
void AT_WiFi_Event(W61_Object_t *Obj, const uint8_t *rxbuf, int32_t rxbuf_len)
{
  uint32_t temp_mac[6] = {0};
  uint32_t temp_ip[4] = {0};
  char *ptr = (char *)rxbuf;
  W61_CbParamWiFiData_t cb_param_wifi_data;
  if (Obj == NULL)
  {
    return;
  }

  if (strncmp(ptr, W61_WIFI_EVT_SCAN_DONE_KEYWORD, sizeof(W61_WIFI_EVT_SCAN_DONE_KEYWORD) - 1) == 0)
  {
    if (Obj->ulcbs.UL_wifi_sta_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_sta_cb(W61_WIFI_EVT_SCAN_DONE_ID, NULL);
    }
    return;
  }

  if (strncmp(ptr, W61_WIFI_EVT_SCAN_RESULT_KEYWORD, sizeof(W61_WIFI_EVT_SCAN_RESULT_KEYWORD) - 1) == 0)
  {
    if (Obj->WifiCtx.ScanResults.Count < W61_WIFI_MAX_DETECTED_AP)
    {
      W61_WiFi_AtParseAp((char *)rxbuf, rxbuf_len, &(Obj->WifiCtx.ScanResults));
    }
    return;
  }

  if (strncmp(ptr, W61_WIFI_EVT_CONNECTED_KEYWORD, sizeof(W61_WIFI_EVT_CONNECTED_KEYWORD) - 1) == 0)
  {
    if (Obj->ulcbs.UL_wifi_sta_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_sta_cb(W61_WIFI_EVT_CONNECTED_ID, NULL);
    }
    return;
  }

  if (strncmp(ptr, W61_WIFI_EVT_GOT_IP_KEYWORD, sizeof(W61_WIFI_EVT_GOT_IP_KEYWORD) - 1) == 0)
  {
    if (Obj->ulcbs.UL_wifi_sta_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_sta_cb(W61_WIFI_EVT_GOT_IP_ID, NULL);
    }
    return;
  }

  if (strncmp(ptr, W61_WIFI_EVT_DISCONNECTED_KEYWORD, sizeof(W61_WIFI_EVT_DISCONNECTED_KEYWORD) - 1) == 0)
  {
    /* Reset the context structure with all the information relative to the previous connection */
    memset(&Obj->WifiCtx.NetSettings, 0, sizeof(Obj->WifiCtx.NetSettings));
    memset(&Obj->WifiCtx.STASettings, 0, sizeof(Obj->WifiCtx.STASettings));
    if (Obj->ulcbs.UL_wifi_sta_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_sta_cb(W61_WIFI_EVT_DISCONNECTED_ID, NULL);
    }
    return;
  }

  if (strncmp(ptr, W61_WIFI_EVT_CONNECTING_KEYWORD, sizeof(W61_WIFI_EVT_CONNECTING_KEYWORD) - 1) == 0)
  {
    if (Obj->ulcbs.UL_wifi_sta_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_sta_cb(W61_WIFI_EVT_CONNECTING_ID, NULL);
    }
    return;
  }

  if (strncmp(ptr, W61_WIFI_EVT_ERROR_KEYWORD, sizeof(W61_WIFI_EVT_ERROR_KEYWORD) - 1) == 0)
  {
    static int32_t reason_code = 0;

    if (Obj->ulcbs.UL_wifi_sta_cb == NULL)
    {
      return;
    }

    ptr += sizeof(W61_WIFI_EVT_ERROR_KEYWORD) - 1;

    /* Parse the error code number from +CW:ERROR,19 */
    reason_code = ParseNumber(ptr, NULL);
    Obj->ulcbs.UL_wifi_sta_cb(W61_WIFI_EVT_REASON_ID, &reason_code);
    return;
  }

  if (strncmp(ptr, W61_WIFI_EVT_AP_STA_CONNECTED_KEYWORD, sizeof(W61_WIFI_EVT_AP_STA_CONNECTED_KEYWORD) - 1) == 0)
  {
    ptr += sizeof(W61_WIFI_EVT_AP_STA_CONNECTED_KEYWORD);
    if (sscanf(ptr, "\"%02lx:%02lx:%02lx:%02lx:%02lx:%02lx\"\r\n",
               &temp_mac[0], &temp_mac[1], &temp_mac[2], &temp_mac[3], &temp_mac[4], &temp_mac[5]) != 6)
    {
      LogError("Parsing of the MAC address failed");
      return;
    }

    /* Check the MAC address validity */
    for (int32_t i = 0; i < 6; i++)
    {
      if (temp_mac[i] > 0xFF)
      {
        LogError("MAC address is not valid.\r\n");
        return;
      }
      cb_param_wifi_data.MAC[i] = (uint8_t)temp_mac[i];
    }

    if (Obj->ulcbs.UL_wifi_ap_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_ap_cb(W61_WIFI_EVT_STA_CONNECTED_ID, &cb_param_wifi_data);
    }
    return;
  }

  if (strncmp(ptr, W61_WIFI_EVT_AP_STA_DISCONNECTED_KEYWORD,
              sizeof(W61_WIFI_EVT_AP_STA_DISCONNECTED_KEYWORD) - 1) == 0)
  {
    ptr += sizeof(W61_WIFI_EVT_AP_STA_DISCONNECTED_KEYWORD);
    if (sscanf(ptr, "\"%02lx:%02lx:%02lx:%02lx:%02lx:%02lx\"\r\n",
               &temp_mac[0], &temp_mac[1], &temp_mac[2], &temp_mac[3], &temp_mac[4], &temp_mac[5]) != 6)
    {
      LogError("Parsing of the MAC address failed");
      return;
    }

    /* Check the MAC address validity */
    for (int32_t i = 0; i < 6; i++)
    {
      if (temp_mac[i] > 0xFF)
      {
        LogError("MAC address is not valid.\r\n");
        return;
      }
      cb_param_wifi_data.MAC[i] = (uint8_t)temp_mac[i];
    }

    if (Obj->ulcbs.UL_wifi_ap_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_ap_cb(W61_WIFI_EVT_STA_DISCONNECTED_ID, &cb_param_wifi_data);
    }
    return;
  }

  if (strncmp(ptr, W61_WIFI_EVT_AP_STA_IP_KEYWORD, sizeof(W61_WIFI_EVT_AP_STA_IP_KEYWORD) - 1) == 0)
  {
    /* String returned : +CW:DIST_STA_IP "aa:bb:cc:dd:ee:ff","192.168.4.1"*/
    if (sscanf(ptr, "+CW:DIST_STA_IP \"%02lx:%02lx:%02lx:%02lx:%02lx:%02lx\",\"%lu.%lu.%lu.%lu\"\r\n",
               &temp_mac[0], &temp_mac[1], &temp_mac[2], &temp_mac[3], &temp_mac[4], &temp_mac[5],
               &temp_ip[0], &temp_ip[1], &temp_ip[2], &temp_ip[3]) != 10)
    {
      LogError("%s", W61_Parsing_Error_str);
      return;
    }

    /* Check the MAC address validity */
    for (int32_t i = 0; i < 6; i++)
    {
      if (temp_mac[i] > 0xFF)
      {
        LogError("MAC address is not valid.\r\n");
        return;
      }
      cb_param_wifi_data.MAC[i] = (uint8_t)temp_mac[i];
    }

    /* Check the IP address validity */
    for (int32_t i = 0; i < 4; i++)
    {
      if (temp_ip[i] > 0xFF)
      {
        LogError("IP address is not valid.\r\n");
        return;
      }
      cb_param_wifi_data.IP[i] = (uint8_t)temp_ip[i];
    }

    if (Obj->ulcbs.UL_wifi_ap_cb != NULL)
    {
      Obj->ulcbs.UL_wifi_ap_cb(W61_WIFI_EVT_DIST_STA_IP_ID, &cb_param_wifi_data);
    }
    return;
  }
}

W61_Status_t W61_WiFi_SetAutoConnect(W61_Object_t *Obj, uint32_t OnOff)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (!((OnOff == 0) || (OnOff == 1)))
  {
    LogError("Invalid value passed to Autoconnect function");
    return ret;
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWAUTOCONN=%lu\r\n", OnOff);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Obj->NcpTimeout);
    ATunlock(Obj);
    if (ret == W61_STATUS_OK)
    {
      Obj->WifiCtx.DeviceConfig.AutoConnect = OnOff;
    }
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_GetAutoConnect(W61_Object_t *Obj, uint32_t *OnOff)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(OnOff);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWAUTOCONN?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, Obj->NcpTimeout);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char *)Obj->CmdResp, "+CWAUTOCONN:%lu\r\n", OnOff) != 1)
      {
        LogError("%s", W61_Parsing_Error_str);
        ret = W61_STATUS_ERROR;
      }
      else
      {
        ret = W61_STATUS_OK;
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_SetHostname(W61_Object_t *Obj, uint8_t Hostname[33])
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWHOSTNAME=\"%s\"\r\n", Hostname);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Obj->NcpTimeout);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_GetHostname(W61_Object_t *Obj, uint8_t Hostname[33])
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWHOSTNAME?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_WIFI_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char *)Obj->CmdResp, "+CWHOSTNAME:%32[^\r\n]", Hostname) != 1)
      {
        ret = W61_STATUS_ERROR;
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_WiFi_ActivateSta(W61_Object_t *Obj)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWMODE=1,0\r\n");
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_WIFI_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_SetScanOpts(W61_Object_t *Obj, W61_Scan_Opts_t *ScanOpts)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint32_t max_cnt = 50;
  scan_type_e type =  WIFI_SCAN_ACTIVE;
  uint8_t SSID[W61_WIFI_MAX_SSID_SIZE + 1] = {'\0'};
  uint8_t MAC[6] = {'\0'};
  uint8_t channel = 0;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(ScanOpts);

  if ((ScanOpts->MaxCnt > 0) && (ScanOpts->MaxCnt < 50))
  {
    max_cnt = ScanOpts->MaxCnt;
  }
  if ((ScanOpts->scan_type == 0) || (ScanOpts->scan_type == 1))
  {
    ScanOptions.scan_type = ScanOpts->scan_type;
  }
  else
  {
    ScanOptions.scan_type = type;
  }
  if (ScanOpts->SSID[0] != '\0')
  {
    memcpy(ScanOptions.SSID, ScanOpts->SSID, W61_WIFI_MAX_SSID_SIZE + 1);
  }
  else
  {
    memcpy(ScanOptions.SSID, SSID, W61_WIFI_MAX_SSID_SIZE + 1);
  }
  if (ScanOpts->MAC[0] != '\0')
  {
    memcpy(ScanOptions.MAC, ScanOpts->MAC, 6);
  }
  else
  {
    memcpy(ScanOptions.MAC, MAC, 6);
  }
  if ((ScanOpts->Channel > 0) && (ScanOpts->Channel < 13))
  {
    ScanOptions.Channel = ScanOpts->Channel;
  }
  else
  {
    ScanOptions.Channel = channel;
  }

  /* The config of the scan options must be made before every scan */
  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWLAPOPT=1,2047,-100,255,%lu\r\n", max_cnt);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Obj->NcpTimeout);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_Scan(W61_Object_t *Obj)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char MAC[20 + 1];
  char SSID[W61_WIFI_MAX_SSID_SIZE + 3] = {'\0'}; /* Size + 2 if contains double-quote characters */
  W61_NULL_ASSERT(Obj);

  memset((uint8_t *)&Obj->WifiCtx.ScanResults, 0, sizeof(Obj->WifiCtx.ScanResults));

  snprintf(MAC, 20, "\"" MACSTR "\"", MAC2STR(ScanOptions.MAC));
  if (strcmp((char *)ScanOptions.SSID, "\0") == 0)
  {
    snprintf(SSID, W61_WIFI_MAX_SSID_SIZE + 3, "%s", ScanOptions.SSID);
  }
  else
  {
    snprintf(SSID, W61_WIFI_MAX_SSID_SIZE + 3, "\"%s\"", ScanOptions.SSID);
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWLAP=%d,%s,%s,%d\r\n", ScanOptions.scan_type, SSID,
             strcmp(MAC, "\"00:00:00:00:00:00\"") == 0 ? "" : MAC, ScanOptions.Channel);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Obj->NcpTimeout);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_SetReconnectionOpts(W61_Object_t *Obj, W61_Connect_Opts_t *ConnectOpts)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(ConnectOpts);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWRECONNCFG=%d,%d\r\n",
             ConnectOpts->Reconnection_interval, ConnectOpts->Reconnection_nb_attempts);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Obj->NcpTimeout);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_Connect(W61_Object_t *Obj, W61_Connect_Opts_t *ConnectOpts, uint32_t Timeout)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint32_t pos = 0;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(ConnectOpts);

  if (ConnectOpts->SSID[0] == '\0')
  {
    LogError("SSID cannot be NULL");
    return ret;
  }

  /* The config of the connect options must be made before every connect */
  if (ConnectOpts->Reconnection_interval > RECONNECTION_INTERVAL)
  {
    LogError("Reconnection interval must be between [0;7200], parameter set to default value : 0");
    ConnectOpts->Reconnection_interval = 0;
  }
  if (ConnectOpts->Reconnection_nb_attempts > RECONNECTION_ATTEMPTS)
  {
    LogError("Number of reconnection attempts must be between [0;1000], parameter set to default value : 0");
    ConnectOpts->Reconnection_nb_attempts = 0;
  }

  /* Setup the SSID and Password as required parameters */
  pos += snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWJAP=\"%s\",\"%s\",",
                  ConnectOpts->SSID, ConnectOpts->Password);

  /* Add optional BSSID parameters if defined */
  if (ConnectOpts->MAC[0] != '\0')
  {
    pos += snprintf((char *)&Obj->CmdResp[pos], W61_ATD_CMDRSP_STRING_SIZE - pos,
                    "\"" MACSTR "\"", MAC2STR(ConnectOpts->MAC));
  }

  /* Add optional WEP mode (always disabled) */
  snprintf((char *)&Obj->CmdResp[pos], W61_ATD_CMDRSP_STRING_SIZE - pos, ",0\r\n");

  if (ATlock(Obj, Timeout))
  {
    ret = AT_SetExecute(Obj, Obj->CmdResp, Timeout);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  if ((ret == W61_STATUS_OK) && (W61_WiFi_SetReconnectionOpts(Obj, ConnectOpts) != W61_STATUS_OK))
  {
    LogError("Connection configuration command issued");
    return W61_STATUS_ERROR;
  }

  return ret;
}

W61_Status_t W61_WiFi_GetConnectInfo(W61_Object_t *Obj, int32_t *Rssi)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char tmp_mac[18];
  int32_t cmp = 0;
  uint32_t wep = 0;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Rssi);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWJAP?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_WIFI_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      cmp = sscanf((char *)Obj->CmdResp, "+CWJAP:\"%32[^\"]\",\"%17[^\"]\",%ld,%ld,%ld\r\n",
                   (char *)Obj->WifiCtx.NetSettings.SSID, tmp_mac, &(Obj->WifiCtx.STASettings.Channel),
                   Rssi, &wep);
      if (cmp != 5)
      {
        LogError("Get connection information failed, %d parameters catch on 6", cmp);
        ret = W61_STATUS_ERROR;
      }
      else
      {
        ParseMAC(tmp_mac, Obj->WifiCtx.APSettings.MAC_Addr);
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_Disconnect(W61_Object_t *Obj)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWQAP\r\n");
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_WIFI_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_GetStaMacAddress(W61_Object_t *Obj, uint8_t *Mac)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char *ptr, *token;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Mac);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSTAMAC?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_WIFI_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      ptr = strstr((char *)(Obj->CmdResp), "+CIPSTAMAC:");
      if (ptr == NULL)
      {
        ret = W61_STATUS_ERROR;
      }
      else
      {
        ptr += sizeof("+CIPSTAMAC:");
        token = strstr(ptr, "\r");
        if (token == NULL)
        {
          ret = W61_STATUS_ERROR;
        }
        else
        {
          *(--token) = 0;
          ParseMAC(ptr, Mac);
        }
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_GetStaIpAddress(W61_Object_t *Obj)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char *ptr, *token;
  uint32_t line = 0;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPSTA?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Obj->NcpTimeout);
      while ((recv_len > 0) && (strncmp((char *)Obj->CmdResp, "+CIPSTA:", strlen("+CIPSTA:")) == 0))
      {
        Obj->CmdResp[recv_len] = 0;
        switch (line++)
        {
          case 0:
            ptr = strstr((char *)(Obj->CmdResp), "+CIPSTA:ip:");
            if (ptr == NULL)
            {
              goto _err;
            }
            ptr += sizeof("+CIPSTA:ip:");

            token = strstr(ptr, "\r");
            if (token == NULL)
            {
              goto _err;
            }

            *(--token) = 0;
            ParseIP(ptr, Obj->WifiCtx.NetSettings.IP_Addr);
            break;
          case 1:
            ptr = strstr((char *)(Obj->CmdResp), "+CIPSTA:gateway:");
            if (ptr == NULL)
            {
              goto _err;
            }
            ptr += sizeof("+CIPSTA:gateway:");

            token = strstr(ptr, "\r");
            if (token == NULL)
            {
              goto _err;
            }

            *(--token) = 0;
            ParseIP(ptr, Obj->WifiCtx.NetSettings.Gateway_Addr);
            break;
          case 2:
            ptr = strstr((char *)(Obj->CmdResp), "+CIPSTA:netmask:");
            if (ptr == NULL)
            {
              goto _err;
            }
            ptr += sizeof("+CIPSTA:netmask:");

            token = strstr(ptr, "\r");
            if (token == NULL)
            {
              goto _err;
            }
            *(--token) = 0;
            ParseIP(ptr, Obj->WifiCtx.NetSettings.IP_Mask);
            break;
          default:
            break;
        }
        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Obj->NcpTimeout);
      }

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;

_err:
  ATunlock(Obj);
  return W61_STATUS_ERROR;
}

W61_Status_t W61_WiFi_SetStaIpAddress(W61_Object_t *Obj, uint8_t Ip_addr[4], uint8_t Gateway_addr[4],
                                      uint8_t Netmask_addr[4])
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint8_t Gateway_addr_def[4];
  uint8_t Netmask_addr_def[4];
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT_STR(Ip_addr, "Station IP NULL");

  if (Is_ip_valid(Ip_addr) != W61_STATUS_OK)
  {
    LogError("Station IP is invalid.");
    goto _err;
  }

  /* Get the actual IP configuration to replace fields not specified in the function */
  ret = W61_WiFi_GetStaIpAddress(Obj);
  if (ret != W61_STATUS_OK)
  {
    goto _err;
  }

  if ((Gateway_addr == NULL) || (Is_ip_valid(Gateway_addr) != W61_STATUS_OK))
  {
    LogWarn("Gateway IP NULL or invalid. Previous one will be use.");
    memcpy(Gateway_addr_def, Obj->WifiCtx.NetSettings.Gateway_Addr, W61_WIFI_SIZEOF_IPV4_BYTES);
  }
  else
  {
    memcpy(Gateway_addr_def, Gateway_addr, W61_WIFI_SIZEOF_IPV4_BYTES);
  }

  if ((Netmask_addr == NULL) || (Is_ip_valid(Netmask_addr) != W61_STATUS_OK))
  {
    LogWarn("Netmask IP NULL or invalid. Previous one will be use.");
    memcpy(Netmask_addr_def, Obj->WifiCtx.NetSettings.IP_Mask, W61_WIFI_SIZEOF_IPV4_BYTES);
  }
  else
  {
    memcpy(Netmask_addr_def, Netmask_addr, W61_WIFI_SIZEOF_IPV4_BYTES);
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE,
             "AT+CIPSTA=\"" IPSTR "\",\"" IPSTR "\",\"" IPSTR "\"\r\n",
             IP2STR(Ip_addr), IP2STR(Gateway_addr_def), IP2STR(Netmask_addr_def));
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_WIFI_TIMEOUT);
    ATunlock(Obj);
    if (ret == W61_STATUS_OK)
    {
      Obj->WifiCtx.DeviceConfig.DHCP_STA_IsEnabled = 0;
    }
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

_err:
  return ret;
}

W61_Status_t W61_WiFi_GetStaState(W61_Object_t *Obj, W61_StaStateType_e *State, char *Ssid)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint32_t tmp = 0;
  int32_t result = 0;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(State);
  W61_NULL_ASSERT(Ssid);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWSTATE?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, Obj->NcpTimeout);
    if (ret == W61_STATUS_OK)
    {
      Ssid[0] = '\0';
      result = sscanf((const char *)Obj->CmdResp, "+CWSTATE:%lu,\"%[^\"]\"\r\n", &tmp, Ssid);

      if ((result != 1) && (result != 2))
      {
        LogError("%s", W61_Parsing_Error_str);
        ret = W61_STATUS_ERROR;
      }
      else
      {
        *State = (W61_StaStateType_e)tmp;
        if (!(tmp == W61_WIFI_STATE_STA_CONNECTED || tmp == W61_WIFI_STATE_STA_GOT_IP))
        {
          Ssid[0] = '\0';
        }
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_GetDhcpConfig(W61_Object_t *Obj, W61_DhcpType_e *State)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint32_t tmp = 0;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(State);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWDHCP?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, Obj->NcpTimeout);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((const char *)Obj->CmdResp, "+CWDHCP:%ld\r\n", &tmp) != 1)
      {
        LogError("%s", W61_Parsing_Error_str);
      }
      else
      {
        *State = (W61_DhcpType_e)tmp;
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_SetDhcpConfig(W61_Object_t *Obj, W61_DhcpType_e *State, uint32_t *Operate)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(State);
  W61_NULL_ASSERT(Operate);

  if (!((*Operate == 0) || (*Operate == 1)) || !((*State == 0) || (*State == 1) || (*State == 2) || (*State == 3)))
  {
    LogError("Incorrect parameters");
    return ret;
  }
  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWDHCP=%ld,%d\r\n", *Operate, *State);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Obj->NcpTimeout);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  if (ret == W61_STATUS_OK)
  {
    if ((*Operate == 0) && ((*State == 1) || (*State == 3)))
    {
      Obj->WifiCtx.DeviceConfig.DHCP_STA_IsEnabled = 0;
    }
    else if ((*Operate == 1) && ((*State == 1) || (*State == 3)))
    {
      Obj->WifiCtx.DeviceConfig.DHCP_STA_IsEnabled = 1;
    }
  }

  return ret;
}

W61_Status_t W61_WiFi_GetCountryCode(W61_Object_t *Obj, uint32_t *Policy, char *CountryString)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  CountryString[0] = '\0';
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Policy);
  W61_NULL_ASSERT(CountryString);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWCOUNTRY?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, Obj->NcpTimeout);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((const char *)Obj->CmdResp, "+CWCOUNTRY:%ld,\"%[^\"]\"\r\n",
                 Policy, CountryString) != 2)
      {
        LogError("%s", W61_Parsing_Error_str);
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_SetCountryCode(W61_Object_t *Obj, uint32_t *Policy, char *CountryString)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t code = 0;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Policy);
  W61_NULL_ASSERT(CountryString);

  for (code = 0; code < W61_WIFI_COUNTRY_CODE_MAX; code++)
  {
    if (strcasecmp(CountryString, Country_code_str[code]) == 0)
    {
      break;
    }
  }
  if (code >= W61_WIFI_COUNTRY_CODE_MAX)
  {
    LogError("Incorrect country code string.");
    return ret;
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWCOUNTRY=%lu,\"%s\"\r\n",
             *Policy, CountryString);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_WIFI_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_WiFi_GetDnsAddress(W61_Object_t *Obj)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint8_t dns1[17] = {0};
  uint8_t dns2[17] = {0};
  uint8_t dns3[17] = {0};
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPDNS?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_WIFI_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char *)Obj->CmdResp, "+CIPDNS:%ld,\"%16[^\"]\",\"%16[^\"]\",\"%16[^\"]\"\r\n",
                 &Obj->WifiCtx.NetSettings.DNS_isset, dns1, dns2, dns3) != 4)
      {
        LogError("Get DNS IP failed");
        ret = W61_STATUS_ERROR;
      }
      else
      {
        ParseIP((char *)dns1, Obj->WifiCtx.NetSettings.DNS1);
        ParseIP((char *)dns2, Obj->WifiCtx.NetSettings.DNS2);
        ParseIP((char *)dns3, Obj->WifiCtx.NetSettings.DNS3);
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_SetDnsAddress(W61_Object_t *Obj, uint32_t *State, uint8_t Dns1_addr[4], uint8_t Dns2_addr[4],
                                    uint8_t Dns3_addr[4])
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint32_t pos = 0;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(State);

  if (!((*State == 0) || (*State == 1)))
  {
    LogError("First parameter enable / disable is mandatory.");
    return ret;
  }

  pos += snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPDNS=%ld", *State);
  if (*State == 1)
  {
    if ((Dns1_addr == NULL) || (Is_ip_valid(Dns1_addr) != W61_STATUS_OK))
    {
      LogWarn("Dns1_addr IP NULL or invalid. DNS1 IP will not be set.");
    }
    else
    {
      pos += snprintf((char *)&Obj->CmdResp[pos], W61_ATD_CMDRSP_STRING_SIZE - pos,
                      ",\"" IPSTR "\"", IP2STR(Dns1_addr));
    }

    if ((Dns2_addr == NULL) || (Is_ip_valid(Dns2_addr) != W61_STATUS_OK))
    {
      LogWarn("Dns2_addr IP NULL or invalid. DNS2 IP will not be set.");
    }
    else
    {
      pos += snprintf((char *)&Obj->CmdResp[pos], W61_ATD_CMDRSP_STRING_SIZE - pos,
                      ",\"" IPSTR "\"", IP2STR(Dns2_addr));
    }

    if ((Dns3_addr == NULL) || (Is_ip_valid(Dns3_addr) != W61_STATUS_OK))
    {
      LogWarn("Dns3_addr IP NULL or invalid. DNS3 IP will not be set.");
    }
    else
    {
      pos += snprintf((char *)&Obj->CmdResp[pos], W61_ATD_CMDRSP_STRING_SIZE - pos,
                      ",\"" IPSTR "\"", IP2STR(Dns3_addr));
    }
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)&Obj->CmdResp[pos], W61_ATD_CMDRSP_STRING_SIZE - pos, "\r\n");
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_WIFI_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

/* Soft-Access Point related functions definition ----------------------------------------------*/
W61_Status_t W61_WiFi_SetDualMode(W61_Object_t *Obj)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint32_t sta_state = 0;
  W61_NULL_ASSERT(Obj);

  /* Check if station is connected */
  if ((Obj->WifiCtx.StaState == W61_WIFI_STATE_STA_CONNECTED) || (Obj->WifiCtx.StaState == W61_WIFI_STATE_STA_GOT_IP))
  {
    sta_state = 1;
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWMODE=3,%ld\r\n", sta_state);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_WIFI_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  if (ret == W61_STATUS_OK)
  {
    Obj->WifiCtx.ApState = W61_WIFI_STATE_AP_RUNNING;
  }

  return ret;
}

W61_Status_t W61_WiFi_ActivateAp(W61_Object_t *Obj, W61_ApConfig_t *ApConfig)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t tmp_rssi = 0;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(ApConfig);

  if (ApConfig->SSID[0] == '\0')
  {
    LogError("SSID cannot be NULL");
    return ret;
  }

  if (strlen((char *)ApConfig->SSID) > W61_WIFI_MAX_SSID_SIZE)
  {
    LogError("SSID is too long, maximum length is 32");
    return ret;
  }

  if (ApConfig->MaxConnections > W61_WIFI_MAX_CONNECTED_STATIONS)
  {
    LogWarn("Max connections number is too high, set to default value 10");
    ApConfig->MaxConnections = W61_WIFI_MAX_CONNECTED_STATIONS;
  }

  /* Get the channel the station is connected to */
  if ((Obj->WifiCtx.StaState == W61_WIFI_STATE_STA_CONNECTED) || (Obj->WifiCtx.StaState == W61_WIFI_STATE_STA_GOT_IP))
  {
    if (W61_WiFi_GetConnectInfo(Obj, &tmp_rssi) != W61_STATUS_OK)
    {
      LogError("Get connection information failed");
      return W61_STATUS_ERROR;
    }
    ApConfig->Channel = Obj->WifiCtx.STASettings.Channel;
    LogWarn("Soft-AP channel number will align on STATION's one : %ld", ApConfig->Channel);
  }
  else if ((ApConfig->Channel < 1) || (ApConfig->Channel > 13))
  {
    LogWarn("Channel value out of range, set to default value 1");
    ApConfig->Channel = 1;
  }

  if ((ApConfig->Security > W61_SECURITY_WPA2_PSK) || (ApConfig->Security == W61_SECURITY_WEP))
  {
    LogError("Security not supported");
    return ret;
  }
  else
  {
    if ((ApConfig->Security != W61_SECURITY_OPEN) &&
        ((strlen((char *)ApConfig->Password) < 8) ||
         (strlen((char *)ApConfig->Password) > W61_WIFI_MAX_PASSWORD_SIZE)))
    {
      LogError("Password length incorrect, must be in following range [8;63]");
      return ret;
    }

    /* Need to set the password to null if security selected is OPEN */
    if ((ApConfig->Security == W61_SECURITY_OPEN) && (strlen((char *)ApConfig->Password) > 0))
    {
      LogWarn("Password is not needed for open security, set to NULL");
      ApConfig->Password[0] = '\0';
    }
  }

  if (ApConfig->Hidden > 1)
  {
    LogWarn("Hidden parameter is not supported, set to default value 0");
    ApConfig->Hidden = 0;
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWSAP=\"%s\",\"%s\",%ld,%d,%ld,%ld\r\n",
             ApConfig->SSID, ApConfig->Password, ApConfig->Channel, ApConfig->Security,
             ApConfig->MaxConnections, ApConfig->Hidden);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_WIFI_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  if (ret == W61_STATUS_OK)
  {
    LogInfo("Soft-AP configuration done");
  }

  return ret;
}

W61_Status_t W61_WiFi_DeactivateAp(W61_Object_t *Obj, uint8_t Reconnect)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint32_t sta_state = 0;
  W61_NULL_ASSERT(Obj);

  /* Check if station is connected */
  if ((Obj->WifiCtx.StaState == W61_WIFI_STATE_STA_CONNECTED) || (Obj->WifiCtx.StaState == W61_WIFI_STATE_STA_GOT_IP))
  {
    sta_state = 1;
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    if (Reconnect == 0)
    {
      snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWMODE=1,0\r\n");
    }
    else
    {
      snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWMODE=1,%ld\r\n", sta_state);
    }
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_WIFI_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  if (ret == W61_STATUS_OK)
  {
    Obj->WifiCtx.ApState = W61_WIFI_STATE_AP_RESET;
  }

  return ret;
}

W61_Status_t W61_WiFi_GetApConfig(W61_Object_t *Obj, W61_ApConfig_t *ApConfig)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint32_t tmp = 0;
  int32_t result = 0;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(ApConfig);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWSAP?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_WIFI_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      result = sscanf((char *)Obj->CmdResp, "+CWSAP:\"%32[^\"]\",\"%63[^\"]\",%ld,%ld,%ld,%ld\r\n",
                      (char *)ApConfig->SSID, (char *)ApConfig->Password, &ApConfig->Channel, &tmp,
                      &ApConfig->MaxConnections, &ApConfig->Hidden);
      ApConfig->Security = (W61_SecurityType_e)tmp;
      /* Case when the soft-AP has the open security and does not have any password */
      if (result != 6)
      {
        result = sscanf((char *)Obj->CmdResp, "+CWSAP:\"%32[^\"]\",\"\",%ld,%ld,%ld,%ld\r\n",
                        (char *)ApConfig->SSID, &ApConfig->Channel, &tmp,
                        &ApConfig->MaxConnections, &ApConfig->Hidden);
        if (result != 5)
        {
          LogError("Get SoftAP configuration failed");
          ret = W61_STATUS_ERROR;
        }
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  memset(ApConfig->Password, 0, W61_WIFI_MAX_PASSWORD_SIZE + 1);
  return ret;
}

W61_Status_t W61_WiFi_ListConnectedSta(W61_Object_t *Obj, W61_Connected_Sta_t *Stations)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint8_t count = 0;  /* Number of connected stations */
  uint8_t tmp_mac[18] = {0};
  uint8_t tmp_ip[16] = {0};
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Stations);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWLIF\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_WIFI_TIMEOUT);

      while ((recv_len > 0) && (strncmp((char *)Obj->CmdResp, "+CWLIF:", sizeof("+CWLIF:") - 1) == 0) &&
             (count < W61_WIFI_MAX_CONNECTED_STATIONS))
      {
        if (sscanf((char *)Obj->CmdResp, "+CWLIF:%15[^,],%17s\r\n", tmp_ip, tmp_mac) == 2)
        {
          ParseIP((char *)tmp_ip, Stations->STA[count].IP);
          ParseMAC((char *)tmp_mac, Stations->STA[count].MAC);
          count++;
        }
        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_WIFI_TIMEOUT);
      }

      ret = AT_ParseOkErr((char *)Obj->CmdResp);

      Stations->Count = count;
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_DisconnectSta(W61_Object_t *Obj, uint8_t *MAC)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(MAC);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWQIF=\"" MACSTR "\"\r\n", MAC2STR(MAC));
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_WIFI_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_SetApIpAddress(W61_Object_t *Obj, uint8_t Ip_addr[4], uint8_t Netmask_addr[4])
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT_STR(Ip_addr, "Soft-AP IP NULL");

  if (Is_ip_valid(Ip_addr) != W61_STATUS_OK)
  {
    LogError("Soft-AP IP invalid.");
    return ret;
  }

  if (Netmask_addr == NULL)
  {
    LogWarn("Netmask IP NULL.");
    return ret;
  }

  if (Is_ip_valid(Netmask_addr) != W61_STATUS_OK)
  {
    LogWarn("Netmask IP invalid. Default one will be use : 255.255.255.0.");
    Netmask_addr[0] = 0xFF;
    Netmask_addr[1] = 0xFF;
    Netmask_addr[2] = 0xFF;
    Netmask_addr[3] = 0;
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPAP=\"" IPSTR "\",\"" IPSTR "\",\"" IPSTR "\"\r\n",
             IP2STR(Ip_addr), IP2STR(Ip_addr), IP2STR(Netmask_addr));
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_WIFI_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  if (ret == W61_STATUS_OK)
  {
    memcpy(Obj->WifiCtx.APSettings.IP_Addr, Ip_addr, W61_WIFI_SIZEOF_IPV4_BYTES);
  }
  return ret;
}

W61_Status_t W61_WiFi_GetApIpAddress(W61_Object_t *Obj)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char *ptr, *token;
  uint32_t line = 0;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPAP?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Obj->NcpTimeout);
      while ((recv_len > 0) && (strncmp((char *)Obj->CmdResp, "+CIPAP:", strlen("+CIPAP:")) == 0))
      {
        Obj->CmdResp[recv_len] = 0;
        switch (line++)
        {
          case 0:
            ptr = strstr((char *)(Obj->CmdResp), "+CIPAP:ip:");
            if (ptr == NULL)
            {
              goto _err;
            }
            ptr += sizeof("+CIPAP:ip:");

            token = strstr(ptr, "\r");
            if (token == NULL)
            {
              goto _err;
            }

            *(--token) = 0;
            ParseIP(ptr, Obj->WifiCtx.APSettings.IP_Addr);
            break;
          case 1:
            ptr = strstr((char *)(Obj->CmdResp), "+CIPAP:gateway:");
            if (ptr == NULL)
            {
              goto _err;
            }
            ptr += sizeof("+CIPAP:gateway:");

            token = strstr(ptr, "\r");
            if (token == NULL)
            {
              goto _err;
            }

            *(--token) = 0;
            break;
          case 2:
            ptr = strstr((char *)(Obj->CmdResp), "+CIPAP:netmask:");
            if (ptr == NULL)
            {
              goto _err;
            }
            ptr += sizeof("+CIPAP:netmask:");

            token = strstr(ptr, "\r");
            if (token == NULL)
            {
              goto _err;
            }
            *(--token) = 0;
            ParseIP(ptr, Obj->WifiCtx.APSettings.IP_Mask);
            break;
          default:
            break;
        }
        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Obj->NcpTimeout);
      }

      if (recv_len > 0)
      {
        Obj->CmdResp[recv_len] = 0;
        ret = AT_ParseOkErr((char *)Obj->CmdResp);
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;

_err:
  ATunlock(Obj);
  return W61_STATUS_ERROR;
}

W61_Status_t W61_WiFi_GetDhcpsConfig(W61_Object_t *Obj, uint32_t *lease_time, uint8_t start_ip[4], uint8_t end_ip[4])
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char *ptr, *token;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(lease_time);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWDHCPS?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_WIFI_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      ptr = strstr((char *)(Obj->CmdResp), "+CWDHCPS:");
      if (ptr == NULL)
      {
        goto _err;
      }

      ptr += sizeof("+CWDHCPS:") - 1;
      token = strstr(ptr, ",");
      if (token == NULL)
      {
        goto _err;
      }
      *(token) = 0;
      *lease_time = ParseNumber(ptr, NULL);

      ptr = token + 1;
      token = strstr(ptr, ",");
      if (token == NULL)
      {
        goto _err;
      }
      *(token) = 0;
      ParseIP(ptr, start_ip);

      ptr = token + 1;
      token = strstr(ptr, "\r");
      if (token == NULL)
      {
        goto _err;
      }
      *(token) = 0;
      ParseIP(ptr, end_ip);
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;

_err:
  ATunlock(Obj);
  return W61_STATUS_ERROR;
}

W61_Status_t W61_WiFi_SetDhcpsConfig(W61_Object_t *Obj, uint32_t lease_time)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint8_t StartIP[4] = {0};
  uint8_t EndIP[4] = {0};
  uint32_t Previous_lease_time = 0;
  W61_NULL_ASSERT(Obj);

  if (W61_WiFi_GetDhcpsConfig(Obj, &Previous_lease_time, StartIP, EndIP) != W61_STATUS_OK)
  {
    LogError("Get DHCP server configuration failed");
    return ret;
  }

  if (Is_ip_valid(StartIP) != W61_STATUS_OK)
  {
    LogError("Start IP NULL or invalid.");
    return ret;
  }

  if (Is_ip_valid(EndIP) != W61_STATUS_OK)
  {
    LogError("End IP NULL or invalid.");
    return ret;
  }

  if ((lease_time < 1) || (lease_time > 2880))
  {
    LogError("Lease time is invalid, range : [1;2880] minutes.");
    return ret;
  }

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CWDHCPS=1,%ld,\"" IPSTR "\",\"" IPSTR "\"\r\n",
             lease_time, IP2STR(StartIP), IP2STR(EndIP));
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_WIFI_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_GetApMacAddress(W61_Object_t *Obj, uint8_t *Mac)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char *ptr, *token;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Mac);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+CIPAPMAC?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_WIFI_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      ptr = strstr((char *)(Obj->CmdResp), "+CIPAPMAC:");
      if (ptr == NULL)
      {
        ret = W61_STATUS_ERROR;
      }
      else
      {
        ptr += sizeof("+CIPAPMAC:");
        token = strstr(ptr, "\r");
        if (token == NULL)
        {
          ret = W61_STATUS_ERROR;
        }
        else
        {
          *(--token) = 0;
          ParseMAC(ptr, Mac);
        }
      }
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_WiFi_SetDTIM(W61_Object_t *Obj, uint32_t dtim)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    if (dtim == 0)
    {
      snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+SLCLDTIM\r\n");
    }
    else
    {
      snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+SLWKDTIM=%ld\r\n", dtim);
    }

    ret = AT_SetExecute(Obj, Obj->CmdResp, Obj->NcpTimeout);
    if (ret == W61_STATUS_OK)
    {
      Obj->LowPowerCfg.WiFi_DTIM = dtim;
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_WiFi_GetDTIM(W61_Object_t *Obj, uint32_t *dtim)
{
  *dtim =  Obj->LowPowerCfg.WiFi_DTIM;
  return W61_STATUS_OK;
}

/* Private Functions Definition ----------------------------------------------*/
static void W61_WiFi_AtParseAp(char *pdata, int32_t len, W61_Scan_Result_t *APs)
{
  uint8_t num = 0;
  char buf[100] = {0};
  char *ptr;

  memcpy(buf, pdata, len);

  /* Parsing the string separated by , */
  ptr = strtok(buf, ",");
  /* Looping while the ptr reach the length of the data received or there is no new token (,) to parse (end of string)*/
  while ((ptr != NULL) && (buf + len - 3 > ptr) && (num < 10))
  {
    switch (num++)
    {
      case 0:
        ptr += 8; /* sizeof("+CWLAP:(") - 1 */
        APs->AP[APs->Count].Security = (W61_SecurityType_e)ParseNumber(ptr, NULL);
        break;
      case 1:
        /* Those two operations on ptr are used to remove the "" at the beginning and the end of the string */
        ptr++;
        ptr[strlen(ptr) - 1] = 0;
        strncpy((char *)APs->AP[APs->Count].SSID, ptr, W61_WIFI_MAX_SSID_SIZE);
        break;
      case 2:
        APs->AP[APs->Count].RSSI = ParseNumber(ptr, NULL);
        break;
      case 3:
        ptr++;
        ptr[strlen(ptr) - 1] = 0;
        ParseMAC(ptr, APs->AP[APs->Count].MAC);
        break;
      case 4:
        APs->AP[APs->Count].Channel = ParseNumber(ptr, NULL);
        break;
      /* Cases 5 and 6 are reserved for future use (hardcoded to -1) in the NCP */
      case 5:
      case 6:
        break;
      case 7:
        APs->AP[APs->Count].Group_cipher = (W61_CipherType_e)ParseNumber(ptr, NULL);
        break;
      case 8:
        APs->AP[APs->Count].WiFi_version = (W61_WiFiType_e)ParseNumber(ptr, NULL);
        break;
      case 9:
        ptr[1] = 0; /* Remove the last ) at the end of the string */
        APs->AP[APs->Count].WPS = ParseNumber(ptr, NULL);
        APs->Count++;
        continue;
    }
    /* When the pointer is pointing to the second to last parameter, the next token is the last ")" */
    if (num == 9)
    {
      ptr = strtok(NULL, ")");
    }
    else
    {
      ptr = strtok(NULL, ",");
    }
  }
}

/** @} */

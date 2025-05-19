/**
  ******************************************************************************
  * @file    w61_at_ble.c
  * @author  GPM Application Team
  * @brief   This file provides code for W61 BLE AT module
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
#include "w61_io.h" /* SPI_XFER_MTU_BYTES */
#include "common_parser.h" /* Common Parser functions */

/* Global variables ----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/** @addtogroup ST67W61_AT_BLE_Constants
  * @{
  */

#define W61_BLE_EVT_CONNECTED_KEYWORD             "+BLE:CONNECTED"        /*!< Connected event keyword */
#define W61_BLE_EVT_DISCONNECTED_KEYWORD          "+BLE:DISCONNECTED"     /*!< Disconnected event keyword */
#define W61_BLE_EVT_CONNECTION_PARAM_KEYWORD      "+BLE:CONNPARAM"        /*!< Connection parameter event keyword */
#define W61_BLE_EVT_READ_KEYWORD                  "+BLE:GATTREAD"         /*!< GATT Read event keyword */
#define W61_BLE_EVT_WRITE_KEYWORD                 "+BLE:GATTWRITE"        /*!< GATT Write event keyword */
#define W61_BLE_EVT_SERVICE_FOUND_KEYWORD         "+BLE:SRV"              /*!< Service found event keyword */
#define W61_BLE_EVT_CHAR_FOUND_KEYWORD            "+BLE:SRVCHAR"          /*!< Characteristic found event keyword */
#define W61_BLE_EVT_INDICATION_STATUS_KEYWORD     "+BLE:INDICATION"       /*!< Indication status event keyword */
#define W61_BLE_EVT_NOTIFICATION_STATUS_KEYWORD   "+BLE:NOTIFICATION"     /*!< Notification status event keyword */
#define W61_BLE_EVT_NOTIFICATION_DATA_KEYWORD     "+BLE:NOTIDATA"         /*!< Notification data event keyword */
#define W61_BLE_EVT_MTU_SIZE_KEYWORD              "+BLE:MTUSIZE"          /*!< MTU size event keyword */
#define W61_BLE_EVT_PAIRING_FAILED_KEYWORD        "+BLE:PAIRINGFAILED"    /*!< Pairing failed event keyword */
#define W61_BLE_EVT_PAIRING_COMPLETED_KEYWORD     "+BLE:PAIRINGCOMPLETED" /*!< Pairing completed event keyword */
#define W61_BLE_EVT_PAIRING_CONFIRM_KEYWORD       "+BLE:PAIRINGCONFIRM"   /*!< Pairing confirm event keyword */
#define W61_BLE_EVT_PASSKEY_ENTRY_KEYWORD         "+BLE:PASSKEYENTRY"     /*!< Passkey entry event keyword */
#define W61_BLE_EVT_PASSKEY_DISPLAY_KEYWORD       "+BLE:PASSKEYDISPLAY"   /*!< Passkey display event keyword */
#define W61_BLE_EVT_PASSKEY_CONFIRM_KEYWORD       "+BLE:PASSKEYCONFIRM"   /*!< Passkey confirm event keyword */
#define W61_BLE_EVT_BAS_LEVEL_KEYWORD             "+BLE:BASLEVEL"         /*!< Battery level event keyword */
#define W61_BLE_EVT_SCAN_KEYWORD                  "+BLE:SCAN"             /*!< BLE scan event keyword */

#define W61_BLE_CONNECT_TIMEOUT                   6000                    /*!< BLE connection timeout in ms */
/** @} */

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/** Variable to manage BLE Scan */
uint8_t ScanComplete = 0;

/** sscanf parsing error string */
static const char W61_Parsing_Error_str[] = "Parsing of the result failed";

/* Private function prototypes -----------------------------------------------*/
/** @addtogroup ST67W61_AT_BLE_Functions
  * @{
  */

/**
  * @brief  Convert UUID string to UUID array
  * @param  hexString: UUID string
  * @param  byteArray: UUID array
  * @param  byteArraySize: UUID array size
  */
static void hexStringToByteArray(const char *hexString, char *byteArray, size_t byteArraySize);

/**
  * @brief  Parse peripheral data
  * @param  pdata: peripheral data
  * @param  len: data length
  * @param  Peripherals: peripheral structure
  */
static void W61_Ble_AT_ParsePeripheral(char *pdata, int32_t len, W61_Ble_Scan_Result_t *Peripherals);

/**
  * @brief  Parse service data
  * @param  pdata: service data
  * @param  len: data length
  * @param  Service: service structure
  */
static void W61_Ble_AT_ParseService(char *pdata, int32_t len, W61_Ble_Service_t *Service);

/**
  * @brief  Parse service characteristic data
  * @param  pdata: service characteristic data
  * @param  len: data length
  * @param  Service: service structure
  */
static void W61_Ble_AT_ParseServiceCharac(char *pdata, int32_t len, W61_Ble_Service_t *Service);

/**
  * @brief  Analyze advertising data
  * @param  ptr: advertising data
  * @param  Peripherals: peripheral structure
  * @param  index: index of the peripheral to fill with adv information
  */
static void W61_Ble_AnalyzeAdvData(char *ptr, W61_Ble_Scan_Result_t *Peripherals, uint32_t index);

/* Functions Definition ------------------------------------------------------*/
void AT_Ble_Event(W61_Object_t *Obj, const uint8_t *rxbuf, int32_t rxbuf_len)
{
  char *ptr = (char *)rxbuf;
  char *token;
  W61_CbParamBleData_t cb_param_ble_data = {0};
  uint8_t conn_handle;

  UNUSED(rxbuf_len);

  if ((Obj == NULL) || (Obj->ulcbs.UL_ble_cb == NULL))
  {
    return;
  }

  if (strncmp(ptr, W61_BLE_EVT_CONNECTION_PARAM_KEYWORD, sizeof(W61_BLE_EVT_CONNECTION_PARAM_KEYWORD) - 1) == 0)
  {
    Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_CONNECTION_PARAM_ID, &cb_param_ble_data);
    return;
  }

  if (strncmp(ptr, W61_BLE_EVT_CONNECTED_KEYWORD, sizeof(W61_BLE_EVT_CONNECTED_KEYWORD) - 1) == 0)
  {
    ptr += sizeof(W61_BLE_EVT_CONNECTED_KEYWORD);
    conn_handle = ParseNumber(ptr, NULL);
    cb_param_ble_data.remote_ble_device.conn_handle = conn_handle;
    ptr += sizeof(conn_handle) + sizeof(",");
    token = strstr(ptr, "\r");
    if (token != NULL)
    {
      *(--token) = 0;
      ParseMAC(ptr, Obj->BleCtx.NetSettings.RemoteDevice[conn_handle].BDAddr);
    }
    Obj->BleCtx.NetSettings.RemoteDevice[conn_handle].IsConnected = 1;
    Obj->BleCtx.NetSettings.RemoteDevice[conn_handle].conn_handle = conn_handle;
    Obj->BleCtx.NetSettings.DeviceConnectedNb++;

    cb_param_ble_data.remote_ble_device = Obj->BleCtx.NetSettings.RemoteDevice[conn_handle];

    Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_CONNECTED_ID, &cb_param_ble_data);
    return;
  }

  if (strncmp(ptr, W61_BLE_EVT_DISCONNECTED_KEYWORD, sizeof(W61_BLE_EVT_DISCONNECTED_KEYWORD) - 1) == 0)
  {
    ptr += sizeof(W61_BLE_EVT_CONNECTED_KEYWORD);
    conn_handle = ParseNumber(ptr, NULL);
    cb_param_ble_data.remote_ble_device.conn_handle = conn_handle;
    Obj->BleCtx.NetSettings.RemoteDevice[conn_handle].IsConnected = 0;
    Obj->BleCtx.NetSettings.RemoteDevice[conn_handle].conn_handle = 0xf;
    for (int32_t i = 0; i < W61_BLE_BD_ADDR_SIZE; i++)
    {
      Obj->BleCtx.NetSettings.RemoteDevice[conn_handle].BDAddr[i] = 0x0;
    }
    Obj->BleCtx.NetSettings.DeviceConnectedNb--;
    cb_param_ble_data.remote_ble_device = Obj->BleCtx.NetSettings.RemoteDevice[conn_handle];
    Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_DISCONNECTED_ID, &cb_param_ble_data);
    return;
  }

  if (strncmp(ptr, W61_BLE_EVT_INDICATION_STATUS_KEYWORD, sizeof(W61_BLE_EVT_INDICATION_STATUS_KEYWORD) - 1) == 0)
  {
    uint32_t status = 0;
    uint32_t service_idx = 0;
    uint32_t charac_idx = 0;
    ptr += sizeof(W61_BLE_EVT_INDICATION_STATUS_KEYWORD);
    if (sscanf(ptr, "%ld,%ld,%ld", &status, &service_idx, &charac_idx) != 3)
    {
      return;
    }
    cb_param_ble_data.service_idx = service_idx;
    cb_param_ble_data.charac_idx = charac_idx;
    cb_param_ble_data.indication_status[service_idx] = status;

    if (status)
    {
      Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_INDICATION_STATUS_ENABLED_ID, &cb_param_ble_data);
    }
    else
    {
      Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_INDICATION_STATUS_DISABLED_ID, &cb_param_ble_data);
    }
    return;
  }

  if (strncmp(ptr, W61_BLE_EVT_NOTIFICATION_STATUS_KEYWORD, sizeof(W61_BLE_EVT_NOTIFICATION_STATUS_KEYWORD) - 1) == 0)
  {
    uint32_t status = 0;
    uint32_t service_idx = 0;
    uint32_t charac_idx = 0;
    ptr += sizeof(W61_BLE_EVT_NOTIFICATION_STATUS_KEYWORD);
    if (sscanf(ptr, "%ld,%ld,%ld", &status, &service_idx, &charac_idx) != 3)
    {
      return;
    }
    cb_param_ble_data.service_idx = service_idx;
    cb_param_ble_data.charac_idx = charac_idx;
    cb_param_ble_data.notification_status[service_idx] = status;

    if (status)
    {
      Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_NOTIFICATION_STATUS_ENABLED_ID, &cb_param_ble_data);
    }
    else
    {
      Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_NOTIFICATION_STATUS_DISABLED_ID, &cb_param_ble_data);
    }
    return;
  }

  if (strncmp(ptr, W61_BLE_EVT_NOTIFICATION_DATA_KEYWORD, sizeof(W61_BLE_EVT_NOTIFICATION_DATA_KEYWORD) - 1) == 0)
  {
    /* Process data */
    char *str_token;
    ptr += sizeof(W61_BLE_EVT_NOTIFICATION_DATA_KEYWORD);
    str_token = strtok(ptr, ",");
    if (str_token == NULL)
    {
      return;
    }
    cb_param_ble_data.available_data_length = ParseNumber(str_token, NULL);
    Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_NOTIFICATION_DATA_ID, &cb_param_ble_data);
    return;
  }

  if (strncmp(ptr, W61_BLE_EVT_WRITE_KEYWORD, sizeof(W61_BLE_EVT_WRITE_KEYWORD) - 1) == 0)
  {
    /* Process data */
    char *str_token;
    ptr += sizeof(W61_BLE_EVT_WRITE_KEYWORD);
    str_token = strtok(ptr, ",");
    if (str_token == NULL)
    {
      return;
    }
    cb_param_ble_data.remote_ble_device.conn_handle = ParseNumber(str_token, NULL);
    str_token = strtok(NULL, ",");
    if (str_token == NULL)
    {
      return;
    }
    cb_param_ble_data.service_idx = ParseNumber(str_token, NULL);
    str_token = strtok(NULL, ",");
    if (str_token == NULL)
    {
      return;
    }
    cb_param_ble_data.charac_idx = ParseNumber(str_token, NULL);
    str_token = strtok(NULL, ",");
    if (str_token == NULL)
    {
      return;
    }
    cb_param_ble_data.available_data_length = ParseNumber(str_token, NULL);
    if (cb_param_ble_data.available_data_length != 0)
    {
      Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_WRITE_ID, &cb_param_ble_data);
    }
    return;
  }

  if (strncmp(ptr, W61_BLE_EVT_READ_KEYWORD, sizeof(W61_BLE_EVT_READ_KEYWORD) - 1) == 0)
  {
    /* Process data */
    char *str_token;
    ptr += sizeof(W61_BLE_EVT_READ_KEYWORD);
    str_token = strtok(ptr, ",");
    if (str_token == NULL)
    {
      return;
    }
    cb_param_ble_data.remote_ble_device.conn_handle = ParseNumber(str_token, NULL);
    str_token = strtok(NULL, ",");
    if (str_token == NULL)
    {
      return;
    }
    cb_param_ble_data.service_idx = ParseNumber(str_token, NULL);
    str_token = strtok(NULL, ",");
    if (str_token == NULL)
    {
      return;
    }
    cb_param_ble_data.charac_idx = ParseNumber(str_token, NULL);
    str_token = strtok(NULL, ",");
    if (str_token == NULL)
    {
      return;
    }
    cb_param_ble_data.available_data_length = ParseNumber(str_token, NULL);
    if (cb_param_ble_data.available_data_length != 0)
    {
      Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_READ_ID, &cb_param_ble_data);
    }
    return;
  }

  if (strncmp(ptr, W61_BLE_EVT_SCAN_KEYWORD, sizeof(W61_BLE_EVT_SCAN_KEYWORD) - 1) == 0)
  {
    if ((Obj->BleCtx.ScanResults.Count < W61_BLE_MAX_DETECTED_PERIPHERAL) && (ScanComplete == 0))
    {
      W61_Ble_AT_ParsePeripheral((char *)rxbuf, rxbuf_len, &(Obj->BleCtx.ScanResults));
    }
    else
    {
      if ((Obj->ulcbs.UL_ble_cb != NULL) && (ScanComplete == 0))
      {
        ScanComplete = 1;
        Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_SCAN_DONE_ID, NULL);
      }
    }
    return;
  }

  if (strncmp(ptr, W61_BLE_EVT_CHAR_FOUND_KEYWORD, sizeof(W61_BLE_EVT_CHAR_FOUND_KEYWORD) - 1) == 0)
  {
    /* Get connection handle */
    ptr += sizeof(W61_BLE_EVT_CHAR_FOUND_KEYWORD);
    cb_param_ble_data.remote_ble_device.conn_handle = ParseNumber(ptr, NULL);

    /* Get service */
    W61_Ble_AT_ParseServiceCharac((char *)rxbuf, rxbuf_len, &cb_param_ble_data.remote_ble_device.Service);
    Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_CHAR_FOUND_ID, &cb_param_ble_data);
    return;
  }

  if (strncmp(ptr, W61_BLE_EVT_SERVICE_FOUND_KEYWORD, sizeof(W61_BLE_EVT_SERVICE_FOUND_KEYWORD) - 1) == 0)
  {
    /* Get connection handle */
    ptr += sizeof(W61_BLE_EVT_SERVICE_FOUND_KEYWORD);
    cb_param_ble_data.remote_ble_device.conn_handle = ParseNumber(ptr, NULL);

    /* Get service */
    W61_Ble_AT_ParseService((char *)rxbuf, rxbuf_len, &cb_param_ble_data.remote_ble_device.Service);
    Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_SERVICE_FOUND_ID, &cb_param_ble_data);
    return;
  }

  if (strncmp(ptr, W61_BLE_EVT_PASSKEY_CONFIRM_KEYWORD, sizeof(W61_BLE_EVT_PASSKEY_CONFIRM_KEYWORD) - 1) == 0)
  {
    /* Process data */
    char *str_token;
    ptr += sizeof(W61_BLE_EVT_PASSKEY_CONFIRM_KEYWORD);
    str_token = strstr(ptr, "PASSKEY:");
    if (str_token == NULL)
    {
      return;
    }
    str_token = str_token + sizeof("PASSKEY:") - 1;
    cb_param_ble_data.PassKey = ParseNumber(str_token, NULL);
    if (cb_param_ble_data.PassKey != 0)
    {
      Obj->ulcbs.UL_ble_cb(W61_BLE_EVT_PASSKEY_CONFIRM_ID, &cb_param_ble_data);
    }
    return;
  }
}

/* ============ AT COMMANDS implementation ====================== */
W61_Status_t W61_Ble_SetTxPower(W61_Object_t *Obj, uint32_t power)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLETXPWR=%ld\r\n", power);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_GetTxPower(W61_Object_t *Obj, uint32_t *power)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(power);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLETXPWR?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_BLE_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      if (strncmp((char *)Obj->CmdResp, "+BLETXPWR:", sizeof("+BLETXPWR:") - 1) == 0)
      {
        *power = ParseNumber((char *)Obj->CmdResp + strlen("+BLETXPWR:"), NULL);
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

W61_Status_t W61_Ble_Init(W61_Object_t *Obj, uint8_t mode, uint8_t *p_recv_data, uint32_t req_len)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  Obj->BleCtx.AppBuffRecvData = p_recv_data;
  Obj->BleCtx.AppBuffRecvDataSize = req_len;

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEINIT=%d\r\n", mode);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_GetInitMode(W61_Object_t *Obj, W61_Ble_Mode_t *Mode)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint32_t tmp = 0;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(Mode);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEINIT?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_BLE_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((const char *)Obj->CmdResp, "+BLEINIT:%ld\r\n", &tmp) != 1)
      {
        LogError("%s", W61_Parsing_Error_str);
        ret = W61_STATUS_ERROR;
      }
      else
      {
        *Mode = (W61_Ble_Mode_t)tmp;
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

W61_Status_t W61_Ble_AdvStart(W61_Object_t *Obj)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEADVSTART\r\n");
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_AdvStop(W61_Object_t *Obj)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEADVSTOP\r\n");
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_GetConnection(W61_Object_t *Obj, uint32_t *ConnectionHandle, uint8_t address[6])
{
  W61_Status_t ret = W61_STATUS_ERROR;
  int32_t recv_len;
  int32_t cmp;
  char remote_address[19];
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(ConnectionHandle);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLECONN?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_BLE_TIMEOUT);
      while ((recv_len > 0) && (strncmp((char *)Obj->CmdResp, "+BLECONN:", strlen("+BLECONN:")) == 0))
      {
        Obj->CmdResp[recv_len] = 0;
        cmp = sscanf((char *)Obj->CmdResp, "+BLECONN:%ld,\"%18[^\"]\"", ConnectionHandle, remote_address);

        if (cmp == 2)
        {
          ParseMAC(remote_address, address);
        }
        else if (cmp == 0)
        {
          *ConnectionHandle = 0;
        }

        recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_BLE_TIMEOUT);
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
}

W61_Status_t W61_Ble_Disconnect(W61_Object_t *Obj, uint32_t connection_handle)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEDISCONN=%ld\r\n",
             connection_handle);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_ExchangeMTU(W61_Object_t *Obj, uint32_t connection_handle)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEEXCHANGEMTU=%ld\r\n",
             connection_handle);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_SetBDAddress(W61_Object_t *Obj, const uint8_t *bdaddr)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(bdaddr);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEADDR=\"%02X:%02X:%02X:%02X:%02X:%02X\"\r\n",
             bdaddr[0], bdaddr[1], bdaddr[2], bdaddr[3], bdaddr[4], bdaddr[5]);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_GetBDAddress(W61_Object_t *Obj, uint8_t *BdAddr)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char *ptr, *token;
  int32_t recv_len;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(BdAddr);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEADDR?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_BLE_TIMEOUT);

      if (recv_len > 0)
      {
        ptr = strstr((char *)(Obj->CmdResp), "+BLEADDR:");
        if (ptr == NULL)
        {
          ret = W61_STATUS_ERROR;
        }
        else
        {
          ptr += sizeof("+BLEADDR:");
          token = strstr(ptr, "\r");
          if (token == NULL)
          {
            ret = W61_STATUS_ERROR;
          }
          else
          {
            *(--token) = 0;
            ParseMAC(ptr, BdAddr);
          }
          recv_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, Obj->NcpTimeout);
          if (recv_len > 0)
          {
            ret = AT_ParseOkErr((char *)Obj->CmdResp);
          }
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

W61_Status_t W61_Ble_SetDeviceName(W61_Object_t *Obj, const char *name)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(name);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLENAME=\"%s\"\r\n", name);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_GetDeviceName(W61_Object_t *Obj, char *DeviceName)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(DeviceName);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLENAME?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_BLE_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char *)Obj->CmdResp, "+BLENAME:%26[^\r\n]", DeviceName) != 1)
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

W61_Status_t W61_Ble_SetAdvData(W61_Object_t *Obj, const char *advdata)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(advdata);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEADVDATA=\"%s\"\r\n", advdata);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_SetScanRespData(W61_Object_t *Obj, const char *scanrespdata)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(scanrespdata);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLESCANRSPDATA=\"%s\"\r\n", scanrespdata);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_SetAdvParam(W61_Object_t *Obj, uint32_t adv_int_min, uint32_t adv_int_max,
                                 uint8_t adv_type, uint8_t adv_channel)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEADVPARAM=%ld,%ld,%d,%d\r\n",
             adv_int_min, adv_int_max, adv_type, adv_channel);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_Scan(W61_Object_t *Obj, uint8_t enable)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  memset((uint8_t *)&Obj->BleCtx.ScanResults, 0, sizeof(Obj->BleCtx.ScanResults));

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLESCAN=%d\r\n", enable);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    if (ret == W61_STATUS_OK && enable == 1)
    {
      ScanComplete = 0;
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_Ble_SetScanParam(W61_Object_t *Obj, uint8_t scan_type, uint8_t own_addr_type,
                                  uint8_t filter_policy, uint32_t scan_interval, uint32_t scan_window)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLESCANPARAM=%d,%d,%d,%ld,%ld\r\n",
             scan_type, own_addr_type, filter_policy, scan_interval, scan_window);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_GetScanParam(W61_Object_t *Obj, uint32_t *ScanType, uint32_t *OwnAddrType,
                                  uint32_t *FilterPolicy, uint32_t *ScanInterval, uint32_t *ScanWindow)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(ScanType);
  W61_NULL_ASSERT(OwnAddrType);
  W61_NULL_ASSERT(FilterPolicy);
  W61_NULL_ASSERT(ScanInterval);
  W61_NULL_ASSERT(ScanWindow);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLESCANPARAM?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_BLE_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char *)Obj->CmdResp, "+BLESCANPARAM:%ld,%ld,%ld,%ld,%ld\r\n",
                 ScanType, OwnAddrType, FilterPolicy, ScanInterval, ScanWindow) != 5)
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

W61_Status_t W61_Ble_GetAdvParam(W61_Object_t *Obj, uint32_t *AdvIntMin,
                                 uint32_t *AdvIntMax, uint32_t *AdvType, uint32_t *ChannelMap)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(AdvIntMin);
  W61_NULL_ASSERT(AdvIntMax);
  W61_NULL_ASSERT(AdvType);
  W61_NULL_ASSERT(ChannelMap);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEADVPARAM?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_BLE_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char *)Obj->CmdResp, "+BLEADVPARAM:%ld,%ld,%ld,%ld\r\n",
                 AdvIntMin, AdvIntMax, AdvType, ChannelMap) != 4)
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

W61_Status_t W61_Ble_SetConnParam(W61_Object_t *Obj, uint32_t conn_handle, uint32_t conn_int_min,
                                  uint32_t conn_int_max, uint32_t latency, uint32_t timeout)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLECONNPARAM=%ld,%ld,%ld,%ld,%ld\r\n",
             conn_handle, conn_int_min, conn_int_max, latency, timeout);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_GetConnParam(W61_Object_t *Obj, uint32_t *ConnHandle, uint32_t *ConnIntMin,
                                  uint32_t *ConnIntMax, uint32_t *ConnIntCurrent, uint32_t *Latency, uint32_t *Timeout)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(ConnHandle);
  W61_NULL_ASSERT(ConnIntMin);
  W61_NULL_ASSERT(ConnIntMax);
  W61_NULL_ASSERT(ConnIntCurrent);
  W61_NULL_ASSERT(Latency);
  W61_NULL_ASSERT(Timeout);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLECONNPARAM?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_BLE_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char *)Obj->CmdResp, "+BLECONNPARAM:%ld,%ld,%ld,%ld,%ld,%ld\r\n", ConnHandle, ConnIntMin, ConnIntMax,
                 ConnIntCurrent, Latency, Timeout) != 6)
      {
        LogError("%s", W61_Parsing_Error_str);
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

W61_Status_t W61_Ble_GetConn(W61_Object_t *Obj, uint32_t *ConnHandle, uint8_t *RemoteBDAddr)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char tmp_bdaddr[18];
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(ConnHandle);
  W61_NULL_ASSERT(RemoteBDAddr);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLECONN?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_BLE_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char *)Obj->CmdResp, "+BLECONN:%ld,\"%17[^\"]\"\r\n", ConnHandle, tmp_bdaddr) != 2)
      {
        LogError("%s", W61_Parsing_Error_str);
        ret = W61_STATUS_ERROR;
      }
      else
      {
        ParseMAC(tmp_bdaddr, RemoteBDAddr);
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

W61_Status_t W61_Ble_Connect(W61_Object_t *Obj, uint32_t conn_handle, uint8_t *RemoteBDAddr)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char status_buf[sizeof(AT_ERROR_STRING) - 1] = {'\0'};
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(RemoteBDAddr);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLECONN=%ld,\"%02X:%02X:%02X:%02X:%02X:%02X\"\r\n",
             conn_handle, RemoteBDAddr[0], RemoteBDAddr[1], RemoteBDAddr[2],
             RemoteBDAddr[3], RemoteBDAddr[4], RemoteBDAddr[5]);
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      /* ATrecv Timeout is increased to manage connection processing time */
      if (ATrecv(Obj, (uint8_t *)status_buf, AT_RSP_SIZE, W61_BLE_CONNECT_TIMEOUT) != 0)
      {
        ret = AT_ParseOkErr(status_buf);
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

W61_Status_t W61_Ble_SetDataLength(W61_Object_t *Obj, uint32_t conn_handle, uint32_t tx_bytes, uint32_t tx_trans_time)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEDATALEN=%ld,%ld,%ld\r\n",
             conn_handle, tx_bytes, tx_trans_time);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

/* GATT Server APIs */
W61_Status_t W61_Ble_CreateService(W61_Object_t *Obj, uint8_t service_index, const char *service_uuid)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTSSRVCRE=%d,\"%s\",1\r\n", service_index,
             service_uuid);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_DeleteService(W61_Object_t *Obj, uint8_t service_index, W61_Ble_Service_t *ServiceInfo)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(ServiceInfo);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTSSRVDEL=%d\r\n", service_index);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      /* Remove service from services table */
      ServiceInfo->service_idx = 0;
      ServiceInfo->service_type = 0;
      ServiceInfo->uuid_length = 0;
      memset(ServiceInfo->ServiceUUID, 0x0, sizeof(ServiceInfo->ServiceUUID));
      for (uint8_t index_char = 0; index_char < W61_BLE_MAX_CHAR_NBR; index_char++)
      {
        /* Remove characteristic of this service from services table */
        ServiceInfo->service_idx = 0;
        ServiceInfo->charac.char_idx = 0;
        ServiceInfo->charac.char_uuid_length = 0;
        memset(ServiceInfo->charac.CharUUID, 0x0,
               sizeof(ServiceInfo->charac.CharUUID));
        ServiceInfo->charac.char_property = 0x0;
        index_char++;
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

W61_Status_t W61_Ble_GetService(W61_Object_t *Obj, W61_Ble_Service_t *ServiceInfo)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char service_uuid_tmp[33] = {0};
  int32_t cmp;
  int32_t resp_len;
  uint8_t index = 0;
  uint32_t tmp_srv_idx = 0;
  uint32_t tmp_srv_type = 0;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTSSRV?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      resp_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_BLE_TIMEOUT);

      while (resp_len > 0)
      {
        cmp = strncmp((char *) Obj->CmdResp, "+BLEGATTSSRV:", sizeof("+BLEGATTSSRV:") - 1);
        if (cmp == 0)
        {
          memset(service_uuid_tmp, 0x0, sizeof(service_uuid_tmp));
          if (sscanf((char *)Obj->CmdResp, "+BLEGATTSSRV:%ld,\"%32[^\"]\",%ld\r\n",
                     &tmp_srv_idx, service_uuid_tmp, &tmp_srv_type) != 3)
          {
            LogError("%s", W61_Parsing_Error_str);
            ret = W61_STATUS_ERROR;
          }
          else
          {
            ServiceInfo->service_idx = tmp_srv_idx;
            ServiceInfo->service_type = tmp_srv_type;
            ServiceInfo->uuid_length = strlen(service_uuid_tmp) / 2;
            memset(ServiceInfo->ServiceUUID, 0, sizeof(ServiceInfo->ServiceUUID));
            hexStringToByteArray(service_uuid_tmp, ServiceInfo->ServiceUUID, 16);
            resp_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_BLE_TIMEOUT);
          }
        }
        else
        {
          index = tmp_srv_idx + 1;
          /* Clean Service Table */
          while (index < W61_BLE_MAX_SERVICE_NBR)
          {
            ServiceInfo->service_idx = 0;
            ServiceInfo->service_type = 0;
            ServiceInfo->uuid_length = 0;
            memset(ServiceInfo->ServiceUUID, 0x0,
                   sizeof(ServiceInfo->ServiceUUID));
            index++;
          }
          ret = AT_ParseOkErr((char *)Obj->CmdResp);
          resp_len = 0;
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

W61_Status_t W61_Ble_CreateCharacteristic(W61_Object_t *Obj, uint8_t service_index, uint8_t char_index,
                                          const char *char_uuid, uint8_t char_property, uint8_t char_permission)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTSCHARCRE=%d,%d,\"%s\",%d,%d\r\n",
             service_index, char_index, char_uuid, char_property, char_permission);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_GetCharacteristic(W61_Object_t *Obj, W61_Ble_Service_t *ServiceInfo)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  char charac_uuid_tmp[33] = {0};
  int32_t cmp;
  int32_t resp_len;
  uint32_t index_srv = 0;
  uint32_t index_char = 0;
  uint32_t tmp_srv_idx = 0;
  uint32_t tmp_char_idx = 0;
  uint32_t tmp_char_prop = 0;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTSCHAR?\r\n");
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      resp_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_BLE_TIMEOUT);

      while (resp_len > 0)
      {
        cmp = strncmp((char *) Obj->CmdResp, "+BLEGATTSCHAR:", sizeof("+BLEGATTSCHAR:") - 1);
        if (cmp == 0)
        {
          memset(charac_uuid_tmp, 0x0, sizeof(charac_uuid_tmp));
          if (sscanf((char *)Obj->CmdResp, "+BLEGATTSCHAR:%ld,%ld,\"%32[^\"]\",%ld\r\n",
                     &tmp_srv_idx, &tmp_char_idx, charac_uuid_tmp, &tmp_char_prop) != 4)
          {
            LogError("%s", W61_Parsing_Error_str);
            ret = W61_STATUS_ERROR;
          }
          else
          {
            ServiceInfo->service_idx = tmp_srv_idx;
            ServiceInfo->charac.char_idx = tmp_char_idx;
            ServiceInfo->charac.char_uuid_length = strlen(charac_uuid_tmp) / 2;
            memset(ServiceInfo->charac.CharUUID, 0,
                   sizeof(ServiceInfo->charac.CharUUID));
            hexStringToByteArray(charac_uuid_tmp, ServiceInfo->charac.CharUUID, 16);
            ServiceInfo->charac.char_property = tmp_char_prop;
            resp_len = ATrecv(Obj, Obj->CmdResp, AT_RSP_SIZE, W61_BLE_TIMEOUT);
          }
        }
        else
        {
          index_srv = tmp_srv_idx + 1;
          index_char = tmp_char_idx + 1;
          /* Clean Characteristic Table */
          while (index_srv < W61_BLE_MAX_SERVICE_NBR)
          {
            while (index_char < W61_BLE_MAX_CHAR_NBR)
            {
              ServiceInfo->service_idx = 0;
              ServiceInfo->charac.char_idx = 0;
              ServiceInfo->charac.char_uuid_length = 0;
              memset(ServiceInfo->charac.CharUUID, 0x0,
                     sizeof(ServiceInfo->charac.CharUUID));
              ServiceInfo->charac.char_property = 0x0;
              index_char++;
            }
            index_srv++;
          }
          ret = AT_ParseOkErr((char *)Obj->CmdResp);
          resp_len = 0;
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

W61_Status_t W61_Ble_RegisterCharacteristics(W61_Object_t *Obj)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTSREGISTER=1\r\n");
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  return ret;
}

W61_Status_t W61_Ble_ServerSendNotification(W61_Object_t *Obj, uint8_t service_index, uint8_t char_index,
                                            uint8_t *pdata, uint32_t req_len, uint32_t *SentLen, uint32_t Timeout)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(pdata);
  W61_NULL_ASSERT(SentLen);

  if (req_len > SPI_XFER_MTU_BYTES)
  {
    req_len = SPI_XFER_MTU_BYTES;
  }

  *SentLen = req_len;

  if (ATlock(Obj, Timeout))
  {
    /* AT_RequestSendData timeout should let the time to NCP to return SEND:ERROR message */
    if (Timeout < W61_BLE_TIMEOUT)
    {
      Timeout = W61_BLE_TIMEOUT;
    }
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTSNTFY=%d,%d,%ld\r\n",
             service_index, char_index, req_len);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Timeout);
    if (ret == W61_STATUS_OK)
    {
      ret = AT_RequestSendData(Obj, pdata, req_len, Timeout, true);
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  if (ret != W61_STATUS_OK)
  {
    *SentLen = 0;
  }
  return ret;
}

W61_Status_t W61_Ble_ServerSendIndication(W61_Object_t *Obj, uint8_t service_index, uint8_t char_index,
                                          uint8_t *pdata, uint32_t req_len, uint32_t *SentLen, uint32_t Timeout)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(pdata);
  W61_NULL_ASSERT(SentLen);

  if (req_len > SPI_XFER_MTU_BYTES)
  {
    req_len = SPI_XFER_MTU_BYTES;
  }

  *SentLen = req_len;

  if (ATlock(Obj, Timeout))
  {
    /* AT_RequestSendData timeout should let the time to NCP to return SEND:ERROR message */
    if (Timeout < W61_BLE_TIMEOUT)
    {
      Timeout = W61_BLE_TIMEOUT;
    }
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTSIND=%d,%d,%ld\r\n", service_index,
             char_index, req_len);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Timeout);
    if (ret == W61_STATUS_OK)
    {
      ret = AT_RequestSendData(Obj, pdata, req_len, Timeout, true);
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  if (ret != W61_STATUS_OK)
  {
    *SentLen = 0;
  }
  return ret;
}

W61_Status_t W61_Ble_ServerSetReadData(W61_Object_t *Obj, uint8_t service_index, uint8_t char_index, uint8_t *pdata,
                                       uint32_t req_len, uint32_t *SentLen, uint32_t Timeout)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(pdata);
  W61_NULL_ASSERT(SentLen);

  if (req_len > SPI_XFER_MTU_BYTES)
  {
    req_len = SPI_XFER_MTU_BYTES;
  }
  if (req_len > Obj->BleCtx.AppBuffRecvDataSize)
  {
    req_len = Obj->BleCtx.AppBuffRecvDataSize;
  }
  *SentLen = req_len;

  if (ATlock(Obj, Timeout))
  {
    /* AT_RequestSendData timeout should let the time to NCP to return SEND:ERROR message */
    if (Timeout < W61_BLE_TIMEOUT)
    {
      Timeout = W61_BLE_TIMEOUT;
    }
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTSRD=%d,%d,%ld\r\n", service_index,
             char_index, req_len);
    ret = AT_SetExecute(Obj, Obj->CmdResp, Timeout);
    if (ret == W61_STATUS_OK)
    {
      ret = AT_RequestSendData(Obj, pdata, req_len, Timeout, true);
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  if (ret != W61_STATUS_OK)
  {
    *SentLen = 0;
  }
  return ret;
}

/* GATT Client APIs */
W61_Status_t W61_Ble_RemoteServiceDiscovery(W61_Object_t *Obj, uint8_t connection_handle)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTCSRVDIS=%d\r\n", connection_handle);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  return ret;
}

W61_Status_t W61_Ble_RemoteCharDiscovery(W61_Object_t *Obj, uint8_t connection_handle, uint8_t service_index)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTCCHARDIS=%d,%d\r\n", connection_handle,
             service_index);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  return ret;
}

W61_Status_t W61_Ble_ClientWriteData(W61_Object_t *Obj, uint8_t conn_handle, uint8_t service_index, uint8_t char_index,
                                     uint8_t *pdata, uint32_t req_len, uint32_t *SentLen, uint32_t Timeout)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(pdata);
  W61_NULL_ASSERT(SentLen);

  if (req_len > SPI_XFER_MTU_BYTES)
  {
    req_len = SPI_XFER_MTU_BYTES;
  }

  *SentLen = req_len;

  snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTCWR=%d,%d,%d,%ld\r\n",
           conn_handle, service_index, char_index, req_len);

  if (ATlock(Obj, Timeout))
  {
    /* AT_RequestSendData timeout should let the time to NCP to return SEND:ERROR message */
    if (Timeout < W61_BLE_TIMEOUT)
    {
      Timeout = W61_BLE_TIMEOUT;
    }
    if (ATsend(Obj, Obj->CmdResp, strlen((char *)Obj->CmdResp), Obj->NcpTimeout) > 0)
    {
      ret = AT_RequestSendData(Obj, pdata, req_len, Timeout, true);
    }
    else
    {
      ret = W61_STATUS_ERROR;
    }
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }

  if (ret != W61_STATUS_OK)
  {
    *SentLen = 0;
  }
  return ret;
}

W61_Status_t W61_Ble_ClientReadData(W61_Object_t *Obj, uint8_t conn_handle, uint8_t service_index, uint8_t char_index)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTCRD=%d,%d,%d\r\n", conn_handle, service_index,
             char_index);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_Ble_ClientSubscribeChar(W61_Object_t *Obj, uint8_t conn_handle, uint8_t char_value_handle,
                                         uint8_t char_prop)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  uint8_t char_desc_handle = char_value_handle + 1;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTCSUBSCRIBE=%d,%d,%d,%d\r\n", conn_handle,
             char_desc_handle, char_value_handle, char_prop);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

W61_Status_t W61_Ble_ClientUnsubscribeChar(W61_Object_t *Obj, uint8_t conn_handle, uint8_t char_value_handle)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLEGATTCUNSUBSCRIBE=%d,%d\r\n", conn_handle,
             char_value_handle);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }
  else
  {
    ret = W61_STATUS_BUSY;
  }
  return ret;
}

/* Security APIs */
W61_Status_t W61_Ble_SetSecurityParam(W61_Object_t *Obj, uint8_t security_parameter)
{
  W61_Status_t ret = W61_STATUS_ERROR;

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLESECPARAM=%d\r\n",
             security_parameter);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }

  return ret;
}

W61_Status_t W61_Ble_GetSecurityParam(W61_Object_t *Obj, uint32_t *SecurityParameter)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);
  W61_NULL_ASSERT(SecurityParameter);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLESECPARAM?\r\n");
    ret = AT_Query(Obj, Obj->CmdResp, Obj->CmdResp, W61_BLE_TIMEOUT);
    if (ret == W61_STATUS_OK)
    {
      if (sscanf((char *)Obj->CmdResp, "+BLESECPARAM:%ld\r\n", SecurityParameter) != 1)
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
  return ret;
}

W61_Status_t W61_Ble_SecurityStart(W61_Object_t *Obj, uint8_t conn_handle, uint8_t security_level)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLESECSTART=%d,%d\r\n",
             conn_handle, security_level);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }

  return ret;
}

W61_Status_t W61_Ble_SecurityPassKeyConfirm(W61_Object_t *Obj, uint8_t conn_handle)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLESECPASSKEYCONFIRM=%d\r\n",
             conn_handle);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }

  return ret;
}

W61_Status_t W61_Ble_SecurityPairingConfirm(W61_Object_t *Obj, uint8_t conn_handle)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLESECPAIRINGCONFIRM=%d\r\n",
             conn_handle);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }

  return ret;
}

W61_Status_t W61_Ble_SecuritySetPassKey(W61_Object_t *Obj, uint8_t conn_handle, uint32_t passkey)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLESECPASSKEY=%d,%06ld\r\n",
             conn_handle, passkey);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }

  return ret;
}

W61_Status_t W61_Ble_SecurityPairingCancel(W61_Object_t *Obj, uint8_t conn_handle)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE, "AT+BLESECCANNEL=%d\r\n",
             conn_handle);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }

  return ret;
}

W61_Status_t W61_Ble_SecurityUnpair(W61_Object_t *Obj, uint8_t *RemoteBDAddr, uint32_t remote_addr_type)
{
  W61_Status_t ret = W61_STATUS_ERROR;
  W61_NULL_ASSERT(Obj);

  if (ATlock(Obj, AT_LOCK_TIMEOUT))
  {
    snprintf((char *)Obj->CmdResp, W61_ATD_CMDRSP_STRING_SIZE,
             "AT+BLESECUNPAIR=\"%02X:%02X:%02X:%02X:%02X:%02X\",%ld\r\n",
             RemoteBDAddr[0], RemoteBDAddr[1], RemoteBDAddr[2],
             RemoteBDAddr[3], RemoteBDAddr[4], RemoteBDAddr[5],
             remote_addr_type);
    ret = AT_SetExecute(Obj, Obj->CmdResp, W61_BLE_TIMEOUT);
    ATunlock(Obj);
  }

  return ret;
}
/* Private Functions Definition ----------------------------------------------*/
static void hexStringToByteArray(const char *hexString, char *byteArray, size_t byteArraySize)
{
  size_t hexStringLength = strlen(hexString);
  for (size_t i = 0; i < hexStringLength / 2 && i < byteArraySize; i++)
  {
    uint32_t byte;
    if (sscanf(hexString + 2 * i, "%02lx", &byte) != 1)
    {
      return;
    }
    byteArray[i] = byte;
  }
}

static void W61_Ble_AT_ParsePeripheral(char *pdata, int32_t len, W61_Ble_Scan_Result_t *Peripherals)
{
  int32_t rssi = 0;
  int32_t bd_addr_type = -1;
  char mac[18] = {0};
  uint8_t mac_ui8[W61_BLE_BD_ADDR_SIZE] = {0};
  char adv_data[100] = {0};
  char scan_rsp_data[100] = {0};

  int32_t ret = sscanf(pdata, "+BLE:SCAN:\"%[^\"]\",%ld,%[^,],%[^,],%ld", mac, &rssi,
                       adv_data, scan_rsp_data, &bd_addr_type);
  if (ret < 5)
  {
    /* Handle the case where scan_rsp_data is empty */
    ret = sscanf(pdata, "+BLE:SCAN:\"%[^\"]\",%ld,%[^,],,%ld", mac, &rssi, adv_data, &bd_addr_type);
    if (ret == 4)
    {
      /* Set scan_rsp_data to an empty string */
      scan_rsp_data[0] = '\0';
    }
    /* Handle the case where adv_data is empty */
    else if (ret == 2)
    {
      ret = sscanf(pdata, "+BLE:SCAN:\"%[^\"]\",%ld,,,%ld", mac, &rssi, &bd_addr_type);
      if (ret == 3)
      {
        /* Set adv_data and scan_rsp_data to an empty string */
        adv_data[0] = '\0';
        scan_rsp_data[0] = '\0';
      }
      else
      {
        /* Handle the case where adv_data is empty and scan_rsp_data filled*/
        ret = sscanf(pdata, "+BLE:SCAN:\"%[^\"]\",%ld,,%[^,],%ld", mac, &rssi, scan_rsp_data, &bd_addr_type);
        if (ret == 4)
        {
          /* Set adv_data to an empty string */
          adv_data[0] = '\0';
        }
      }
    }
  }

  if (ret == 5 || ret == 4 || ret == 3)
  {
    uint32_t index = 0;
    ParseMAC(mac, mac_ui8);

    for (; index < Peripherals->Count; index++)
    {
      if (memcmp(mac_ui8, Peripherals->Detected_Peripheral[index].BDAddr, W61_BLE_BD_ADDR_SIZE) == 0)
      {
        break;
      }
    }

    if (index < W61_BLE_MAX_DETECTED_PERIPHERAL)
    {
      /* Update data and RSSI */
      Peripherals->Detected_Peripheral[index].RSSI = rssi;
      W61_Ble_AnalyzeAdvData(adv_data, Peripherals, index);
      W61_Ble_AnalyzeAdvData(scan_rsp_data, Peripherals, index);

      if (index == Peripherals->Count)
      {
        /* New peripheral detected, fill BD address and address type */
        ParseMAC(mac, Peripherals->Detected_Peripheral[index].BDAddr);
        Peripherals->Detected_Peripheral[index].bd_addr_type = bd_addr_type;
        Peripherals->Count++;
      }
    }
  }
}

static void W61_Ble_AT_ParseService(char *pdata, int32_t len, W61_Ble_Service_t *Service)
{
  uint8_t num = 0;
  uint8_t service_idx = 0;
  char buf[101] = {0};
  char tmp_uuid[33] = {0};
  char *ptr;
  uint32_t j = 0;

  memcpy(buf, pdata, len);

  /* Parsing the string separated by , */
  ptr = strtok(buf, ",");
  /* Looping while the ptr reach the length of the data received or there is no new token (,) to parse (end of string)*/
  while ((ptr != NULL) && (buf + len - 4 > ptr) && (num < 4))
  {
    switch (num++)
    {
      case 0:
        /* Connection handle */
        ptr = strtok(NULL, ",");
        break;
      case 1:
        /* Service Index */
        service_idx  = ParseNumber(ptr, NULL);
        Service->service_idx = service_idx;
        ptr = strtok(NULL, ",");
        break;
      case 2:
        /* UUID */
        for (int32_t i = 0; (i < 32); i++)
        {
          if (ptr[j] == 0x00) /* end of UUID */
          {
            ptr[i] = 0x00;
          }
          else
          {
            if (ptr[j] == '-')
            {
              j++;
            }
            tmp_uuid[i] = ptr[j];
            j++;
          }
        }

        if (j == 4) /* Short UUID */
        {
          Service->uuid_length = 4;
        }
        else /* Long UUID */
        {
          Service->uuid_length = 32;
        }
        memset(Service->ServiceUUID, 0, sizeof(Service->ServiceUUID));
        hexStringToByteArray(tmp_uuid, Service->ServiceUUID, W61_BLE_MAX_UUID_SIZE);
        ptr = strtok(NULL, ",");
        break;

      case 3:
        /* Service type */
        Service->service_type = ParseNumber(ptr, NULL);
        break;
    }
  }
}

static void W61_Ble_AT_ParseServiceCharac(char *pdata, int32_t len, W61_Ble_Service_t *Service)
{
  uint8_t num = 0;
  uint8_t service_idx = 0;
  uint8_t charac_idx = 0;
  char buf[101] = {0};
  char tmp_uuid[33] = {0};
  char *ptr;
  uint32_t j = 0;

  memcpy(buf, pdata, len);

  /* Parsing the string separated by , */
  ptr = strtok(buf, ",");
  /* Looping while the ptr reach the length of the data received or there is no new token (,) to parse (end of string)*/
  while ((ptr != NULL) && (buf + len - 3 > ptr) && (num < 7))
  {
    switch (num++)
    {
      case 0:
        /* Connection handle */
        ptr = strtok(NULL, ",");
        break;
      case 1:
        /* Service Index */
        service_idx  = ParseNumber(ptr, NULL);
        Service->service_idx = service_idx;
        ptr = strtok(NULL, ",");
        break;
      case 2:
        /* Characteristic Index */
        charac_idx  = ParseNumber(ptr, NULL);
        Service->charac.char_idx = charac_idx;
        ptr = strtok(NULL, ",");
        break;
      case 3:
        /* UUID */
        for (int32_t i = 0; (i < 32); i++)
        {
          if (ptr[j] == 0x00) /* end of UUID */
          {
            ptr[i] = 0x00;
          }
          else
          {
            if (ptr[j] == '-')
            {
              j++;
            }
            tmp_uuid[i] = ptr[j];
            j++;
          }
        }

        if (j == 4) /* Short UUID */
        {
          Service->charac.char_uuid_length = 4;
        }
        else /* Long UUID */
        {
          Service->charac.char_uuid_length = 32;
        }
        memset(Service->charac.CharUUID, 0, sizeof(Service->charac.CharUUID));
        hexStringToByteArray(tmp_uuid, Service->charac.CharUUID, W61_BLE_MAX_UUID_SIZE);
        ptr = strtok(NULL, ",");
        break;

      case 4:
        /* Characteristic property */
        ptr += 2; /* sizeof("0x") - 1 */
        Service->charac.char_property = ParseHexNumber(ptr, NULL);
        ptr = strtok(NULL, ",");
        break;
      case 5:
        /* Characteristic handle */
        Service->charac.char_handle = ParseNumber(ptr, NULL);
        ptr = strtok(NULL, ",");
        break;
      case 6:
        /* Characteristic value handle */
        Service->charac.char_value_handle = ParseNumber(ptr, NULL);
        break;
    }
  }
}

static void W61_Ble_AnalyzeAdvData(char *ptr, W61_Ble_Scan_Result_t *Peripherals, uint32_t index)
{
  uint32_t adv_data_size;
  uint32_t adv_data_flag;
  uint32_t end_of_data_packet;
  char *p_adv_data;
  uint16_t i = 0;
  if (ptr != NULL)
  {
    p_adv_data = (char *)ptr;
    end_of_data_packet = *(p_adv_data + 2);

    /* First byte is data length */
    if (sscanf(p_adv_data, "%2lx", &adv_data_size) != 1)
    {
      return;
    }

    while (end_of_data_packet != 0)
    {
      if ((sscanf(p_adv_data + 2 * i, "%2lx", &adv_data_size) != 1) ||
          (sscanf(p_adv_data  + 2 * i + 2, "%2lx", &adv_data_flag) != 1))
      {
        return;
      }

      switch (adv_data_flag)
      {
        case W61_BLE_AD_TYPE_FLAGS:
          break;
        case W61_BLE_AD_TYPE_MANUFACTURER_SPECIFIC_DATA:
        {
          hexStringToByteArray(p_adv_data + 2 * i + 4,
                               (char *) Peripherals->Detected_Peripheral[index].ManufacturerData,
                               adv_data_size - 1);
          break;
        }
        case W61_BLE_AD_TYPE_COMPLETE_LOCAL_NAME:
        {
          hexStringToByteArray(p_adv_data + 2 * i + 4,
                               (char *) Peripherals->Detected_Peripheral[index].DeviceName,
                               adv_data_size - 1);
          break;
        }
        default:
          break;
      } /* end of switch */

      i = i + adv_data_size + 1;

      /* check enf of decoded data packet */
      end_of_data_packet = *(p_adv_data + 2 * i);
    }
  }
}

/** @} */

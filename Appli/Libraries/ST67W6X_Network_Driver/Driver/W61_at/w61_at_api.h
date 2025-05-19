/**
  ******************************************************************************
  * @file    w61_at_api.h
  * @author  GPM Application Team
  * @brief   This file provides the API definitions of the W61 AT driver
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef W61_AT_API_H
#define W61_AT_API_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "semphr.h"
#include "message_buffer.h"
#include "w61_driver_config.h"

/** @defgroup ST67W61_AT ST67W61 AT Driver
  */

/** @defgroup ST67W61_AT_System ST67W61 AT Driver System
  * @ingroup  ST67W61_AT
  */

/** @defgroup ST67W61_AT_WiFi ST67W61 AT Driver Wi-Fi
  * @ingroup  ST67W61_AT
  */

/** @defgroup ST67W61_AT_Net ST67W61 AT Driver Net
  * @ingroup  ST67W61_AT
  */

/** @defgroup ST67W61_AT_HTTP ST67W61 AT Driver HTTP
  * @ingroup  ST67W61_AT
  */

/** @defgroup ST67W61_AT_MQTT ST67W61 AT Driver MQTT
  * @ingroup  ST67W61_AT
  */

/** @defgroup ST67W61_AT_BLE ST67W61 AT Driver BLE
  * @ingroup  ST67W61_AT
  */

/* Exported constants --------------------------------------------------------*/
/* ===================================================================== */
/** @defgroup ST67W61_AT_System_Constants ST67W61 AT Driver System Constants
  * @ingroup  ST67W61_AT_System
  * @{
  */
/* ===================================================================== */
/**
  * @brief  W61 Status enumeration
  */
typedef enum
{
  W61_STATUS_OK             = 0,    /*!< Operation successful */
  W61_STATUS_BUSY           = 1,    /*!< Operation not done: driver in use by another task */
  W61_STATUS_ERROR          = 2,    /*!< Operation failed */
  W61_STATUS_TIMEOUT        = 3,    /*!< Operation timeout */
  W61_STATUS_IO_ERROR       = 4,    /*!< IO error */
} W61_Status_t;

#define W61_SYS_FS_FILENAME_SIZE      32 /*!< Maximum size of the filename */

#define W61_SYS_FS_MAX_FILES          20 /*!< Maximum number of files */

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_WiFi_Constants ST67W61 AT Driver Wi-Fi Constants
  * @ingroup  ST67W61_AT_WiFi
  * @{
  */
/* ===================================================================== */
/* W61_event_id_t */
#define W61_WIFI_EVT_SCAN_DONE_ID                      1  /*!< event_args parameter to be casted to W61_Scan_Result_t */
#define W61_WIFI_EVT_CONNECTED_ID                      2  /*!< no param */
#define W61_WIFI_EVT_DISCONNECTED_ID                   3  /*!< no param */
#define W61_WIFI_EVT_GOT_IP_ID                         4  /*!< no param */
#define W61_WIFI_EVT_CONNECTING_ID                     5  /*!< no param */
#define W61_WIFI_EVT_REASON_ID                         6  /*!< no param */

#define W61_WIFI_EVT_STA_CONNECTED_ID                  10 /*!< event_args parameter to be casted to MAC ??? */
#define W61_WIFI_EVT_STA_DISCONNECTED_ID               11 /*!< event_args parameter to be casted to MAC ??? */
#define W61_WIFI_EVT_DIST_STA_IP_ID                    12 /*!< event_args parameter to be casted to MAC+IP ??? */

#define W61_WIFI_MAX_SSID_SIZE                         32 /*!< Maximum size of the SSID */

#define W61_WIFI_MAX_PASSWORD_SIZE                     63 /*!< Maximum size of the password */

#define W61_WIFI_MAX_HOSTNAME_SIZE                     32 /*!< Maximum size of the hostname */

#define W61_WIFI_SIZEOF_IPV4_BYTES                     4  /*!< Size of an IPv4 address in bytes */

#define W61_WIFI_MAX_CONNECTED_STATIONS                10 /*!< Maximum number of connected stations */

/**
  * @brief  IPv4/IPv6 layer version
  */
typedef enum
{
  W61_IPV4 = 0x00,                  /*!< IPv4 */
  W61_IPV6 = 0x01,                  /*!< IPv6 */
} W61_IPVer_e;

/**
  * @brief  Wi-Fi scan type
  */
typedef enum
{
  WIFI_SCAN_ACTIVE = 0,                     /*!< Active scan */
  WIFI_SCAN_PASSIVE = 1                     /*!< Passive scan */
} scan_type_e;

/**
  * @brief  Wi-Fi security encryption method
  */
typedef enum
{
  W61_SECURITY_OPEN = 0x00,         /*!< Wi-Fi is open */
  W61_SECURITY_WEP = 0x01,          /*!< Wired Equivalent Privacy option for Wi-Fi security. */
  W61_SECURITY_WPA_PSK = 0x02,      /*!< Wi-Fi Protected Access */
  W61_SECURITY_WPA2_PSK = 0x03,     /*!< Wi-Fi Protected Access 2 */
  W61_SECURITY_WPA_WPA2_PSK = 0x04, /*!< Wi-Fi Protected Access with both modes */
  W61_SECURITY_WPA_ENT = 0x05,      /*!< Wi-Fi Protected Access */
  W61_SECURITY_WPA3_SAE = 0x06,     /*!< Wi-Fi Protected Access 2 */
  W61_SECURITY_WPA2_WPA3_SAE = 0x07,/*!< Wi-Fi Protected Access with both modes */
  W61_SECURITY_UNKNOWN = 0x08,      /*!< Other modes */
} W61_SecurityType_e;

/**
  * @brief  Wi-Fi Connection status
  */
typedef enum
{
  W61_WIFI_STATE_STA_NO_STARTED_CONNECTION  = 0,  /*!< No connection started */
  W61_WIFI_STATE_STA_CONNECTED              = 1,  /*!< Connection established */
  W61_WIFI_STATE_STA_GOT_IP                 = 2,  /*!< IP address received */
  W61_WIFI_STATE_STA_CONNECTING             = 3,  /*!< Connection in progress */
  W61_WIFI_STATE_STA_DISCONNECTED           = 4,  /*!< Connection lost */
  W61_WIFI_STATE_STA_OFF                    = 5,  /*!< Wi-Fi is off */
} W61_StaStateType_e;

/**
  * @brief  Wi-Fi Access Point status
  */
typedef enum
{
  W61_WIFI_STATE_AP_RESET                   = 0,  /*!< Soft Access Point reset */
  W61_WIFI_STATE_AP_RUNNING                 = 1,  /*!< Soft Access Point running */
  W61_WIFI_STATE_AP_OFF                     = 2,  /*!< Soft Access Point off */
} W61_ApStateType_e;

/**
  * @brief  Wi-Fi DHCP Client type enumeration
  */
typedef enum
{
  W61_WIFI_DHCP_DISABLED               = 0,  /*!< DHCP client disabled */
  W61_WIFI_DHCP_STA_ENABLED            = 1,  /*!< DHCP client enabled for the station */
  W61_WIFI_DHCP_AP_ENABLED             = 2,  /*!< DHCP client enabled for the AP */
  W61_WIFI_DHCP_STA_AP_ENABLED         = 3,  /*!< DHCP client enabled for both station and AP */
} W61_DhcpType_e;

/**
  * @brief  Wi-Fi Connection pairwise cipher type enumeration
  */
typedef enum
{
  WIFI_EVENT_BEACON_IND_CIPHER_NONE     = 0,        /*!< None */
  WIFI_EVENT_BEACON_IND_CIPHER_WEP      = 1,        /*!< WEP40 */
  WIFI_EVENT_BEACON_IND_CIPHER_AES      = 4,        /*!< AES/CCMP */
  WIFI_EVENT_BEACON_IND_CIPHER_TKIP     = 3,        /*!< TKIP */
  WIFI_EVENT_BEACON_IND_CIPHER_TKIP_AES = 5,        /*!< TKIP and AES/CCMP */
  WIFI_EVENT_BEACON_IND_UNKNOW          = 7,        /*!< Unknown */
} W61_CipherType_e;

/**
  * @brief  Wi-Fi mode bitmask enumeration
  */
typedef enum
{
  WIFI_MODE_802_11B       = 0x01,                   /*!< 802.11 b */
  WIFI_MODE_802_11A       = 0x02,                   /*!< 802.11 a */
  WIFI_MODE_802_11G       = 0x04,                   /*!< 802.11 g */
  WIFI_MODE_802_11N_2_4   = 0x08,                   /*!< 802.11n at 2.4GHz */
  WIFI_MODE_802_11N_5     = 0x10,                   /*!< 802.11n at 5GHz */
  WIFI_MODE_802_11AC_5    = 0x20,                   /*!< 802.11ac at 5GHz */
  WIFI_MODE_802_11AC_2_4  = 0x40,                   /*!< 802.11ac at 2.4GHz */
  WIFI_MODE_802_11AX_2_4  = 0x80,                   /*!< 802.11ax at 2.4GHz */
} W61_WiFiType_e;

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_Net_Constants ST67W61 AT Driver Net Constants
  * @ingroup  ST67W61_AT_Net
  * @{
  */
/* ===================================================================== */
/* W61_event_id_t */
#define W61_NET_EVT_SOCK_DATA_ID                       50 /*!< event_args parameter to be casted to W61_CbParamNetData_t */
#define W61_NET_EVT_SOCK_CONNECTED_ID                  51 /*!< event_args parameter to be casted to W61_CbParamNetData_t */
#define W61_NET_EVT_SOCK_DISCONNECTED_ID               52 /*!< event_args parameter to be casted to W61_CbParamNetData_t */

#define W61_NET_MAX_CONNECTIONS                 5     /*!< Maximum number of Network connections */

/**
  * @brief  Net server type
  */
typedef enum
{
  W61_TCP_CONNECTION        = 0,            /*!< TCP connection */
  W61_UDP_CONNECTION        = 1,            /*!< UDP connection */
  W61_UDP_LITE_CONNECTION   = 2,            /*!< UDP Lite connection */
  W61_TCP_SSL_CONNECTION    = 3,            /*!< TCP SSL connection */
} W61_ConnType_e;

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_MQTT_Constants ST67W61 AT Driver MQTT Constants
  * @ingroup  ST67W61_AT_MQTT
  * @{
  */
/* ===================================================================== */
/* W61_event_id_t */
#define W61_MQTT_EVT_CONNECTED_ID                      60 /*!< no param */
#define W61_MQTT_EVT_DISCONNECTED_ID                   61 /*!< no param */
#define W61_MQTT_EVT_SUBSCRIPTION_RECEIVED_ID          62 /*!< no param */

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_BLE_Constants ST67W61 AT Driver BLE Constants
  * @ingroup  ST67W61_AT_BLE
  * @{
  */
/* ===================================================================== */
/* W61_event_id_t */
/* #define W61_BLE_EVT_SCAN_ID                          20 */
#define W61_BLE_EVT_CONNECTED_ID                       21 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_DISCONNECTED_ID                    22 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_CONNECTION_PARAM_ID                23 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_READ_ID                            24 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_WRITE_ID                           25 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_SERVICE_FOUND_ID                   26 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_CHAR_FOUND_ID                      27 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_INDICATION_STATUS_ENABLED_ID       28 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_INDICATION_STATUS_DISABLED_ID      29 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_NOTIFICATION_STATUS_ENABLED_ID     30 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_NOTIFICATION_STATUS_DISABLED_ID    31 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_NOTIFICATION_DATA_ID               32 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_MTU_SIZE_ID                        33 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_PAIRING_FAILED_ID                  34 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_PAIRING_COMPLETED_ID               35 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_PAIRING_CONFIRM_ID                 36 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_PASSKEY_ENTRY_ID                   37 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_PASSKEY_DISPLAY_ID                 38 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_PASSKEY_CONFIRM_ID                 39 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_BAS_LEVEL_ID                       40 /*!< event_args parameter to be casted to W61_CbParamBleData_t */
#define W61_BLE_EVT_SCAN_DONE_ID                       41 /*!< event_args parameter to be casted to W61_CbParamBleData_t */

#define W61_BLE_BD_ADDR_SIZE                           6  /*!< BLE BD Address size */
#define W61_BLE_DEVICE_NAME_SIZE                       26 /*!< BLE Device Name size */
#define W61_BLE_MANUF_DATA_SIZE                        30 /*!< BLE Maximum Manufacturer Data size */

/* AD types for advertising data and scan response data */
#define W61_BLE_AD_TYPE_FLAGS                          0x01U  /*!< Advertising type Flags */
#define W61_BLE_AD_TYPE_SHORTENED_LOCAL_NAME           0x08U  /*!< Advertising type Shortened Local Name */
#define W61_BLE_AD_TYPE_COMPLETE_LOCAL_NAME            0x09U  /*!< Advertising type Complete Local Name */
#define W61_BLE_AD_TYPE_MANUFACTURER_SPECIFIC_DATA     0xFFU  /*!< Advertising type Manufacturer Specific Data */

/**
  * @brief  BLE mode type
  */
typedef enum
{
  W61_BLE_MODE_CLIENT = 1,
  W61_BLE_MODE_SERVER = 2
} W61_Ble_Mode_t;

/**
  * @brief  BLE Advertising type enumeration
  */
typedef enum
{
  W61_BLE_ADV_TYPE_IND = 0,
  W61_BLE_ADV_TYPE_SCAN_IND = 1,
  W61_BLE_ADV_TYPE_NONCONN_IND = 2
} W61_Ble_AdvType_t;

/**
  * @brief  BLE Advertising channel enumeration
  */
typedef enum
{
  W61_BLE_ADV_CHANNEL_37  = 1,
  W61_BLE_ADV_CHANNEL_38  = 2,
  W61_BLE_ADV_CHANNEL_39  = 4,
  W61_BLE_ADV_CHANNEL_ALL = 7
} W61_Ble_AdvChannel_t;

/**
  * @brief  BLE Scan type enumeration
  */
typedef enum
{
  W61_BLE_SCAN_PASSIVE = 0,
  W61_BLE_SCAN_ACTIVE = 1
} W61_Ble_ScanType_t;

/**
  * @brief  BLE address type
  */
typedef enum
{
  W61_BLE_PUBLIC_ADDR = 0,
  W61_BLE_RANDOM_ADDR = 1,
  W61_BLE_RPA_PUBLIC_ADDR = 2,
  W61_BLE_RPA_RANDOM_ADDR = 3
} W61_Ble_AddrType_t;

/**
  * @brief  BLE address type
  */
typedef enum
{
  W61_BLE_SCAN_FILTER_ALLOW_ALL = 0,
  W61_BLE_SCAN_FILTER_ALLOW_ONLY_WLST = 1,
  W61_BLE_SCAN_FILTER_ALLOW_UND_RPA_DIR = 2,
  W61_BLE_SCAN_FILTER_ALLOW_WLIST_PRA_DIR = 3
} W61_Ble_FilterPolicy_t;
/** @} */

/* Exported types ------------------------------------------------------------*/
/* ===================================================================== */
/** @defgroup ST67W61_AT_System_Types ST67W61 AT Driver System Types
  * @ingroup  ST67W61_AT_System
  * @{
  */
/* ===================================================================== */

/**
  * @brief  IO Init Bus callback
  */
typedef int32_t (*IO_Init_Func)(void);

/**
  * @brief  IO DeInit Bus callback
  */
typedef int32_t (*IO_DeInit_Func)(void);

/**
  * @brief  IO Delay Bus callback
  */
typedef void (*IO_Delay_Func)(uint32_t);

/**
  * @brief  IO Send Bus callback
  */
typedef int32_t (*IO_Send_Func)(uint8_t *, uint16_t len, uint32_t timeout);

/**
  * @brief  IO Receive Bus callback
  */
typedef int32_t (*IO_Receive_Func)(uint8_t *, uint16_t len, uint32_t timeout);

/**
  * @brief  IO Wait Ready Bus callback
  */
typedef int32_t (*IO_WaitReady_Func)(void);

/**
  * @brief  W61 events type
  */
typedef  uint8_t W61_event_id_t;

/**
  * @brief  callback for Wi-Fi station events. the type of event_args depends of the event_id:\
  *        - W61_Scan_Result_t for W61_WIFI_EVT_SCAN_DONE_ID
  *        - NULL for [W61_WIFI_EVT_CONNECTED_ID, W61_WIFI_EVT_DISCONNECTED_ID, \
  *                    W61_WIFI_EVT_GOT_IP_ID]
  */
typedef void (*UpperLayer_wifi_sta_Cb)(W61_event_id_t event_id, void *event_args);

/**
  * @brief  Callback for Wi-Fi access point events. the type of event_args depends of the event_id:\
  *         - .. for [W61_WIFI_EVT_STA_CONNECTED_ID, W61_WIFI_EVT_STA_DISCONNECTED_ID, \
  *                   W61_WIFI_EVT_DIST_STA_IP_ID]
  */
typedef void (*UpperLayer_wifi_ap_Cb)(W61_event_id_t event_id, void *event_args);

/**
  * @brief  Callback for Net events. the type of event_args depends of the event_id:
  *         - W61_CbParamNetData_t for all existing events
  */
typedef void (*UpperLayer_net_Cb)(W61_event_id_t event_id, void *event_args);

/**
  * @brief  Callback for MQTT events. the type of event_args depends of the event_id: None
  */
typedef void (*UpperLayer_mqtt_Cb)(W61_event_id_t event_id, void *event_args);

/**
  * @brief  Callback for BLE events. the type of event_args depends of the event_id:\
  *         - W61_CbParamBleData_t for all existing events
  */
typedef void (*UpperLayer_ble_Cb)(W61_event_id_t event_id, void *event_args);

/**
  * @brief  Upper Layer Callbacks
  */
typedef struct
{
  UpperLayer_wifi_sta_Cb   UL_wifi_sta_cb;    /*!< Callback for Wi-Fi station events */
  UpperLayer_wifi_ap_Cb    UL_wifi_ap_cb;     /*!< Callback for Wi-Fi access point events */
  UpperLayer_net_Cb        UL_net_cb;         /*!< Callback for Net events */
  UpperLayer_mqtt_Cb       UL_mqtt_cb;        /*!< Callback for MQTT events */
  UpperLayer_ble_Cb        UL_ble_cb;         /*!< Callback for BLE events */
} W61_UL_Cb_t;

/**
  * @brief  IO Bus functions
  */
typedef struct
{
  IO_Init_Func       IO_Init;                 /*!< IO_Init Bus function */
  IO_DeInit_Func     IO_DeInit;               /*!< IO_DeInit Bus function */
  IO_Delay_Func      IO_Delay;                /*!< IO_Delay Bus function */
  IO_Send_Func       IO_Send;                 /*!< IO_Send Bus function */
  IO_Receive_Func    IO_Receive;              /*!< IO_Receive Bus function */
} W61_IO_t;

/**
  * @brief  W61 module information
  */
typedef struct
{
  uint8_t AT_Version[32];                     /*!< AT version string */
  uint8_t SDK_Version[32];                    /*!< SDK version string */
  uint8_t MAC_Version[32];                    /*!< MAC version string */
  uint8_t Build_Date[32];                     /*!< Build date string */
  uint32_t BatteryVoltage;                    /*!< Battery voltage */
  int8_t trim_wifi_hp[14];                    /*!< Wi-Fi high power trim */
  int8_t trim_wifi_lp[14];                    /*!< Wi-Fi low power trim */
  int8_t trim_ble[5];                         /*!< BLE trim */
  int8_t trim_xtal;                           /*!< XTAL Frequency compensation */
  uint8_t Mac_Address[6];                     /*!< Customer MAC address */
  uint8_t AntiRollbackBootloader;             /*!< AntiRollback Bootloader counter */
  uint8_t AntiRollbackApp;                    /*!< AntiRollback Application counter */
  char ModuleID[25];                          /*!< Module name */
  uint16_t BomID;                             /*!< BOM ID */
  uint8_t Manufacturing_Week;                 /*!< Manufacturing week */
  uint8_t Manufacturing_Year;                 /*!< Manufacturing year */
} W61_ModuleInfo_t;

/**
  * @brief  File System list structure
  */
typedef struct
{
  char filename[W61_SYS_FS_MAX_FILES][W61_SYS_FS_FILENAME_SIZE];  /*!< List of filenames */
  uint32_t nb_files;                                              /*!< Number of files */
} W61_FS_FilesList_t;

/**
  * @brief  Low Power configuration
  */
typedef struct
{
  uint32_t WakeUpPinIn;                       /*!< NCP wake up pin */
  uint32_t PSAutoEnable;                      /*!< Enable NCP LowPower Entry */
  uint32_t PSMode;                            /*!< Power Save Mode of the NCP */
  uint32_t WiFi_DTIM;                         /*!< DTIM of the NCP */
} W61_LowPowerCfg_t;

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_WiFi_Types ST67W61 AT Driver Wi-Fi Types
  * @ingroup  ST67W61_AT_WiFi
  * @{
  */
/* ===================================================================== */

/**
  * @brief  Wi-Fi Callback parameter for network data
  */
typedef struct
{
  uint8_t IP[4];                              /*!< IP address of the station disconnected from the soft Access Point */
  uint8_t MAC[6];                             /*!< MAC address of the station disconnected from the soft Access Point */
} W61_CbParamWiFiData_t;

/**
  * @brief  Wi-Fi Network settings
  */
typedef struct
{
  uint8_t            SSID[W61_WIFI_MAX_SSID_SIZE + 1];  /*!< Service Set Identifier value. Wi-Fi Access Point name */
  W61_SecurityType_e Security;                          /*!< Security of Wi-Fi Access Point */
  W61_IPVer_e        IP_Ver;                            /*!< IP layer version */
  uint8_t            IP_Addr[4];                        /*!< IP Address */
  uint8_t            IP_Mask[4];                        /*!< Netmask Address */
  uint8_t            Gateway_Addr[4];                   /*!< Gateway IP Address */
  uint32_t           DNS_isset;                         /*!< Flag to indicate if DNS is set */
  uint8_t            DNS1[4];                           /*!< IP Address of First DNS */
  uint8_t            DNS2[4];                           /*!< IP Address of Second DNS */
  uint8_t            DNS3[4];                           /*!< IP Address of Third DNS */
} W61_Network_t;

/**
  * @brief  Wi-Fi Station settings
  */
typedef struct
{
  uint32_t          ReconnRetries;            /*!< Number of reconnection retries */
  uint32_t          ReconnInterval;           /*!< Interval between reconnection retries */
  uint32_t          Channel;                  /*!< Channel number */
} W61_STASettings_t;

/**
  * @brief  Wi-Fi Access Point settings
  */
typedef struct
{
  uint8_t SSID[W61_WIFI_MAX_SSID_SIZE + 1];   /*!< Service Set Identifier value. Wi-Fi Access Point name */
  uint8_t IP_Addr[4];                         /*!< Wi-Fi IP Address */
  uint8_t IP_Mask[4];                         /*!< Wi-Fi Netmask Address */
  uint8_t MAC_Addr[6];                        /*!< MAC address */
} W61_APSettings_t;

/**
  * @brief  Wi-Fi Device configuration
  */
typedef struct
{
  uint8_t          DHCP_STA_IsEnabled;        /*!< Flag to indicate if DHCP client is enabled for the station */
  uint8_t          DHCP_AP_IsEnabled;         /*!< Flag to indicate if DHCP client is enabled for the AP */
  uint8_t          AutoConnect;               /*!< Flag to indicate if auto connection is enabled */
} W61_DeviceConfig_t;

/**
  * @brief  Wi-Fi Beacon information structure from the scan result
  */
typedef struct
{
  uint8_t SSID[W61_WIFI_MAX_SSID_SIZE + 1];   /*!< Service Set Identifier value. Wi-Fi Access Point name */
  W61_SecurityType_e Security;                /*!< Security encryption of the Wi-Fi Access Point */
  int16_t RSSI;                               /*!< Signal strength of Wi-Fi Access Point */
  uint8_t MAC[6];                             /*!< MAC address of Wi-Fi Access Point */
  uint8_t Channel;                            /*!< Wi-Fi channel */
  W61_CipherType_e Group_cipher;              /*!< Group cipher value */
  W61_WiFiType_e WiFi_version;                /*!< Wi-Fi revision supported */
  uint8_t WPS;                                /*!< Is WPS enabled */
} W61_AP_t;

/**
  * @brief  Wi-Fi Scan result
  */
typedef struct
{
  uint32_t Count;                             /*!< Number of detected APs */
  W61_AP_t AP[W61_WIFI_MAX_DETECTED_AP];      /*!< List of detected APs */
} W61_Scan_Result_t;

/**
  * @brief  Wi-Fi scan result callback
  */
typedef void(* W61_scan_result_cb_t)(int32_t status, W61_Scan_Result_t *Scan_results);

/**
  * @brief  Wi-Fi connection options
  */
typedef struct
{
  /** Service Set Identifier value. Wi-Fi Access Point name */
  uint8_t SSID[W61_WIFI_MAX_SSID_SIZE + 1];
  /** Password of Wi-Fi Access Point */
  uint8_t Password[W61_WIFI_MAX_PASSWORD_SIZE + 1];
  /** MAC address of Wi-Fi Access Point */
  uint8_t MAC[6];
  /** Interval between Wi-Fi connection retries. Unit: second. Default: 0. Maximum: 7200 */
  uint16_t Reconnection_interval;
  /** Number of attempts the device makes to reconnect to the AP. Default 0 (always try), Max = 1000 */
  uint16_t Reconnection_nb_attempts;
} W61_Connect_Opts_t;

/**
  * @brief  Wi-Fi internal context
  */
typedef struct
{
  W61_SecurityType_e    Security;             /*!< Security encryption method of the Access Point */
  /** Several field of this structure might be removed (to be studied) */
  W61_Network_t         NetSettings;          /*!< Network settings */
  W61_STASettings_t     STASettings;          /*!< Station settings */
  W61_APSettings_t      APSettings;           /*!< Access Point settings */
  W61_DeviceConfig_t    DeviceConfig;         /*!< Device configuration */
  W61_scan_result_cb_t  scan_done_cb;         /*!< Callback for scan done */
  int32_t               scan_status;          /*!< Status of the scan */
  W61_Scan_Result_t     ScanResults;          /*!< Scan results */
  W61_StaStateType_e    StaState;             /*!< Station state */
  W61_ApStateType_e     ApState;              /*!< Access Point state */
} W61_Wifi_Ctx_t;

/**
  * @brief  Wi-Fi scan options
  */
typedef struct
{
  uint8_t SSID[W61_WIFI_MAX_SSID_SIZE + 1]; /*!< Service Set Identifier value. Wi-Fi Access Point name */
  uint8_t MAC[6];                           /*!< MAC address of Wi-Fi Access Point */
  scan_type_e scan_type;                    /*!< Scan type */
  uint8_t Channel;                          /*!< Wi-Fi channel */
  uint8_t MaxCnt;                           /*!< Max number of APs to return */
} W61_Scan_Opts_t;

/**
  * @brief  Wi-Fi Access point configuration
  */
typedef struct
{
  uint8_t SSID[W61_WIFI_MAX_SSID_SIZE + 1];           /*!< Service Set Identifier value. Wi-Fi Access Point name */
  uint8_t Password[W61_WIFI_MAX_PASSWORD_SIZE + 1];   /*!< Password of Wi-Fi Access Point */
  W61_SecurityType_e Security;                        /*!< Security encryption of the Wi-Fi Access Point */
  uint32_t Channel;                                    /*!< Channel Wi-Fi is operating at */
  uint32_t MaxConnections;                             /*!< Max number of stations that SoftAP will allow to connect. Range [1,10] */
  uint32_t Hidden;                                     /*!< Flag to indicate if the SSID is hidden. 0: broadcasting SSID, 1: hidden SSID */
} W61_ApConfig_t;

/**
  * @brief  Wi-Fi connected station information
  */
typedef struct
{
  uint8_t IP[4];                                          /*!< IP address of connected station */
  uint8_t MAC[6];                                         /*!< MAC address of connected station */
} W61_Connected_Sta_Info_t;

/**
  * @brief  Wi-Fi connected station structure
  */
typedef struct
{
  uint32_t Count;                                             /*!< Number of connected stations */
  W61_Connected_Sta_Info_t STA[W61_WIFI_MAX_CONNECTED_STATIONS];   /*!< Array of connected stations */
} W61_Connected_Sta_t;

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_Net_Types ST67W61 AT Driver Net Types
  * @ingroup  ST67W61_AT_Net
  * @{
  */
/* ===================================================================== */

/**
  * @brief  Callback parameter for network data
  */
typedef struct
{
  uint32_t socket_id;               /*!< Socket ID */
  uint32_t available_data_length;   /*!< Available data length */
  uint8_t  remote_ip[4];            /*!< Remote IP address */
  uint16_t remote_port;             /*!< Remote port */
} W61_CbParamNetData_t;

/**
  * @brief  Network connection configuration
  */
typedef struct
{
  W61_ConnType_e    Type;                             /*!< Connection type */
  uint8_t           Number;                           /*!< Connection number */
  uint16_t          RemotePort;                       /*!< Remote port */
  uint16_t          LocalPort;                        /*!< Local port */
  uint8_t           RemoteIP[4];                      /*!< Remote IP address */
  char              *Name;                            /*!< Connection name */
  uint16_t          KeepAlive;                        /*!< Keep Alive */
} W61_Conn_t;

/**
  * @brief  Net internal context
  */
typedef struct
{
  int32_t               AppBuffRecvDataSize;  /*!< Size of the buffer to receive data */
  uint8_t               *AppBuffRecvData;     /*!< Buffer to receive data */
} W61_Net_Ctx_t;

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_HTTP_Types ST67W61 AT Driver HTTP Types
  * @ingroup  ST67W61_AT_HTTP
  * @{
  */
/* ===================================================================== */

/**
  * @brief  HTTP internal context
  */
typedef struct
{
  int32_t               AppBuffRecvDataSize;  /*!< Size of the buffer to receive data */
  uint8_t               *AppBuffRecvData;     /*!< Buffer to receive data */
} W61_HTTP_Ctx_t;

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_MQTT_Types ST67W61 AT Driver MQTT Types
  * @ingroup  ST67W61_AT_MQTT
  * @{
  */
/* ===================================================================== */

/**
  * @brief  Callback parameter for MQTT data
  */
typedef struct
{
  uint32_t topic_length;            /*!< Length of the topic */
  uint32_t message_length;          /*!< Length of the message */
} W61_CbParamMqttData_t;

/**
  * @brief  MQTT internal context
  */
typedef struct
{
  int32_t               AppBuffRecvDataSize;  /*!< Size of the buffer to receive data */
  uint8_t               *AppBuffRecvData;     /*!< Buffer to receive data */
} W61_MQTT_Ctx_t;

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_BLE_Types ST67W61 AT Driver BLE Types
  * @ingroup  ST67W61_AT_BLE
  * @{
  */
/* ===================================================================== */
/**
  * @brief  BLE characteristic structure
  */
typedef struct
{
  uint8_t char_idx;                     /*!< Characteristic index */
  uint32_t char_uuid_length;            /*!< Characteristic UUID length */
  char CharUUID[W61_BLE_MAX_UUID_SIZE]; /*!< Characteristic UUID */
  uint8_t char_property;                /*!< Characteristic Property */
  uint8_t char_handle;                  /*!< Characteristic handle */
  uint8_t char_value_handle;            /*!< Characteristic value handle */
} W61_Ble_Characteristic_t;

/**
  * @brief  BLE service structure
  */
typedef struct
{
  uint8_t service_idx;                      /*!< Service index */
  uint8_t service_type;                     /*!< Service type */
  uint32_t uuid_length;                     /*!< Service UUID length */
  char ServiceUUID[W61_BLE_MAX_UUID_SIZE];  /*!< Service UUID */
  W61_Ble_Characteristic_t charac;          /*!< Service characteristic */
} W61_Ble_Service_t;

/**
  * @brief  BLE remote device structure
  */
typedef struct
{
  int16_t RSSI;                                       /*!< Signal strength of BLE connection */
  uint8_t IsConnected;                                /*!< Connection status */
  uint8_t conn_handle;                                /*!< Connection handle */
  uint8_t DeviceName[W61_BLE_DEVICE_NAME_SIZE];       /*!< BLE device name */
  uint8_t ManufacturerData[W61_BLE_MANUF_DATA_SIZE];  /*!< Device manufacturer data */
  uint8_t BDAddr[W61_BLE_BD_ADDR_SIZE];               /*!< BD address of BLE Device */
  uint8_t bd_addr_type;                               /*!< Type of BD address */
  W61_Ble_Service_t Service;                          /*!< BLE service */
} W61_Ble_Device_t;

/**
  * @brief  BLE scanned device structure
  */
typedef struct
{
  int16_t RSSI;                                        /*!< Signal strength of BLE connection */
  uint8_t DeviceName[W61_BLE_DEVICE_NAME_SIZE];        /*!< BLE device name */
  uint8_t ManufacturerData[W61_BLE_MANUF_DATA_SIZE];   /*!< Device manufacturer data */
  uint8_t BDAddr[W61_BLE_BD_ADDR_SIZE];                /*!< BD address of BLE Device */
  uint8_t bd_addr_type;                                /*!< Type of BD address */
} W61_Ble_Scanned_Device_t;

/**
  * @brief  Callback parameter for BLE data
  */
typedef struct
{
  uint8_t service_idx;                /*!< Service index */
  uint8_t charac_idx;                 /*!< Characteristic index */
  uint8_t notification_status[2];     /*!< Notification status */
  uint8_t indication_status[2];       /*!< Indication status */
  uint32_t PassKey;                   /*!< BLE Security passkey */
  uint32_t available_data_length;     /*!< Available data length */
  W61_Ble_Device_t remote_ble_device; /*!< BLE Remote device */
} W61_CbParamBleData_t;

/**
  * @brief  BLE network settings
  */
typedef struct
{
  uint8_t          DeviceConnectedNb;                   /*!< Connection status */
  W61_Ble_Device_t RemoteDevice[W61_BLE_MAX_CONN_NBR];  /*!< BLE remote device in network */
  uint8_t          BLE_Mask[4];                         /*!< BLE mask */
} W61_Ble_Network_t;

/**
  * @brief  Scan results structure
  */
typedef struct
{
  uint32_t Count;                                   /*!< Number of BLE peripherals detected */
  W61_Ble_Scanned_Device_t Detected_Peripheral[W61_BLE_MAX_DETECTED_PERIPHERAL];  /*!< Array of BLE peripherals information. */
} W61_Ble_Scan_Result_t;

/**
  * @brief  BLE scan result callback
  */
typedef void(* W61_ble_scan_result_cb_t)(W61_Ble_Scan_Result_t *Ble_Scan_results);

/**
  * @brief  BLE internal context
  */
typedef struct
{
  W61_Ble_Network_t             NetSettings;          /*!< Network settings */
  W61_ble_scan_result_cb_t      scan_done_cb;         /*!< Callback for scan done */
  W61_Ble_Scan_Result_t         ScanResults;          /*!< Scan results */
  int32_t                       AppBuffRecvDataSize;  /*!< Size of the buffer to receive data */
  uint8_t                       *AppBuffRecvData;     /*!< Buffer to receive data */
} W61_Ble_Ctx_t;

/**
  * @brief  BLE write data
  */
typedef struct
{
  uint8_t conn_handle;                      /*!< Connection handle */
  uint8_t service_idx;                      /*!< Service index */
  uint8_t charac_idx;                       /*!< Characteristic index */
  uint8_t data_len;                         /*!< Data length */
  uint8_t data[16];                         /*!< Data */
} W61_BLEWriteData_t;

/**
  * @brief  BLE scan options structure
  */
typedef struct
{
  W61_Ble_ScanType_t scan_type;                    /*!< Scan type (1: Active, 0: Passive) */
  W61_Ble_AddrType_t own_addr_type;                /*!< BLE Address type */
  W61_Ble_FilterPolicy_t filter_policy;            /*!< BLE Scan filter policy */
  /** BLE Scan interval. The scan interval equals this parameter multiplied by 0.625 ms.
    * Should be more than or equal to scan_window. */
  uint16_t scan_interval;
  /** BLE Scan window. The scan window equals this parameter multiplied by 0.625 ms.
    * Should be less than or equal to scan_interval.*/
  uint16_t scan_window;
} W61_Ble_Scan_Opts_t;

/**
  * @brief  BLE Connection options structure
  */
typedef struct
{
  W61_Ble_AddrType_t remote_addr_type;              /*!< BLE Address type */
  uint8_t BDAddr[W61_BLE_BD_ADDR_SIZE];             /*!< BD address of BLE Device */
  uint32_t timeout;                                 /*!< Connection timeout (ms) */
} W61_Ble_Connect_Opts_t;

/** @} */

/* ===================================================================== */
/** @addtogroup ST67W61_AT_System_Types
  * @{
  */
/* ===================================================================== */

/**
  * @brief  W61 Object structure
  */
typedef struct
{
  W61_ModuleInfo_t      ModuleInfo;                  /*!< Module information */
  W61_UL_Cb_t           ulcbs;                       /*!< Upper Layer Callbacks */
  W61_IO_t              fops;                        /*!< IO Bus functions */
  TaskHandle_t          ATD_RxPooling_task_handle;   /*!< AT Rx parser task handle (Responses and Events) */
  uint8_t               *ATD_RecvBuf;                /*!< Rx buffer for AT responses and events */
  W61_LowPowerCfg_t     LowPowerCfg;                 /*!< Low Power configuration */
  /* Task and buffer specific to Command/Responses */
  MessageBufferHandle_t ATD_Resp_xMessageBuffer;     /*!< Stream buffer handle for AT responses to commands */
  uint8_t               *CmdResp;                    /*!< Buffer for AT commands formatting and Rx Responses */
  /* Task and buffers specific to async Events */
  TaskHandle_t          ATD_EvtPooling_task_handle;  /*!< AT Event parser task handle (dedicated Events) */
  MessageBufferHandle_t ATD_Evt_xMessageBuffer;      /*!< Stream buffer handle for AT events */
  uint8_t               *EventsBuf;                  /*!< Buffer for AT received events */
  uint32_t               NcpTimeout;                 /*!< NCP timeout for AT msg which don't need wireless exchange */
  /**
    * Semaphore to protect the access to the AT command request buffer.
    * Note: should NOT be used by RxPoolingTask.
    */
  SemaphoreHandle_t     xCmdMutex;
  W61_Ble_Ctx_t         BleCtx;                     /*!< BLE context */
  W61_Wifi_Ctx_t        WifiCtx;                    /*!< Wi-Fi context */
  W61_Net_Ctx_t         NetCtx;                     /*!< Net context */
  W61_HTTP_Ctx_t        HTTPCtx;                    /*!< HTTP context */
  W61_MQTT_Ctx_t        MQTTCtx;                    /*!< MQTT context */
} W61_Object_t;

/** @} */

/* Exported variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
/* ===================================================================== */
/** @defgroup ST67W61_AT_System_Functions ST67W61 AT Driver System Functions
  * @ingroup  ST67W61_AT_System
  * @{
  */
/* ===================================================================== */

/**
  * @brief  Get the W61 object
  * @return W61_Object_t
  */
W61_Object_t *W61_ObjGet(void);

/**
  * @brief  Register the upper layer callbacks
  * @param  Obj: pointer to module handle
  * @param  UL_wifi_sta_cb: callback for Wi-Fi station events
  * @param  UL_wifi_ap_cb: callback for Wi-Fi access point events
  * @param  UL_net_cb: callback for Net events
  * @param  UL_mqtt_cb: callback for MQTT events
  * @param  UL_ble_cb: callback for BLE events
  * @return Operation Status.
  */
W61_Status_t W61_RegisterULcb(W61_Object_t *Obj,
                              UpperLayer_wifi_sta_Cb   UL_wifi_sta_cb,
                              UpperLayer_wifi_ap_Cb    UL_wifi_ap_cb,
                              UpperLayer_net_Cb        UL_net_cb,
                              UpperLayer_mqtt_Cb       UL_mqtt_cb,
                              UpperLayer_ble_Cb        UL_ble_cb);

/**
  * @brief  Register the IO functions
  * @param  Obj: pointer to module handle
  * @param  IO_Init: pointer to IO_Init function
  * @param  IO_DeInit: pointer to IO_DeInit function
  * @param  IO_Delay: pointer to IO_Delay function
  * @param  IO_Send: pointer to IO_Send function
  * @param  IO_Receive: pointer to IO_Receive function
  * @return Operation Status.
  */
W61_Status_t W61_RegisterBusIO(W61_Object_t *Obj,
                               IO_Init_Func IO_Init,
                               IO_DeInit_Func  IO_DeInit,
                               IO_Delay_Func   IO_Delay,
                               IO_Send_Func    IO_Send,
                               IO_Receive_Func  IO_Receive);

/**
  * @brief  Initialize WIFI module.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_Init(W61_Object_t *Obj);

/**
  * @brief  Initialize WIFI module.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_DeInit(W61_Object_t *Obj);

/**
  * @brief  Wait for the module to be ready and responsive.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_WaitReady(W61_Object_t *Obj);

/**
  * @brief  Change default NCP Timeout: response time when no wireless communication required
  * @param  Obj: pointer to module handle
  * @param  Timeout: Timeout in mS
  * @return Operation Status.
  */
W61_Status_t W61_SetTimeout(W61_Object_t *Obj, uint32_t Timeout);

/**
  * @brief  Reset To factory defaults.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_ResetToFactoryDefault(W61_Object_t *Obj);

/**
  * @brief  Get current remaining heap size in NCP
  * @param  Obj: pointer to module handle
  * @param  RemainingHeap: current remaining heap size.
  * @return Operation Status.
  */
W61_Status_t W61_GetNcpHeapState(W61_Object_t *Obj, uint32_t *RemainingHeap);

/**
  * @brief  Get the parameter store mode
  * @param  Obj: pointer to module handle
  * @param  mode: store mode
  * @return Operation Status.
  */
W61_Status_t W61_GetStoreMode(W61_Object_t *Obj, uint32_t *mode);

/**
  * @brief  Set the parameter store mode
  * @param  Obj: pointer to module handle
  * @param  mode: store mode
  * @return Operation Status.
  */
W61_Status_t W61_SetStoreMode(W61_Object_t *Obj, uint32_t mode);

/**
  * @brief  Read the EFUSE content
  * @param  Obj: pointer to module handle
  * @param  addr: address of EFUSE to read
  * @param  nbytes: number of EFUSE bytes to read
  * @param  data: pointer to output data
  * @retval Operation Status.
  */
W61_Status_t W61_ReadEFuse(W61_Object_t *Obj, uint32_t addr, uint32_t nbytes, uint8_t *data);

/**
  * @brief  Reset the module.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_ResetModule(W61_Object_t *Obj);

/**
  * @brief  Get the module information.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_GetModuleInfo(W61_Object_t *Obj);

/**
  * @brief  Delete the file entry in the NCP file system
  * @param  Obj: pointer to module handle
  * @param  filename: pointer to the file name
  * @return Operation Status.
  */
W61_Status_t W61_FS_DeleteFile(W61_Object_t *Obj, char *filename);

/**
  * @brief  Create a file entry in the NCP file system
  * @param  Obj: pointer to module handle
  * @param  filename: pointer to the file name
  * @return Operation Status.
  */
W61_Status_t W61_FS_CreateFile(W61_Object_t *Obj, char *filename);

/**
  * @brief  Write data to a file in the NCP file system
  * @param  Obj: pointer to module handle
  * @param  filename: pointer to the file name
  * @param  offset: offset in the file
  * @param  data: pointer to the data to write
  * @param  len: length of the data to write
  * @return Operation Status.
  */
W61_Status_t W61_FS_WriteFile(W61_Object_t *Obj, char *filename, uint32_t offset, uint8_t *data, uint32_t len);

/**
  * @brief  Read data from a file in the NCP file system
  * @param  Obj: pointer to module handle
  * @param  filename: pointer to the file name
  * @param  offset: offset in the file
  * @param  data: pointer to the data to read
  * @param  len: length of the data to read
  * @return Operation Status.
  */
W61_Status_t W61_FS_ReadFile(W61_Object_t *Obj, char *filename, uint32_t offset, uint8_t *data, uint32_t len);

/**
  * @brief  Get the size of a file in the NCP file system
  * @param  Obj: pointer to module handle
  * @param  filename: pointer to the file name
  * @param  size: pointer to the file size
  * @return Operation Status.
  */
W61_Status_t W61_FS_GetSizeFile(W61_Object_t *Obj, char *filename, uint32_t *size);

/**
  * @brief  List all files in the NCP file system
  * @param  Obj: pointer to module handle
  * @param  files_list: pointer to the file list structure
  * @return Operation Status.
  */
W61_Status_t W61_FS_ListFiles(W61_Object_t *Obj, W61_FS_FilesList_t *files_list);

/**
  * @brief  Send AT command to start the OTA process
  * @param  Obj: pointer to module handle
  * @param  enable: 0 Terminate the OTA transmission. 1 Start the OTA transmission.
  * @return Operation Status.
  */
W61_Status_t W61_OTA_starts(W61_Object_t *Obj, uint32_t enable);

/**
  * @brief  Send AT command to stop OTA process and reboot the module using the new firmware
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_OTA_Finish(W61_Object_t *Obj);

/**
  * @brief  Send AT command to send OTA data buf of size len
  * @param  Obj: pointer to module handle
  * @param  buff: pointer to the OTA data to send
  * @param  len : length of the OTA data to send
  * @return Operation Status.
  */
W61_Status_t W61_OTA_Send(W61_Object_t *Obj, uint8_t *buff, uint32_t len);

/**
  * @brief  Get the module information.
  * @param  Obj: pointer to module handle
  * @param  WakeUpPinIn: NCP wake up pin
  * @param  ps_mode: Mode of the NCP
  * @return Operation Status.
  */
W61_Status_t W61_LowPowerConfig(W61_Object_t *Obj, uint32_t WakeUpPinIn, uint32_t ps_mode);

/**
  * @brief  Configure Power mode
  * @param  Obj: pointer to module handle
  * @param  ps_mode: ps_mode mode to set (0: normal mode, 1: hibernate mode, 2: standby mode)
  * @param  hbn_level: hibernate level to set wakeup source (0: wakeup pin, 1: RTC alarm or wakeup pin, 2 : HBN only)
  * @return Operation Status.
  */
W61_Status_t W61_SetPowerMode(W61_Object_t *Obj, uint32_t ps_mode, uint32_t hbn_level);

/**
  * @brief  Configure Power mode
  * @param  Obj: pointer to module handle
  * @param  ps_mode: ps_mode mode to set (0: normal mode, 1: hibernate mode, 2: standby mode)
  * @return Operation Status.
  */
W61_Status_t W61_GetPowerMode(W61_Object_t *Obj, uint32_t *ps_mode);
/**
  * @brief  Set Low Power mode.
  * @param  Obj: pointer to module handle
  * @param  enable: Enable Low Power mode
  * @return Operation Status.
 */
W61_Status_t W61_SetPowerSaveAuto(W61_Object_t *Obj, uint32_t enable);

/**
  * @brief  Get Low Power mode.
  * @param  Obj: pointer to module handle
  * @param  enable: Enable Low Power mode
  * @return PSAutoEnable
 */
W61_Status_t W61_GetPowerSaveAuto(W61_Object_t *Obj, uint32_t *enable);

/**
  * @brief  Enable Low Power mode Entry.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
 */
W61_Status_t W61_LowPowerEnterUnderLock(W61_Object_t *Obj);

/**
  * @brief  Set NCP Wake-up pin of the st67x611M.
  * @param  Obj: pointer to module handle
  * @param  wakeup_pin: pin number of the wake up input
  * @return Operation Status.
  */
W61_Status_t W61_SetWakeUpPin(W61_Object_t *Obj, uint32_t wakeup_pin);

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_WiFi_Functions ST67W61 AT Driver Wi-Fi Functions
  * @ingroup  ST67W61_AT_WiFi
  * @{
  */
/* ===================================================================== */

/**
  * @brief  Chooses if auto connect should be On or Off.
  * @param  Obj: pointer to module handle
  * @param  OnOff: If set to 1 WIFI module will connect to last SSID used to connect
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_SetAutoConnect(W61_Object_t *Obj, uint32_t OnOff);

/**
  * @brief  Return the auto connect status : On or Off.
  * @param  Obj: pointer to module handle
  * @param  OnOff: If set to 1 WIFI module will connect to last SSID used to connect
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_GetAutoConnect(W61_Object_t *Obj, uint32_t *OnOff);

/**
  * @brief  Set the host name of the station
  * @param  Obj: pointer to module handle
  * @param  Hostname: string containing the host name
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_SetHostname(W61_Object_t *Obj, uint8_t Hostname[33]);

/**
  * @brief  Return the host name.
  * @param  Obj: pointer to module handle
  * @param  Hostname: string containing the host name
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_GetHostname(W61_Object_t *Obj, uint8_t Hostname[33]);

/**
  * @brief  Activate STA mode.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_ActivateSta(W61_Object_t *Obj);

/**
  * @brief  List all detected APs.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_Scan(W61_Object_t *Obj);

/**
  * @brief  Set scan options APs.
  * @param  Obj: pointer to module handle
  * @param  ScanOpts : Pointer to scan options structure.
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_SetScanOpts(W61_Object_t *Obj, W61_Scan_Opts_t *ScanOpts);

/**
  * @brief  Set the reconnection options.
  * @param  Obj: pointer to module handle
  * @param  ConnectOpts : Pointer to connection options structure.
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_SetReconnectionOpts(W61_Object_t *Obj, W61_Connect_Opts_t *ConnectOpts);

/**
  * @brief  Join an Access point.
  * @param  Obj: pointer to module handle
  * @param  ConnectOpts : Pointer to connection options structure.
  * @param  Timeout: Timeout in ms.
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_Connect(W61_Object_t *Obj, W61_Connect_Opts_t *ConnectOpts, uint32_t Timeout);

/**
  * @brief  return the information of the Access point station is connected.
  * @param  Obj: pointer to module handle
  * @param  Rssi : Pointer to RSSI value.
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_GetConnectInfo(W61_Object_t *Obj, int32_t *Rssi);

/**
  * @brief  Disconnect from a network.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_Disconnect(W61_Object_t *Obj);

/**
  * @brief  Get the MAC address of the station.
  * @param  Obj: pointer to module handle
  * @param  Mac: pointer to the MAC address array.
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_GetStaMacAddress(W61_Object_t *Obj, uint8_t *Mac);

/**
  * @brief  Get the IP address of the station.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_GetStaIpAddress(W61_Object_t *Obj);

/**
  * @brief  Set the IP address of the module.
  * @param  Obj: pointer to module handle
  * @param  Ip_addr: pointer to the IP address array.
  * @param  Gateway_addr: pointer to the IP address array.
  * @param  Netmask_addr: pointer to the IP address array.
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_SetStaIpAddress(W61_Object_t *Obj, uint8_t Ip_addr[4], uint8_t Gateway_addr[4],
                                      uint8_t Netmask_addr[4]);

/**
  * @brief  return the State of the station,
  *         if connected and/or got ip, return also the ssid of the AP.
  * @param  Obj: pointer to module handle
  * @param  State: pointer to the state enum.
  * @param  Ssid: pointer to the ssid string.
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_GetStaState(W61_Object_t *Obj, W61_StaStateType_e *State, char *Ssid);

/**
  * @brief  return the DHCP config.
  * @param  Obj: pointer to module handle
  * @param  State: pointer to the Dhcp client enum.
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_GetDhcpConfig(W61_Object_t *Obj, W61_DhcpType_e *State);

/**
  * @brief  Set the DHCP global configuration.
  * @param  Obj: pointer to module handle
  * @param  State: pointer to the dhcp client enum.
  * @param  Operate: pointer defining to enable / disable dhcp client.
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_SetDhcpConfig(W61_Object_t *Obj, W61_DhcpType_e *State, uint32_t *Operate);

/**
  * @brief  return the country code information.
  * @param  Obj: pointer to module handle
  * @param  Policy: value to specify if the country code align on AP's one
  * @param  CountryString: pointer to Country code string
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_GetCountryCode(W61_Object_t *Obj, uint32_t *Policy, char *CountryString);

/**
  * @brief  set the country code information.
  * @param  Obj: pointer to module handle
  * @param  Policy: value to specify if the country code align on AP's one
  * @param  CountryString: pointer to Country code string
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_SetCountryCode(W61_Object_t *Obj, uint32_t *Policy, char *CountryString);

/**
  * @brief  return the DNS information.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_GetDnsAddress(W61_Object_t *Obj);

/**
  * @brief  Set the DNS IP address of the module.
  * @param  Obj: pointer to module handle
  * @param  State: pointer to retrieve the state of the DNS (enable/disable)
  * @param  Dns1_addr: pointer to the DNS1 address array.
  * @param  Dns2_addr: pointer to the DNS2 address array.
  * @param  Dns3_addr: pointer to the DNS3 address array.
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_SetDnsAddress(W61_Object_t *Obj, uint32_t *State, uint8_t Dns1_addr[4], uint8_t Dns2_addr[4],
                                    uint8_t Dns3_addr[4]);

/**
  * @brief  Set the Wi-Fi mode to Station mode.
  * @param  Obj: pointer to module handle
  * @return W61_Status_t
  */
W61_Status_t W61_WiFi_SetDualMode(W61_Object_t *Obj);

/**
  * @brief  Configure and activate SoftAP.
  * @param  Obj: pointer to module handle
  * @param  ApConfig : Pointer to AP config structure.
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_ActivateAp(W61_Object_t *Obj, W61_ApConfig_t *ApConfig);

/**
  * @brief  Deactivate the SoftAP.
  * @param  Obj: pointer to module handle
  * @param  Reconnect: 0: Do not reconnect, 1: check if station were previously connected and reconnect
  * @return W61_Status_t
  */
W61_Status_t W61_WiFi_DeactivateAp(W61_Object_t *Obj, uint8_t Reconnect);

/**
  * @brief  Get the SoftAP configuration.
  * @param  Obj: pointer to module handle
  * @param  ApConfig: pointer to the AP configuration structure
  * @return W61_Status_t
  */
W61_Status_t W61_WiFi_GetApConfig(W61_Object_t *Obj, W61_ApConfig_t *ApConfig);

/**
  * @brief  List all connected stations.
  * @param  Obj: pointer to module handle
  * @param  Sta: pointer to the connected stations structure
  * @return W61_Status_t
  */
W61_Status_t W61_WiFi_ListConnectedSta(W61_Object_t *Obj, W61_Connected_Sta_t *Sta);

/**
  * @brief  Disconnect a station from the AP.
  * @param  Obj: pointer to module handle
  * @param  MAC: pointer to the MAC address of the station to disconnect
  * @return W61_Status_t
  */
W61_Status_t W61_WiFi_DisconnectSta(W61_Object_t *Obj, uint8_t *MAC);

/**
  * @brief  Set the IP address of the SoftAP.
  * @param  Obj: pointer to module handle
  * @param  Ip_addr: pointer to the IP address array
  * @param  Netmask_addr: pointer to the Netmask address array
  * @return W61_Status_t
  */
W61_Status_t W61_WiFi_SetApIpAddress(W61_Object_t *Obj, uint8_t Ip_addr[4], uint8_t Netmask_addr[4]);

/**
  * @brief  Get the IP address of the SoftAP.
  * @param  Obj: pointer to module handle
  * @return W61_Status_t
  */
W61_Status_t W61_WiFi_GetApIpAddress(W61_Object_t *Obj);

/**
  * @brief  Get the DHCP server configuration.
  * @param  Obj: pointer to module handle
  * @param  lease_time: lease time
  * @param  start_ip: pointer to the start IP address
  * @param  end_ip: pointer to the end IP address
  * @return W61_Status_t
  */
W61_Status_t W61_WiFi_GetDhcpsConfig(W61_Object_t *Obj, uint32_t *lease_time, uint8_t start_ip[4], uint8_t end_ip[4]);

/**
  * @brief  Set the DHCP server configuration.
  * @param  Obj: pointer to module handle
  * @param  lease_time: lease time
  * @return W61_Status_t
  */
W61_Status_t W61_WiFi_SetDhcpsConfig(W61_Object_t *Obj, uint32_t lease_time);

/**
  * @brief  Get the MAC address of the soft-AP.
  * @param  Obj: pointer to module handle
  * @param  Mac: pointer to the MAC address array.
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_GetApMacAddress(W61_Object_t *Obj, uint8_t *Mac);

/**
  * @brief  Set DTIM period
  * @param  Obj: pointer to module handle
  * @param  dtim: the dtim period
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_SetDTIM(W61_Object_t *Obj, uint32_t dtim);

/**
  * @brief  Set DTIM period
  * @param  Obj: pointer to module handle
  * @param  dtim: the dtim period
  * @return Operation Status.
  */
W61_Status_t W61_WiFi_GetDTIM(W61_Object_t *Obj, uint32_t *dtim);
/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_Net_Functions ST67W61 AT Driver Net Functions
  * @ingroup  ST67W61_AT_Net
  * @{
  */
/* ===================================================================== */
/**
  * @brief  Initialize the Net default configuration.
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_Net_Init(W61_Object_t *Obj);

/** @brief  Ping a remote host.
  * @param  Obj: pointer to module handle
  * @param  location: pointer to the remote host address
  * @param  length: length of the remote host address
  * @param  count: number of ping requests to send
  * @param  interval: time interval between each ping request : [100, 3500] (in ms)
  * @param  average_time: pointer to the average time measured
  * @param  received_response: pointer to the status of the ping request
  * @return Operation Status.
  */
W61_Status_t W61_Net_Ping(W61_Object_t *Obj, char *location, uint16_t length, uint16_t count, uint16_t interval,
                          uint32_t *average_time, uint16_t *received_response);

/**
  * @brief  DNS Lookup to get IP address .
  * @param  Obj: pointer to module handle
  * @param  url: Domain Name.
  * @param  ipaddress: IP address.
  * @return Operation Status.
  */
W61_Status_t W61_Net_DNS_LookUp(W61_Object_t *Obj, const char *url, uint8_t *ipaddress);

/**
  * @brief  Configure and Start a Client connection.
  * @param  Obj: pointer to module handle
  * @param  conn: pointer to the connection structure
  * @return Operation Status.
  */
W61_Status_t W61_Net_StartClientConnection(W61_Object_t *Obj, W61_Conn_t *conn);

/**
  * @brief  Stop Client connection.
  * @param  Obj: pointer to module handle
  * @param  conn: pointer to the connection structure
  * @return Operation Status.
  */
W61_Status_t W61_Net_StopClientConnection(W61_Object_t *Obj, W61_Conn_t *conn);

/**
  * @brief  Configure and Start a Server.
  * @param  Obj: pointer to module handle
  * @param  Port: Connection Port
  * @param  Type: Connection Type
  * @param  ca_enable: 1 to enable CA
  * @param  keepalive: keepalive time in seconds
  * @return Operation Status.
  */
W61_Status_t W61_Net_StartServer(W61_Object_t *Obj, uint32_t Port, W61_ConnType_e Type, uint8_t ca_enable,
                                 uint32_t keepalive);

/**
  * @brief  Stop a Server.
  * @param  Obj: pointer to module handle
  * @param  close_connections: 1 to close all of the server connections
  * @return Operation Status.
  */
W61_Status_t W61_Net_StopServer(W61_Object_t *Obj, uint8_t close_connections);

/**
  * @brief  Send AT command to get Server Timeout and store results
  * @param  Obj: pointer to module handle
  * @param  Timeout: Configured Server Timeout, in seconds (range [0,7200], 0 means no timeout)
  * @return Operation Status.
  */
W61_Status_t W61_Net_GetServerTimeout(W61_Object_t *Obj, uint32_t *Timeout);

/**
  * @brief  Send AT command to set Server Timeout
  * @param  Obj: pointer to module handle
  * @param  Timeout: Server Timeout to set, in seconds (range [0,7200], 0 means no timeout)
  * @return Operation Status.
  */
W61_Status_t W61_Net_SetServerTimeout(W61_Object_t *Obj, uint32_t Timeout);

/**
  * @brief  Send AT command to get Server Maximum concurrent connections and store results
  * @param  Obj: pointer to module handle
  * @param  MaxConnections: Server maximum concurrent connections
  * @return Operation Status.
  */
W61_Status_t W61_Net_GetServerMaxConnections(W61_Object_t *Obj, uint8_t *MaxConnections);

/**
  * @brief  Send AT command to set Server Maximum concurrent connections
  * @param  Obj: pointer to module handle
  * @param  MaxConnections: Server maximum concurrent connections to set in range [1,5]
  * @return Operation Status.
  */
W61_Status_t W61_Net_SetServerMaxConnections(W61_Object_t *Obj, uint8_t MaxConnections);

/**
  * @brief  Send AT command to get  a given socket's t options and store results
  * @param  Obj: pointer to module handle
  * @param  Socket: Connection ID of the socket
  * @param  Linger:  Configured the SO_LINGER options for the socket
  * @param  TcpNoDelay:  Configured the TCP_NODELAY option for the socket
  * @param  SoSndTimeout:  Configured the SO_SNDTIMEO option for socket
  * @param  KeepAlive: Configured the SO_KEEPALIVE option for socket
  * @return Operation Status.
  */
W61_Status_t W61_Net_GetTCPOpt(W61_Object_t *Obj, uint8_t Socket, int16_t *Linger, uint16_t *TcpNoDelay,
                               uint16_t *SoSndTimeout, uint16_t *KeepAlive);

/**
  * @brief  Send AT command to set options for a given socket
  * @param  Obj: pointer to module handle
  * @param  Socket: Connection ID of the socket
  * @param  Linger:  Configure the SO_LINGER options for the socket
  * @param  TcpNoDelay:  Configure the TCP_NODELAY option for the socket
  * @param  SoSndTimeout:  Configure the SO_SNDTIMEO option for socket
  * @param  KeepAlive: Configure the SO_KEEPALIVE option for socket
  * @return Operation Status.
  */
W61_Status_t W61_Net_SetTCPOpt(W61_Object_t *Obj, uint8_t Socket, int16_t Linger, uint16_t TcpNoDelay,
                               uint16_t SoSndTimeout, uint16_t KeepAlive);

/**
  * @brief  Get information for an opened socket
  * @param  Obj: pointer to module handle
  * @param  Socket: Connection ID of the socket to get information for
  * @param  Protocol: Protocol used in this socket ("TCP" "UDP" or "SSL")
  * @param  RemoteIp: IP address the socket i connected to
  * @param  RemotePort: Port the socket i connected to
  * @param  LocalPort: Local port the socket uses
  * @param  Type:  if the device is the server for that socket, 0 if it is client
  * @return Operation Status.
  */
W61_Status_t W61_Net_GetSocketInformation(W61_Object_t *Obj, uint8_t Socket, uint8_t *Protocol, uint8_t *RemoteIp,
                                          uint32_t *RemotePort, uint32_t *LocalPort, uint8_t *Type);

/**
  * @brief  Send an amount data over WIFI.
  * @param  Obj: pointer to module handle
  * @param  Socket: number of the socket
  * @param  pdata: pointer to data
  * @param  req_len : nr of bytes of the data to be sent
  * @param  SentLen : pointer to variable which contains nr of bytes  sent
  * @param  Timeout: timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_Net_SendData(W61_Object_t *Obj, uint8_t Socket, uint8_t *pdata, uint32_t req_len,
                              uint32_t *SentLen, uint32_t Timeout);

/**
  * @brief  Send an amount data over WIFI using non connected protocols.
  * @param  Obj: pointer to module handle
  * @param  Socket: number of the socket
  * @param  IpAddress: IP address of remote host to send data to
  * @param  Port: Port of remote host to send data to
  * @param  pdata: pointer to data
  * @param  req_len : nr of bytes of the data to be sent
  * @param  SentLen : pointer to variable which contains nr of bytes  sent
  * @param  Timeout: timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_Net_SendData_Non_Connected(W61_Object_t *Obj, uint8_t Socket, char *IpAddress, uint32_t Port,
                                            uint8_t *pdata, uint32_t req_len, uint32_t *SentLen, uint32_t Timeout);

/**
  * @brief  Set Receive buffer length when reading data from socket
  * @param  Obj: pointer to module handle
  * @param  Socket: number of the socket
  * @param  BufLen : Buffer length to set
  * @return Operation Status.
  */
W61_Status_t W61_Net_SetReceiveBufferLen(W61_Object_t *Obj, uint8_t Socket, uint32_t BufLen);

/**
  * @brief  Get Receive buffer length when reading data from socket
  * @param  Obj: pointer to module handle
  * @param  Socket: number of the socket
  * @param  BufLen : pointer to retrieve the length of the NCP buffer
  * @return Operation Status.
  */
W61_Status_t W61_Net_GetReceiveBufferLen(W61_Object_t *Obj, uint8_t Socket, uint32_t *BufLen);

/**
  * @brief  Inquire W61 to check if socket data is available on a socket (Passive mode)
  * @param  Obj: pointer to module handle
  * @param  Socket: number of the socket
  * @param  AvailableDataSize : pointer where to return the available data size on a socket
  * @return Operation Status.
  */
W61_Status_t W61_Net_IsDataAvailableOnSocket(W61_Object_t *Obj, uint8_t Socket, uint32_t *AvailableDataSize);

/**
  * @brief  Pull data from W61 on a socket (Passive mode)
  * @param  Obj: pointer to module handle
  * @param  Socket: number of the socket
  * @param  req_len : nr of bytes of the data to be sent
  * @param  pData: pointer to data
  * @param  Receivedlen : pointer to variable which contains nr of bytes  sent
  * @param  Timeout: timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_Net_PullDataFromSocket(W61_Object_t *Obj, uint8_t Socket, uint32_t req_len, uint8_t *pData,
                                        uint32_t *Receivedlen, uint32_t Timeout);

/**
  * @brief  Send AT command to get current SNTP status, timezone and servers and store results
  * @param  Obj: pointer to module handle
  * @param  Enable:  SNTP usage status
  * @param  Timezone:  Configured Timezone
  * @param  SntpServer1:  Configured Primary SNTP Server URL
  * @param  SntpServer2:  Configured Secondary SNTP Server URL
  * @param  SntpServer3:  Configured Third SNTP Server URL
  * @return Operation Status.
  */
W61_Status_t W61_Net_GetSNTPConfiguration(W61_Object_t *Obj, uint8_t *Enable, int16_t *Timezone,
                                          uint8_t *SntpServer1, uint8_t *SntpServer2, uint8_t *SntpServer3);

/**
  * @brief  Send AT command to set SNTP status, timezone and servers
  * @param  Obj: pointer to module handle
  * @param  Enable:  Enable/Disable SNTP usage
  * @param  Timezone:  Timezone to set in one of the 2 following formats:
  *                     - range [-12,14]: marks most of the time zones by offset from UTC in whole hours
  *                     - HHmm with HH in range[-12,+14] and mm in range [00,59]
  * @param  SntpServer1:  Primary SNTP Server URL to use
  * @param  SntpServer2:  Secondary SNTP Server URL to use
  * @param  SntpServer3:  Third SNTP Server URL to use
  * @return Operation Status.
  */
W61_Status_t W61_Net_SetSNTPConfiguration(W61_Object_t *Obj, uint8_t Enable, int16_t Timezone, uint8_t *SntpServer1,
                                          uint8_t *SntpServer2, uint8_t *SntpServer3);

/**
  * @brief  Send AT command to query date string from SNTP, the used format is asctime style time and store results
  * @param  Obj: pointer to module handle
  * @param  Time:  Buffer to store time
  * @return Operation Status.
  */
W61_Status_t W61_Net_GetSNTPTime(W61_Object_t *Obj, uint8_t *Time);

/**
  * @brief  Send AT command to get SNTP Synchronisation interval and store results
  * @param  Obj: pointer to module handle
  * @param  Interval:  Configured SNTP time synchronization interval, in seconds
  * @return Operation Status.
  */
W61_Status_t W61_Net_GetSNTPInterval(W61_Object_t *Obj, uint16_t *Interval);

/**
  * @brief  Send AT command to set SNTP Synchronisation interval
  * @param  Obj: pointer to module handle
  * @param  Interval:  SNTP time synchronization interval, in seconds (range:[15,4294967])
  * @return Operation Status.
  */
W61_Status_t W61_Net_SetSNTPInterval(W61_Object_t *Obj, uint16_t Interval);

/**
  * @brief  Send AT command to get a given socket's SSL configuration (key and certificates names and Auth method) \
  *         and store results
  * @param  Obj: pointer to module handle
  * @param  Socket: Connection ID of the socket
  * @param  AuthMode:  Authentication Mode configured (0 : None, 1: Client Only, 2: Server Only, 3: Mutual Auth )
  * @param  Certificate:  Configured Client certificate filename
  * @param  PrivateKey:  Configured Private key filename
  * @param  CaCertificate:  Configured Certificate Authority's Certificate filename
  * @return Operation Status.
  */
W61_Status_t W61_Net_GetSSLConfiguration(W61_Object_t *Obj, uint8_t Socket, uint8_t *AuthMode, uint8_t *Certificate,
                                         uint8_t *PrivateKey, uint8_t *CaCertificate);

/**
  * @brief  Send AT command to set SSL configuration for a given socket (key and certificates names and Auth method)
  * @param  Obj: pointer to module handle
  * @param  Socket: Connection ID of the socket
  * @param  AuthMode:  Authentication Mode configured (0 : None, 1: Client Only, 2: Server Only, 3: Mutual Auth )
  * @param  Certificate:  Client certificate name to use (saved in NCP romfs)
  * @param  PrivateKey:  Private key name to use(saved in NCP romfs), it must match \p Certificate
  * @param  CaCertificate:  Certificate Authority's to use(saved in NCP romfs), it must be the \p Certificate issuer
  * @return Operation Status.
  */
W61_Status_t W61_Net_SetSSLConfiguration(W61_Object_t *Obj, uint8_t Socket, uint8_t AuthMode, uint8_t *Certificate,
                                         uint8_t *PrivateKey, uint8_t *CaCertificate);

/**
  * @brief  Send AT command to get SNI used for a given socket and store results
  * @param  Obj: pointer to module handle
  * @param  Socket: Connection ID of the socket
  * @param  SslSni: Server Name indication to advertise configured for the given connection ID
  * @return Operation Status.
  */
W61_Status_t W61_Net_GetSSLServerName(W61_Object_t *Obj, uint8_t Socket, uint8_t *SslSni);

/**
  * @brief  Send AT command to set SNI to use for a given socket
  * @param  Obj: pointer to module handle
  * @param  Socket: Connection ID of the socket
  * @param  SslSni: Server Name indication to advertise when connecting to an SSL Server
  * @return Operation Status.
  */
W61_Status_t W61_Net_SetSSLServerName(W61_Object_t *Obj, uint8_t Socket, uint8_t *SslSni);

/**
  * @brief  Send AT command to get ALPNs used for a given socket and store results
  * @param  Obj: pointer to module handle
  * @param  Socket: Connection ID of the socket
  * @param  Alpn1: Primary Application Layer Protocol Name to advertise configured for the given connection ID
  * @param  Alpn2: Secondary Application Layer Protocol Name to advertise configured for the given connection ID
  * @param  Alpn3: Third Application Layer Protocol Name to advertise configured for the given connection ID
  * @return Operation Status.
  */
W61_Status_t W61_Net_GetSSLApplicationLayerProtocol(W61_Object_t *Obj, uint8_t Socket, uint8_t *Alpn1,
                                                    uint8_t *Alpn2, uint8_t *Alpn3);

/**
  * @brief  Send AT command to get ALPNs to use for a given socket
  * @param  Obj: pointer to module handle
  * @param  Socket: Connection ID of the socket
  * @param  Alpn1: Primary Application Layer Protocol Name to advertise when connecting to an SSL Server
  * @param  Alpn2: Secondary Application Layer Protocol Name to advertise when connecting to an SSL Server
  * @param  Alpn3: Third Application Layer Protocol Name to advertise when connecting to an SSL Server
  * @return Operation Status.
  */
W61_Status_t W61_Net_SetSSLALPN(W61_Object_t *Obj, uint8_t Socket, uint8_t *Alpn1,
                                uint8_t *Alpn2, uint8_t *Alpn3);

/**
  * @brief  Send AT command to get PSK used for a given socket and store results
  * @param  Obj: pointer to module handle
  * @param  Socket: Connection ID of the socket
  * @param  Psk: pre-shared key to be used to open an SSL socket for the given Connection ID
  * @param  Hint: pre-shared key identity to be used to open an SSL connection for the given Connection ID
  * @return Operation Status.
  */
W61_Status_t W61_Net_GetSSLPSK(W61_Object_t *Obj, uint8_t Socket, uint8_t *Psk, uint8_t *Hint);

/**
  * @brief  Send AT command to set PSK to use for a given socket
  * @param  Obj: pointer to module handle
  * @param  Socket: Connection ID of the socket
  * @param  Psk: pre-shared key to use to open an SSL connection for the given Connection ID
  * @param  Hint: pre-shared key identity to use to open an SSL connection for the given Connection ID
  * @return Operation Status.
  */
W61_Status_t W61_Net_SetSSLPSK(W61_Object_t *Obj, uint8_t Socket, uint8_t *Psk, uint8_t *Hint);

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_HTTP_Functions ST67W61 AT Driver HTTP Functions
  * @ingroup  ST67W61_AT_HTTP
  * @{
  */
/* ===================================================================== */

/**
  * @brief  Send AT command to set HTTP mode on the NCP side
  * @param  Obj: pointer to module handle
  * @param  ActivePassive: 0 = Active, 1 = Passive
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_SetMode(W61_Object_t *Obj, uint32_t ActivePassive);

/**
  * @brief  Send AT command to know HTTP mode currently used by the NCP
  * @param  Obj: pointer to module handle
  * @param  ActivePassive: pointer to a variable : 0 = Active, 1 = Passive
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_GetMode(W61_Object_t *Obj, uint32_t *ActivePassive);

/**
  * @brief  Send AT command to set HTTP buffer size on the NCP side used in Passive Mode
  * @param  Obj: pointer to module handle
  * @param  Len: Length of the receive buffer on NCP side.
  * @return Operation Status.
  * @note   Buffer size for Passive mode can only be changed when in active mode.
  */
W61_Status_t W61_HTTP_SetReceiveBufferLen(W61_Object_t *Obj, uint32_t Len);

/**
  * @brief  Get HTTP buffer size on the NCP side used in Passive Mode
  * @param  Obj: pointer to module handle
  * @param  Len: Pointer to retrieve the length of the receive buffer on NCP side.
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_GetReceiveBufferLen(W61_Object_t *Obj, uint32_t *Len);

/**
  * @brief  Send AT command to get HTTP url configured
  * @param  Obj: pointer to module handle
  * @param  Url: URL to get data from
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_Client_Get_URL(W61_Object_t *Obj, uint8_t *Url);

/**
  * @brief  Send AT command to set HTTP url
  * @param  Obj: pointer to module handle
  * @param  Url: URL to get data from
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_Client_Set_URL(W61_Object_t *Obj, uint8_t *Url);

/**
  * @brief  Send AT command to get http data size from a given URL
  * @param  Obj: pointer to module handle
  * @param  Url: URL to get data from
  * @param  Size: Size of the data to get
  * @param  Timeout: Timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_Client_Get_Size(W61_Object_t *Obj, uint8_t *Url, uint32_t *Size, uint32_t Timeout);

/**
  * @brief  Send AT command to execute a HTTP request
  * @param  Obj: pointer to module handle
  * @param  Option: method of HTTP client request (1: HEAD, 2: GET, 3: POST, 4: PUT)
  * @param  ContentType: data type (0: application/x-www-form-urlencoded, 1: application/json, \
  *                      2: multipart/form-data, 3: text/plain)
  * @param  Url: HTTP/HTTPS URL
  * @param  Data: if POST or PUT, data to send
  * @param  Buf: if GET, HEAD or POST, buffer to store the response
  * @param  Timeout: Timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_Client(W61_Object_t *Obj, uint8_t Option, uint8_t ContentType, uint8_t *Url, uint8_t *Data,
                             uint8_t *Buf, uint32_t Timeout);

/**
  * @brief  Send AT command (when in Active Mode) to get http data from a given URL
  * @param  Obj: pointer to module handle
  * @param  Url: URL to get data from
  * @param  Buf: Buffer to store the response
  * @param  BufLen: Length of the buffer
  * @param  Timeout: Timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_Client_Active_Get(W61_Object_t *Obj, uint8_t *Url, uint8_t *Buf, uint32_t BufLen,
                                        uint32_t Timeout);

/**
  * @brief  Send AT command (when in Passive Mode) to get http data Size from a given URL page
  * @param  Obj: pointer to module handle
  * @param  Url: URL to get data from
  * @param  Size: Size of the data to get
  * @param  Timeout: Timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_Client_Passive_Get(W61_Object_t *Obj, uint8_t *Url, uint32_t *Size, uint32_t Timeout);

/**
  * @brief  Send AT command (when in Passive Mode) to receive datain the specified buffer
  * @param  Obj: pointer to module handle
  * @param  ReqID: currently unused, set it to 0
  * @param  Reqlen: Requested length
  * @param  pData: Buffer to store the response
  * @param  Receivedlen: pointer to return the actual received length
  * @param  Timeout: Timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_PullDataFromSocket(W61_Object_t *Obj, uint8_t ReqID, uint32_t Reqlen, uint8_t *pData,
                                         uint32_t *Receivedlen, uint32_t Timeout);

/**
  * @brief  Send AT command (when in Passive Mode) to know the available data Length
  * @param  Obj: pointer to module handle
  * @param  ReqID: currently unused, set it to 0
  * @param  AvailableDataSize: Length of the data available on NCP side.
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_AvailableDataLength(W61_Object_t *Obj, uint8_t ReqID, uint32_t *AvailableDataSize);

/**
  * @brief  Send AT command to put http data to a given URL
  * @param  Obj: pointer to module handle
  * @param  Url: URL to put data to
  * @param  Data: Data to put
  * @param  Buf: Buffer to store the response
  * @param  Timeout: Timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_Client_Put(W61_Object_t *Obj, uint8_t *Url, uint8_t *Data, uint8_t *Buf, uint32_t Timeout);

/**
  * @brief  Send AT command to post http data to a given URL
  * @param  Obj: pointer to module handle
  * @param  Url: URL to post data to
  * @param  Data: Data to post
  * @param  Buf: Buffer to store the response
  * @param  Timeout: Timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_HTTP_Client_Post(W61_Object_t *Obj, uint8_t *Url, uint8_t *Data, uint8_t *Buf, uint32_t Timeout);

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_MQTT_Functions ST67W61 AT Driver MQTT Functions
  * @ingroup  ST67W61_AT_MQTT
  * @{
  */
/* ===================================================================== */

/**
  * @brief  Initialize BLE device as Client/Server or de-initialize.
  * @param  Obj: pointer to module handle
  * @param  p_recv_data: pointer to received data buffer (allocated by the upper layer)
  * @param  recv_data_buf_len: length of the received data buffer
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_Init(W61_Object_t *Obj, uint8_t *p_recv_data, uint32_t recv_data_buf_len);

/**
  * @brief  Send AT command to set MQTT User configuration
  * @param  Obj: pointer to module handle
  * @param  Scheme: Authentication scheme (0: No security, 1: Username/Password, 2: Certificate)
  * @param  ClientId: pointer to the client ID
  * @param  Username: pointer to the username
  * @param  Password: pointer to the password
  * @param  Certificate: pointer to the client certificate
  * @param  PrivateKey: pointer to the client private key
  * @param  CaCertificate: pointer to the CA certificate
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_SetUserConfiguration(W61_Object_t *Obj, uint32_t Scheme, uint8_t *ClientId, uint8_t *Username,
                                           uint8_t *Password, uint8_t *Certificate, uint8_t *PrivateKey,
                                           uint8_t *CaCertificate);
/**
  * @brief  Send AT command to get MQTT User configuration
  * @param  Obj: pointer to module handle
  * @param  ClientId: pointer to the client ID
  * @param  Username: pointer to the username
  * @param  Password: pointer to the password
  * @param  Certificate: pointer to the client certificate
  * @param  PrivateKey: pointer to the client private key
  * @param  CaCertificate: pointer to the CA certificate
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_GetUserConfiguration(W61_Object_t *Obj, uint8_t *ClientId, uint8_t *Username,
                                           uint8_t *Password, uint8_t *Certificate, uint8_t *PrivateKey,
                                           uint8_t *CaCertificate);

/**
  * @brief  Send AT command to set MQTT configuration
  * @param  Obj: pointer to module handle
  * @param  KeepAlive: Keep alive time in seconds
  * @param  CleanSession: Clean session flag
  * @param  WillTopic: pointer to the will topic
  * @param  WillMessage: pointer to the will message
  * @param  WillQos: QoS level for the will message
  * @param  WillRetain: Retain flag for the will message
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_SetConfiguration(W61_Object_t *Obj, uint32_t KeepAlive, uint32_t CleanSession,
                                       uint8_t *WillTopic, uint8_t *WillMessage, uint32_t WillQos,
                                       uint32_t WillRetain);

/**
  * @brief  Send AT command to get MQTT configuration
  * @param  Obj: pointer to module handle
  * @param  KeepAlive: Keep alive time in seconds
  * @param  CleanSession: Clean session flag
  * @param  WillTopic: pointer to the will topic
  * @param  WillMessage: pointer to the will message
  * @param  WillQos: QoS level for the will message
  * @param  WillRetain: Retain flag for the will message
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_GetConfiguration(W61_Object_t *Obj, uint32_t *KeepAlive, uint32_t *CleanSession,
                                       uint8_t *WillTopic, uint8_t *WillMessage, uint32_t *WillQos,
                                       uint32_t *WillRetain);

/**
  * @brief  Send AT command to set MQTT SNI
  * @param  Obj: pointer to module handle
  * @param  SNI: pointer to the SNI
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_SetSNI(W61_Object_t *Obj, uint8_t *SNI);

/**
  * @brief  Send AT command to get MQTT SNI
  * @param  Obj: pointer to module handle
  * @param  SNI: pointer to the SNI
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_GetSNI(W61_Object_t *Obj, uint8_t *SNI);

/**
  * @brief  Send AT command to connect to the MQTT broker
  * @param  Obj: pointer to module handle
  * @param  Host: pointer to the hostname broker
  * @param  Port: port number
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_Connect(W61_Object_t *Obj, uint8_t *Host, uint16_t Port);

/**
  * @brief  Send AT command to get the connection status to the MQTT broker
  * @param  Obj: pointer to module handle
  * @param  Host: pointer to the hostname broker
  * @param  Port: pointer to the port
  * @param  Scheme: pointer to the scheme
  * @param  State: pointer to the state
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_GetConnectionStatus(W61_Object_t *Obj, uint8_t *Host, uint32_t *Port, uint32_t *Scheme,
                                          uint32_t *State);

/**
  * @brief  Send AT command to disconnect from the MQTT broker
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_Disconnect(W61_Object_t *Obj);

/**
  * @brief  Send AT command to subscribe to a topic
  * @param  Obj: pointer to module handle
  * @param  Topic: pointer to the topic
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_Subscribe(W61_Object_t *Obj, uint8_t *Topic);

/**
  * @brief  Send AT command to get the list of subscribed topics
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_GetSubscribedTopics(W61_Object_t *Obj);

/**
  * @brief  Send AT command to unsubscribe from a topic
  * @param  Obj: pointer to module handle
  * @param  Topic: pointer to the topic
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_Unsubscribe(W61_Object_t *Obj, uint8_t *Topic);

/**
  * @brief  Send AT command to publish a message to a topic
  * @param  Obj: pointer to module handle
  * @param  Topic: pointer to the topic
  * @param  Message: pointer to the message
  * @param  Message_len: length of the message
  * @return Operation Status.
  */
W61_Status_t W61_MQTT_Publish(W61_Object_t *Obj, uint8_t *Topic, uint8_t *Message, uint32_t Message_len);

/** @} */

/* ===================================================================== */
/** @defgroup ST67W61_AT_BLE_Functions ST67W61 AT Driver BLE Functions
  * @ingroup  ST67W61_AT_BLE
  * @{
  */
/* ===================================================================== */

/**
  * @brief  Set BLE TX power.
  * @param  Obj: pointer to module handle
  * @param  power: TX power
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SetTxPower(W61_Object_t *Obj, uint32_t power);

/**
  * @brief  Get BLE TX power.
  * @param  Obj: pointer to module handle
  * @param  power: TX power
  * @return Operation Status.
  */
W61_Status_t W61_Ble_GetTxPower(W61_Object_t *Obj, uint32_t *power);
/**
  * @brief  Initialize BLE device as Client/Server or de-initialize.
  * @param  Obj: pointer to module handle
  * @param  mode: BLE mode to initialize (1: Client, 2: Server, 0: De-init)
  * @param  p_recv_data: pointer to received data
  * @param  req_len: length of the received data
  * @return Operation Status.
  */
W61_Status_t W61_Ble_Init(W61_Object_t *Obj, uint8_t mode, uint8_t *p_recv_data, uint32_t req_len);

/**
  * @brief  Get BLE module initialization mode
  * @param  Obj: pointer to module handle
  * @param  Mode: Current BLE mode (1: Client, 2: Server, 0: De-init)
  * @return Operation Status.
  */
W61_Status_t W61_Ble_GetInitMode(W61_Object_t *Obj, W61_Ble_Mode_t *Mode);

/**
  * @brief  Start BLE advertising
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_Ble_AdvStart(W61_Object_t *Obj);

/**
  * @brief  Stop BLE advertising
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_Ble_AdvStop(W61_Object_t *Obj);

/**
  * @brief  Get the BLE connection status
  * @param  Obj: pointer to module handle
  * @param  ConnectionHandle: pointer to the connection handle
  * @param  address: remote address array
  * @return Operation Status.
  */
W61_Status_t W61_Ble_GetConnection(W61_Object_t *Obj, uint32_t *ConnectionHandle, uint8_t address[6]);

/**
  * @brief  Stop BLE connection
  * @param  Obj: pointer to module handle
  * @param  connection_handle: connection handle
  * @return Operation Status.
  */
W61_Status_t W61_Ble_Disconnect(W61_Object_t *Obj, uint32_t connection_handle);

/**
  * @brief  Exchange BLE MTU length
  * @param  Obj: pointer to module handle
  * @param  connection_handle: connection handle
  * @return Operation Status.
  */
W61_Status_t W61_Ble_ExchangeMTU(W61_Object_t *Obj, uint32_t connection_handle);

/**
  * @brief  Set the BLE BD address of the device
  * @param  Obj: pointer to module handle
  * @param  bdaddr: pointer to the BD address array
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SetBDAddress(W61_Object_t *Obj, const uint8_t *bdaddr);

/**
  * @brief  Get the BD address of the module.
  * @param  Obj: pointer to module handle
  * @param  BdAddr: pointer to the BD address array.
  * @return Operation Status.
  */
W61_Status_t W61_Ble_GetBDAddress(W61_Object_t *Obj, uint8_t *BdAddr);

/**
  * @brief  Set the BLE device name
  * @param  Obj: pointer to module handle
  * @param  name: pointer to the device name
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SetDeviceName(W61_Object_t *Obj, const char *name);

/**
  * @brief  Get the BLE Device name.
  * @param  Obj: pointer to module handle
  * @param  DeviceName: string containing the host name
  * @return Operation Status.
  */
W61_Status_t W61_Ble_GetDeviceName(W61_Object_t *Obj, char *DeviceName);

/**
  * @brief  Set the BLE Advertising data
  * @param  Obj: pointer to module handle
  * @param  advdata: pointer to the adv data array
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SetAdvData(W61_Object_t *Obj, const char *advdata);

/**
  * @brief  Set the BLE scan response data
  * @param  Obj: pointer to module handle
  * @param  scanrespdata: pointer to the scan response data array
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SetScanRespData(W61_Object_t *Obj, const char *scanrespdata);

/**
  * @brief  Set the BLE Advertising parameters
  * @param  Obj: pointer to module handle
  * @param  adv_int_min: minimum advertising interval this parameter is multiplied by 0.625ms
  * @param  adv_int_max: maximum advertising interval this parameter is multiplied by 0.625ms
  * @param  adv_type: Advertising type
  * @param  adv_channel: advertising channel
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SetAdvParam(W61_Object_t *Obj, uint32_t adv_int_min, uint32_t adv_int_max,
                                 uint8_t adv_type, uint8_t adv_channel);

/**
  * @brief  Enable/disable BLE device scan
  * @param  Obj: pointer to module handle
  * @param  enable: Enable/disable scan
  * @return Operation Status.
  */
W61_Status_t W61_Ble_Scan(W61_Object_t *Obj, uint8_t enable);

/**
  * @brief  Set the BLE scan parameters
  * @param  Obj: pointer to module handle
  * @param  scan_type: Type of scan (0:passive, 1:active)
  * @param  own_addr_type: type of BLE BD address
  * @param  filter_policy: Scan filter policy
  * @param  scan_interval: scan interval this parameter is multiplied by 0.625ms
  * @param  scan_window: scan window this parameter is multiplied by 0.625ms
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SetScanParam(W61_Object_t *Obj, uint8_t scan_type, uint8_t own_addr_type,
                                  uint8_t filter_policy, uint32_t scan_interval, uint32_t scan_window);

/**
  * @brief  Get the Scan parameters
  * @param  Obj: pointer to module handle
  * @param  ScanType: pointer to get scan type
  * @param  OwnAddrType: pointer to get BLE BD address type
  * @param  FilterPolicy: pointer to get scan filter
  * @param  ScanInterval: pointer to get scan interval
  * @param  ScanWindow: pointer to get scan window
  * @return Operation Status.
  */
W61_Status_t W61_Ble_GetScanParam(W61_Object_t *Obj, uint32_t *ScanType, uint32_t *OwnAddrType,
                                  uint32_t *FilterPolicy, uint32_t *ScanInterval, uint32_t *ScanWindow);

/**
  * @brief  Get the Advertising parameters
  * @param  Obj: pointer to module handle
  * @param  AdvIntMin: pointer to get minimum advertising interval
  * @param  AdvIntMax: pointer to get maximum advertising interval
  * @param  AdvType: pointer to get advertising type
  * @param  ChannelMap: pointer to get advertising channel
  * @return Operation Status.
  */
W61_Status_t W61_Ble_GetAdvParam(W61_Object_t *Obj, uint32_t *AdvIntMin,
                                 uint32_t *AdvIntMax, uint32_t *AdvType, uint32_t *ChannelMap);

/**
  * @brief  Set the BLE Connection parameters
  * @param  Obj: pointer to module handle
  * @param  conn_handle: BLE connection handle
  * @param  conn_int_min: minimum connecting interval this parameter is multiplied by 1.25ms
  * @param  conn_int_max: maximum connecting interval this parameter is multiplied by 1.25ms
  * @param  latency: latency
  * @param  timeout: connection timeout this parameter is multiplied by 10ms
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SetConnParam(W61_Object_t *Obj, uint32_t conn_handle, uint32_t conn_int_min,
                                  uint32_t conn_int_max, uint32_t latency, uint32_t timeout);

/**
  * @brief  Get the connection parameters
  * @param  Obj: pointer to module handle
  * @param  ConnHandle: pointer to get BLE connection handle
  * @param  ConnIntMin: pointer to get minimum connection interval
  * @param  ConnIntMax: pointer to get maximum connection interval
  * @param  ConnIntCurrent: pointer to get current connection interval
  * @param  Latency: pointer to get latency
  * @param  Timeout: pointer to get connection timeout
  * @return Operation Status.
  */
W61_Status_t W61_Ble_GetConnParam(W61_Object_t *Obj, uint32_t *ConnHandle, uint32_t *ConnIntMin,
                                  uint32_t *ConnIntMax, uint32_t *ConnIntCurrent, uint32_t *Latency,
                                  uint32_t *Timeout);

/**
  * @brief  Get the connection information
  * @param  Obj: pointer to module handle
  * @param  ConnHandle: pointer to get BLE connection handle
  * @param  RemoteBDAddr: pointer to get the remote device address
  * @return Operation Status.
  */
W61_Status_t W61_Ble_GetConn(W61_Object_t *Obj, uint32_t *ConnHandle, uint8_t *RemoteBDAddr);

/**
  * @brief  Create connection to a remote device
  * @param  Obj: pointer to module handle
  * @param  conn_handle: index of the BLE connection
  * @param  RemoteBDAddr: pointer to the remote device address
  * @return Operation Status.
  */
W61_Status_t W61_Ble_Connect(W61_Object_t *Obj, uint32_t conn_handle, uint8_t *RemoteBDAddr);
/**
  * @brief  Set the BLE Data length
  * @param  Obj: pointer to module handle
  * @param  conn_handle: BLE connection handle
  * @param  tx_bytes: data packet length. Range [27,251]
  * @param  tx_trans_time: data packet transition time
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SetDataLength(W61_Object_t *Obj, uint32_t conn_handle, uint32_t tx_bytes, uint32_t tx_trans_time);

/**
  * @brief  Create BLE Service
  * @param  Obj: pointer to module handle
  * @param  service_index: index of the service to create
  * @param  service_uuid: UUID of the service to create
  * @return Operation Status.
  */
W61_Status_t W61_Ble_CreateService(W61_Object_t *Obj, uint8_t service_index, const char *service_uuid);

/**
  * @brief  Delete existing BLE Service
  * @param  Obj: pointer to module handle
  * @param  service_index: index of the service to create
  * @param  ServiceInfo: pointer to the services and characteristics information (can be NULL if not required)
  * @return Operation Status.
  */
W61_Status_t W61_Ble_DeleteService(W61_Object_t *Obj, uint8_t service_index, W61_Ble_Service_t *ServiceInfo);

/**
  * @brief  List BLE Services created
  * @param  Obj: pointer to module handle
  * @param  ServiceInfo: pointer to get the existing service
  * @return Operation Status.
  */
W61_Status_t W61_Ble_GetService(W61_Object_t *Obj, W61_Ble_Service_t *ServiceInfo);

/**
  * @brief  Create BLE Characteristic
  * @param  Obj: pointer to module handle
  * @param  service_index: index of the service containing the new characteristic
  * @param  char_index: index of the characteristic to create
  * @param  char_uuid: UUID of the characteristic to create
  * @param  char_property: property of the characteristic to create
  * @param  char_permission: permission of the characteristic to create
  * @return Operation Status.
  */
W61_Status_t W61_Ble_CreateCharacteristic(W61_Object_t *Obj, uint8_t service_index, uint8_t char_index,
                                          const char *char_uuid, uint8_t char_property, uint8_t char_permission);

/**
  * @brief  List BLE Characteristics created
  * @param  Obj: pointer to module handle
  * @param  ServiceInfo: pointer to get the existing service and characteristics
  * @return Operation Status.
  */
W61_Status_t W61_Ble_GetCharacteristic(W61_Object_t *Obj, W61_Ble_Service_t *ServiceInfo);

/**
  * @brief  Register BLE Characteristics
  * @param  Obj: pointer to module handle
  * @return Operation Status.
  */
W61_Status_t W61_Ble_RegisterCharacteristics(W61_Object_t *Obj);

/**
  * @brief  Discover BLE services of remote device
  * @param  Obj: pointer to module handle
  * @param  connection_handle: index of the BLE connection
  * @return Operation Status.
  */
W61_Status_t W61_Ble_RemoteServiceDiscovery(W61_Object_t *Obj, uint8_t connection_handle);

/**
  * @brief  Discover BLE Characteristics of a service remote device
  * @param  Obj: pointer to module handle
  * @param  connection_handle: index of the BLE connection
  * @param  service_index: index of the BLE service to discover
  * @return Operation Status.
  */
W61_Status_t W61_Ble_RemoteCharDiscovery(W61_Object_t *Obj, uint8_t connection_handle, uint8_t service_index);

/**
  * @brief  Notify the Characteristic Value from the Server to a Client
  * @param  Obj: pointer to module handle
  * @param  service_index: index of the service containing characteristic to notify
  * @param  char_index: index of the characteristic to notify
  * @param  pdata: pointer to the indication data
  * @param  req_len: length of the indication data
  * @param  SentLen: pointer to the length of the sent data
  * @param  Timeout: timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_Ble_ServerSendNotification(W61_Object_t *Obj, uint8_t service_index, uint8_t char_index,
                                            uint8_t *pdata, uint32_t req_len, uint32_t *SentLen, uint32_t Timeout);

/**
  * @brief  Indicate the Characteristic Value from the Server to a Client
  * @param  Obj: pointer to module handle
  * @param  service_index: index of the service containing characteristic to indicate
  * @param  char_index: index of the characteristic to indicate
  * @param  pdata: pointer to the notification data
  * @param  req_len: length of the notification data
  * @param  SentLen: pointer to the length of the sent data
  * @param  Timeout: timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_Ble_ServerSendIndication(W61_Object_t *Obj, uint8_t service_index, uint8_t char_index,
                                          uint8_t *pdata, uint32_t req_len, uint32_t *SentLen, uint32_t Timeout);

/**
  * @brief  Set the data when Client read characteristic from the Server
  * @param  Obj: pointer to module handle
  * @param  service_index: index of the service containing characteristic to read
  * @param  char_index: index of the characteristic to read
  * @param  pdata: pointer to the data
  * @param  req_len: length of the data to set
  * @param  SentLen: pointer to the length of the sent data
  * @param  Timeout: timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_Ble_ServerSetReadData(W61_Object_t *Obj, uint8_t service_index, uint8_t char_index, uint8_t *pdata,
                                       uint32_t req_len, uint32_t *SentLen, uint32_t Timeout);

/**
  * @brief  Write data to a Server characteristic
  * @param  Obj: pointer to module handle
  * @param  conn_handle: BLE connection handle
  * @param  service_index: index of the service to write data in
  * @param  char_index: index of the characteristic to write data in
  * @param  pdata: pointer to the data to write
  * @param  req_len: length of the data to write
  * @param  SentLen: pointer to the length of the sent data
  * @param  Timeout: timeout in ms
  * @return Operation Status.
  */
W61_Status_t W61_Ble_ClientWriteData(W61_Object_t *Obj, uint8_t conn_handle, uint8_t service_index, uint8_t char_index,
                                     uint8_t *pdata, uint32_t req_len, uint32_t *SentLen, uint32_t Timeout);

/**
  * @brief  Read data from a Server characteristic
  * @param  Obj: pointer to module handle
  * @param  conn_handle: BLE connection handle
  * @param  service_index: index of the service to read data from
  * @param  char_index: index of the characteristic to read data from
  * @return Operation Status.
  */
W61_Status_t W61_Ble_ClientReadData(W61_Object_t *Obj, uint8_t conn_handle, uint8_t service_index, uint8_t char_index);

/**
  * @brief  Subscribe to notifications or indications from a Server characteristic
  * @param  Obj: pointer to module handle
  * @param  conn_handle: BLE connection handle
  * @param  char_value_handle: Characteristic value handle
  * @param  char_prop: property of the characteristic to subscribe (1 = notification, 2 = indication)
  * @return Operation Status.
  */
W61_Status_t W61_Ble_ClientSubscribeChar(W61_Object_t *Obj, uint8_t conn_handle, uint8_t char_value_handle,
                                         uint8_t char_prop);

/**
  * @brief  Unsubscribe to notifications or indications from a Server characteristic
  * @param  Obj: pointer to module handle
  * @param  conn_handle: BLE connection handle
  * @param  char_value_handle: Characteristic value handle
  * @return Operation Status.
  */
W61_Status_t W61_Ble_ClientUnsubscribeChar(W61_Object_t *Obj, uint8_t conn_handle, uint8_t char_value_handle);

/**
  * @brief  Set BLE security parameters
  * @param  Obj: pointer to module handle
  * @param  security_parameter: BLE security parameter
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SetSecurityParam(W61_Object_t *Obj, uint8_t security_parameter);

/**
  * @brief  Get BLE security parameters
  * @param  Obj: pointer to module handle
  * @param  SecurityParameter: pointer to get BLE security parameter
  * @return Operation Status.
  */
W61_Status_t W61_Ble_GetSecurityParam(W61_Object_t *Obj, uint32_t *SecurityParameter);

/**
  * @brief  Start BLE security
  * @param  Obj: pointer to module handle
  * @param  conn_handle: BLE connection handle
  * @param  security_level: security level. Range: [0,4]
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SecurityStart(W61_Object_t *Obj, uint8_t conn_handle, uint8_t security_level);

/**
  * @brief  BLE security pass key confirm
  * @param  Obj: pointer to module handle
  * @param  conn_handle: BLE connection handle
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SecurityPassKeyConfirm(W61_Object_t *Obj, uint8_t conn_handle);

/**
  * @brief  BLE pairing confirm
  * @param  Obj: pointer to module handle
  * @param  conn_handle: BLE connection handle
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SecurityPairingConfirm(W61_Object_t *Obj, uint8_t conn_handle);

/**
  * @brief  BLE set pass key
  * @param  Obj: pointer to module handle
  * @param  conn_handle: BLE connection handle
  * @param  passkey: BLE security pass key
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SecuritySetPassKey(W61_Object_t *Obj, uint8_t conn_handle, uint32_t passkey);

/**
  * @brief  BLE pairing cancel
  * @param  Obj: pointer to module handle
  * @param  conn_handle: BLE connection handle
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SecurityPairingCancel(W61_Object_t *Obj, uint8_t conn_handle);

/**
  * @brief  BLE unpair
  * @param  Obj: pointer to module handle
  * @param  RemoteBDAddr: remote device address
  * @param  remote_addr_type: remote address type
  * @return Operation Status.
  */
W61_Status_t W61_Ble_SecurityUnpair(W61_Object_t *Obj, uint8_t *RemoteBDAddr, uint32_t remote_addr_type);
/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* W61_AT_API_H */

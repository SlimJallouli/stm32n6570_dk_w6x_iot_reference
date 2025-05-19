/**
  ******************************************************************************
  * @file    w6x_wifi_shell.c
  * @author  GPM Application Team
  * @brief   This file provides code for W6x WiFi Shell Commands
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
#include "w6x_api.h"
#include "shell.h"
#include "logging.h"
#include "common_parser.h" /* Common Parser functions */
#include "FreeRTOS.h"
#include "event_groups.h"

/* Global variables ----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/** @addtogroup ST67W6X_Private_WiFi_Constants
  * @{
  */

#define EVENT_FLAG_SCAN_DONE  (1<<1)  /*!< Scan done event bitmask */

#define SCAN_TIMEOUT          10000   /*!< Delay before to declare the scan in failure */

/** @} */

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/** @addtogroup ST67W6X_Private_WiFi_Variables
  * @{
  */

/** Event bitmask flag used for asynchronous execution */
static EventGroupHandle_t scan_event = NULL;

/** @} */

/* Private function prototypes -----------------------------------------------*/
/** @addtogroup ST67W6X_Private_WiFi_Functions
  * @{
  */

/**
  * @brief  Wi-Fi Scan callback function
  * @param  status: scan status
  * @param  entry: scan result entry
  */
void Cb_scan_shell(int32_t status, W6X_Scan_Result_t *entry);

/**
  * @brief  Wi-Fi scan shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_Scan(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi connect shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_Connect(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi disconnect shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_Disconnect(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi auto-connect mode shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_AutoConnect(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi get/set hostname shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_Hostname(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi get/set STA IP shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_STA_IP(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi get/set STA Gateway shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_STA_DNS(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi get STA MAC shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_STA_MAC(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi get STA state shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_STA_State(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi get/set country code shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_Country_Code(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi start AP shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_StartAP(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi stop AP shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_StopAP(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi list connected stations shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_List_Stations(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi disconnect station shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_Disconnect_Station(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi get AP IP shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_Get_AP_IP(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi get/set DHCP shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_DHCP_Config(int32_t argc, char **argv);

/**
  * @brief  Wi-Fi get AP MAC shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0 in case of success, -1 otherwise
  */
int32_t W6X_Shell_WiFi_AP_MAC(int32_t argc, char **argv);

/**
  * @brief  Set/Get DTIM shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0
  */
int32_t W6X_Shell_WiFi_DTIM(int32_t argc, char **argv);

/* Private Functions Definition ----------------------------------------------*/
void Cb_scan_shell(int32_t status, W6X_Scan_Result_t *entry)
{
  if (scan_event != NULL)
  {
    SHELL_PRINTF("Scan results :\r\n");

    /* Print the scan results */
    for (uint32_t count = 0; count < entry->Count; count++)
    {
      SHELL_PRINTF("[" MACSTR "] Channel: %2d | %13.13s | RSSI: %4d | SSID: %32.32s\r\n",
                   MAC2STR(entry->AP[count].MAC), entry->AP[count].Channel,
                   W6X_WiFi_SecurityToStr(entry->AP[count].Security),
                   entry->AP[count].RSSI, entry->AP[count].SSID);
    }

    /* Set the scan done event */
    xEventGroupSetBits(scan_event, EVENT_FLAG_SCAN_DONE);
  }
}

int32_t W6X_Shell_WiFi_Scan(int32_t argc, char **argv)
{
  W6X_Scan_Opts_t Opts = {0};
  int32_t current_arg = 1;

  /* Initialize the scan event */
  if (scan_event == NULL)
  {
    scan_event = xEventGroupCreate();
  }

  if (argc > 11)
  {
    /* Unknown argument detected, display the usage */
    SHELL_E("Usage: wifi_scan [ -p ][ -s SSID ][ -b BSSID ][ -c channel ][ -n max_count ]\r\n");
    return W6X_STATUS_ERROR;
  }

  while (current_arg < argc)
  {
    /* Passive scan mode argument */
    if ((strncmp(argv[current_arg], "-p", 2) == 0) && strlen(argv[current_arg]) == 2)
    {
      /* Set the passive scan mode */
      Opts.Scan_type = W6X_WIFI_SCAN_PASSIVE;
    }
    /* SSID filter argument */
    else if ((strncmp(argv[current_arg], "-s", 2) == 0) && strlen(argv[current_arg]) == 2)
    {
      current_arg++;
      /* Check the SSID length */
      if ((current_arg == argc) || (strlen(argv[current_arg]) > W6X_WIFI_MAX_SSID_SIZE))
      {
        return W6X_STATUS_ERROR;
      }
      /* Copy the SSID */
      strncpy((char *)Opts.SSID, argv[current_arg], sizeof(Opts.SSID) - 1);
    }
    /* BSSID filter argument */
    else if ((strncmp(argv[current_arg], "-b", 2) == 0) && strlen(argv[current_arg]) == 2)
    {
      uint32_t temp[6];

      current_arg++;
      if (current_arg == argc)
      {
        return W6X_STATUS_ERROR;
      }

      /* Parse the BSSID filter argument */
      if (sscanf(argv[current_arg], "%02lx:%02lx:%02lx:%02lx:%02lx:%02lx",
                 &temp[0], &temp[1], &temp[2], &temp[3], &temp[4], &temp[5]) != 6)
      {
        return W6X_STATUS_ERROR;
      }

      /* Check the MAC address validity */
      for (int32_t i = 0; i < 6; i++)
      {
        if (temp[i] > 0xFF)
        {
          return W6X_STATUS_ERROR;
        }
        Opts.MAC[i] = (uint8_t)temp[i];
      }
    }
    /* Channel filter argument */
    else if ((strncmp(argv[current_arg], "-c", 2) == 0) && (strlen(argv[current_arg]) == 2))
    {
      int32_t chan;

      current_arg++;
      if (current_arg == argc)
      {
        return W6X_STATUS_ERROR;
      }
      /* Parse the channel filter argument */
      chan = ParseNumber(argv[current_arg], NULL);

      /* Check the channel validity */
      if ((chan < 1) || (chan > 13))
      {
        return W6X_STATUS_ERROR;
      }

      Opts.Channel = chan;
    }
    /* Max number of beacon received argument */
    else if ((strncmp(argv[current_arg], "-n", 2) == 0) && strlen(argv[current_arg]) == 2)
    {
      int32_t max;

      current_arg++;
      if (current_arg == argc)
      {
        return W6X_STATUS_ERROR;
      }
      /* Parse the Max number of APs argument */
      max = ParseNumber(argv[current_arg], NULL);

      /* Check the max number of APs validity */
      if ((max < 1) || (max > W61_WIFI_MAX_DETECTED_AP))
      {
        return W6X_STATUS_ERROR;
      }

      Opts.MaxCnt = max;
    }
    else
    {
      return W6X_STATUS_ERROR;
    }

    current_arg++;
  }

  /* Start the scan */
  if (W6X_STATUS_OK == W6X_WiFi_Scan(&Opts, Cb_scan_shell))
  {
    /* Wait for the scan to be done */
    if ((int32_t)xEventGroupWaitBits(scan_event, EVENT_FLAG_SCAN_DONE, pdTRUE, pdFALSE,
                                     SCAN_TIMEOUT / portTICK_PERIOD_MS) != EVENT_FLAG_SCAN_DONE)
    {
      /* Scan timeout */
      SHELL_E("Scan Failed\r\n");
    }
  }
  return W6X_STATUS_OK;
}

/** Shell command to scan for WiFi networks */
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_Scan, wifi_scan,
                       e.g. wifi_scan [ -p ] [ -s SSID ] [ -b BSSID ] [ -c channel [1; 13] ] [ -n max_count [1; 50] ]);

int32_t W6X_Shell_WiFi_Connect(int32_t argc, char **argv)
{
  W6X_Connect_Opts_t connect_opts = {0};
  int32_t current_arg = 1;
  int32_t nb = 0;

  if (argc < 2) /* SSID is mandatory */
  {
    SHELL_E("Too few arguments\r\n");
    return W6X_STATUS_ERROR;
  }

  /* Parse the SSID argument */
  if (strlen(argv[current_arg]) > W6X_WIFI_MAX_SSID_SIZE)
  {
    SHELL_E("SSID is too long\r\n");
    return W6X_STATUS_ERROR;
  }

  /* Copy the SSID */
  strncpy((char *)connect_opts.SSID, argv[current_arg], sizeof(connect_opts.SSID) - 1);
  current_arg++;

  /* Parse the Password argument if present */
  if (argc > 2 && (strncmp(argv[current_arg], "-", 1) != 0))
  {
    /* Check the password length */
    if (strlen(argv[current_arg]) > W6X_WIFI_MAX_PASSWORD_SIZE)
    {
      SHELL_E("Password is too long\r\n");
      return W6X_STATUS_ERROR;
    }
    strncpy((char *)connect_opts.Password, argv[current_arg], sizeof(connect_opts.Password) - 1);
    current_arg++;
  }

  while (current_arg < argc)
  {
    /* Parse the BSSID argument */
    if (strncmp(argv[current_arg], "-b", sizeof("-b") - 1) == 0)
    {
      uint32_t temp[6];
      current_arg++;
      if (current_arg == argc)
      {
        return W6X_STATUS_ERROR;
      }

      /* Parse the MAC address */
      if (sscanf(argv[current_arg], "%02lx:%02lx:%02lx:%02lx:%02lx:%02lx",
                 &temp[0], &temp[1], &temp[2], &temp[3], &temp[4], &temp[5]) != 6)
      {
        SHELL_E("MAC address is not valid.\r\n");
        return W6X_STATUS_ERROR;
      }

      /* Check the MAC address validity */
      for (int32_t i = 0; i < 6; i++)
      {
        if (temp[i] > 0xFF)
        {
          SHELL_E("MAC address is not valid.\r\n");
          return W6X_STATUS_ERROR;
        }
        connect_opts.MAC[i] = (uint8_t)temp[i];
      }
    }

    /* Parse the reconnection interval argument */
    else if (strncmp(argv[current_arg], "-i", sizeof("-i") - 1) == 0)
    {
      current_arg++;
      if (current_arg == argc)
      {
        return W6X_STATUS_ERROR;
      }

      nb = ParseNumber(argv[current_arg], NULL);

      /* Check the reconnection interval validity */
      if (nb < 0 || nb > 7200)
      {
        SHELL_E("Interval between two reconnection is out of range : [0;7200]\r\n");
        return W6X_STATUS_ERROR;
      }

      connect_opts.Reconnection_interval = nb;
    }
    /* Parse the reconnection attempts argument */
    else if (strncmp(argv[current_arg], "-n", sizeof("-n") - 1) == 0)
    {
      current_arg++;
      if (current_arg == argc)
      {
        return W6X_STATUS_ERROR;
      }

      nb = ParseNumber(argv[current_arg], NULL);

      /* Check the reconnection attempts validity */
      if (nb < 0 || nb > 1000)
      {
        SHELL_E("Number of reconnection attempts is out of range : [0;1000]\r\n");
        return W6X_STATUS_ERROR;
      }

      connect_opts.Reconnection_nb_attempts = nb;
    }

    current_arg++;
  }

  /* Connect to the AP */
  if (W6X_WiFi_Connect(&connect_opts) == W6X_STATUS_OK)
  {
    SHELL_PRINTF("Connection success\r\n");
  }
  else
  {
    SHELL_E("Connection error\r\n");
    return W6X_STATUS_ERROR;
  }

  return W6X_STATUS_OK;
}

/** Shell command to connect to a WiFi network */
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_Connect, wifi_sta_connect,
                       e.g. wifi_sta_connect <SSID> [ Password ] [ -b BSSID ]
                       [ -i interval [0; 7200] ] [ -n nb_attempts [0; 1000] ]);

int32_t W6X_Shell_WiFi_Disconnect(int32_t argc, char **argv)
{
  /* Disconnect from the AP */
  if (W6X_WiFi_Disconnect() != W6X_STATUS_OK)
  {
    SHELL_E("Disconnection error\r\n");
    return W6X_STATUS_ERROR;
  }
  return W6X_STATUS_OK;
}

/** Shell command to disconnect from a WiFi network */
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_Disconnect, wifi_sta_disconnect, e.g. wifi_sta_disconnect);

/* TODO: Removed until reconnect / autoconnect feature fully functional */
#if 0
int32_t W6X_Shell_WiFi_AutoConnect(int32_t argc, char **argv)
{
  uint32_t state = 0;
  uint32_t tmp = 0;

  if (argc == 1)
  {
    /* Get the auto connect current state */
    if (W6X_WiFi_GetAutoConnect(&state) == W6X_STATUS_OK)
    {
      SHELL_PRINTF("Auto connect state : %d\r\n", state);
    }
    else
    {
      SHELL_PRINTF("Auto connect get failed\r\n");
      return W6X_STATUS_ERROR;
    }
  }
  else if (argc == 2)
  {
    /* Parse the auto connect state */
    tmp = ParseNumber(argv[1], NULL);

    /* Set/Reset the auto connect */
    if (W6X_WiFi_SetAutoConnect(tmp) == W6X_STATUS_OK)
    {
      SHELL_PRINTF("Auto connect set to state : %d\r\n", tmp);
    }
    else
    {
      SHELL_PRINTF("Auto connect set failed\r\n");
      return W6X_STATUS_ERROR;
    }
  }
  else
  {
    SHELL_E("Too many arguments\r\n");
    return W6X_STATUS_ERROR;
  }

  return W6X_STATUS_OK;
}

/** Shell command to enable/disable autoconnect feature */
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_AutoConnect, wifi_auto_connect, e.g. wifi_auto_connect [state]);
#endif /* Disable autoconnect feature */

int32_t W6X_Shell_WiFi_Hostname(int32_t argc, char **argv)
{
  uint8_t hostname[34] = {0};

  if (argc == 1)
  {
    /* Get the host name */
    if (W6X_WiFi_GetHostname(hostname) == W6X_STATUS_OK)
    {
      SHELL_PRINTF("Host name : %s\r\n", hostname);
    }
    else
    {
      SHELL_PRINTF("Get host name failed\r\n");
      return W6X_STATUS_ERROR;
    }
  }
  else if (argc == 2)
  {
    /* Check the host name length */
    if (strlen(argv[1]) > 33)
    {
      SHELL_E("Host name maximum length is 32\r\n");
      return W6X_STATUS_ERROR;
    }

    /* Set the host name */
    strncpy((char *)hostname, argv[1], 33);
    if (W6X_WiFi_SetHostname(hostname) == W6X_STATUS_OK)
    {
      SHELL_PRINTF("Host name set successfully\r\n");
    }
    else
    {
      SHELL_PRINTF("Set host name failed\r\n");
      return W6X_STATUS_ERROR;
    }
  }
  else
  {
    SHELL_E("Too many arguments\r\n");
    return W6X_STATUS_ERROR;
  }

  return W6X_STATUS_OK;
}

/** Shell command to get/set the hostname */
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_Hostname, wifi_hostname, e.g. wifi_hostname [ hostname ]);

int32_t W6X_Shell_WiFi_STA_IP(int32_t argc, char **argv)
{
  uint8_t ip_addr[4] = {0};
  uint8_t gateway_addr[4] = {0};
  uint8_t netmask_addr[4] = {0};

  if (argc > 4)
  {
    SHELL_E("Too many arguments\r\n");
    return W6X_STATUS_ERROR;
  }

  if (argc == 1)
  {
    /* Get the STA IP configuration */
    if (W6X_WiFi_GetStaIpAddress(ip_addr, gateway_addr, netmask_addr) == W6X_STATUS_OK)
    {
      /* Display the IP configuration */
      SHELL_PRINTF("STA IP : " IPSTR "\r\n", IP2STR(ip_addr));
      SHELL_PRINTF("GW IP : " IPSTR "\r\n", IP2STR(gateway_addr));
      SHELL_PRINTF("NETMASK IP : " IPSTR "\r\n", IP2STR(netmask_addr));
    }
    else
    {
      SHELL_E("Get STA IP error\r\n");
      return W6X_STATUS_ERROR;
    }
  }
  else
  {
    /* Set the STA IP configuration in IP, Gateway, Netmask fixed order. Gateway and Netmask are optional */
    for (int32_t i = 1;  i < argc; i++)
    {
      switch (i)
      {
        case 1:
          /* Parse the IP address */
          ParseIP(argv[i], ip_addr);
          if (Is_ip_valid(ip_addr) != 0)
          {
            SHELL_E("IP address invalid\r\n");
            return W6X_STATUS_ERROR;
          }
          break;
        case 2:
          /* Parse the Gateway address */
          ParseIP(argv[i], gateway_addr);
          if (Is_ip_valid(gateway_addr) != 0)
          {
            SHELL_E("Gateway IP address invalid\r\n");
            return W6X_STATUS_ERROR;
          }
          break;
        case 3:
          /* Parse the Netmask address */
          ParseIP(argv[i], netmask_addr);
          if (Is_ip_valid(netmask_addr) != 0)
          {
            SHELL_E("Netmask IP address invalid\r\n");
            return W6X_STATUS_ERROR;
          }
          break;
      }
    }

    /* Set the IP configuration */
    if (W6X_WiFi_SetStaIpAddress(ip_addr, gateway_addr, netmask_addr) == W6X_STATUS_OK)
    {
      SHELL_PRINTF("STA IP configuration set successfully\r\n");
    }
    else
    {
      SHELL_E("Set STA IP configuration failed\r\n");
      return W6X_STATUS_ERROR;
    }
  }
  return W6X_STATUS_OK;
}

/** Shell command to get/set the STA IP configuration */
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_STA_IP, wifi_sta_ip,
                       e.g. wifi_sta_ip [ IP addr ] [ Gateway addr ] [ Netmask addr ]);

int32_t W6X_Shell_WiFi_STA_DNS(int32_t argc, char **argv)
{
  uint32_t dns_enable = 0;
  uint8_t dns1_addr[4] = {0};
  uint8_t dns2_addr[4] = {0};
  uint8_t dns3_addr[4] = {0};

  if (argc > 5)
  {
    SHELL_E("Too many arguments\r\n");
    return W6X_STATUS_ERROR;
  }

  if (argc == 1)
  {
    /* Get the STA DNS configuration */
    if (W6X_WiFi_GetDnsAddress(&dns_enable, dns1_addr, dns2_addr, dns3_addr) == W6X_STATUS_OK)
    {
      /* Display the DNS configuration */
      SHELL_PRINTF("DNS state : %d\r\n", dns_enable);
      SHELL_PRINTF("DNS1 IP : " IPSTR "\r\n", IP2STR(dns1_addr));
      SHELL_PRINTF("DNS2 IP : " IPSTR "\r\n", IP2STR(dns2_addr));
      SHELL_PRINTF("DNS3 IP : " IPSTR "\r\n", IP2STR(dns3_addr));
      return W6X_STATUS_OK;
    }
    else
    {
      SHELL_E("Get STA DNS configuration failed\r\n");
      return W6X_STATUS_ERROR;
    }
  }
  else
  {
    /* Set the STA DNS configuration in enable, DNS1, DNS2, DNS3 fixed order. DNS2 and DNS3 are optional */
    for (int32_t i = 1;  i < argc; i++)
    {
      switch (i)
      {
        case 1:
          /* Parse the DNS enable */
          dns_enable = ParseNumber(argv[i], NULL);
          break;
        case 2:
          /* Parse the DNS1 address */
          ParseIP(argv[i], dns1_addr);
          if (Is_ip_valid(dns1_addr) != 0)
          {
            SHELL_E("DNS IP 1 invalid\r\n");
            return W6X_STATUS_ERROR;
          }
          break;
        case 3:
          /* Parse the DNS2 address */
          ParseIP(argv[i], dns2_addr);
          if (Is_ip_valid(dns2_addr) != 0)
          {
            SHELL_E("DNS IP 2 invalid\r\n");
            return W6X_STATUS_ERROR;
          }
          break;
        case 4:
          /* Parse the DNS3 address */
          ParseIP(argv[i], dns3_addr);
          if (Is_ip_valid(dns3_addr) != 0)
          {
            SHELL_E("DNS IP 3 invalid\r\n");
            return W6X_STATUS_ERROR;
          }
          break;
      }
    }

    /* Set the DNS configuration */
    if (W6X_WiFi_SetDnsAddress(&dns_enable, dns1_addr, dns2_addr, dns3_addr) == W6X_STATUS_OK)
    {
      SHELL_PRINTF("DNS configuration set successfully\r\n");
      return W6X_STATUS_OK;
    }
    else
    {
      SHELL_E("Set DNS configuration failed\r\n");
      return W6X_STATUS_ERROR;
    }
  }
}

/** Shell command to get/set the STA DNS configuration */
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_STA_DNS, wifi_sta_dns,
                       e.g. wifi_sta_dns [ enable / disable ] [ DNS1 addr ] [ DNS2 addr ] [ DNS3 addr ]);

int32_t W6X_Shell_WiFi_STA_MAC(int32_t argc, char **argv)
{
  uint8_t mac_addr[6] = {0};

  if (argc != 1)
  {
    SHELL_E("Too many arguments\r\n");
    return W6X_STATUS_ERROR;
  }
  else
  {
    /* Get the STA MAC address */
    if (W6X_WiFi_GetStaMacAddress(mac_addr) == W6X_STATUS_OK)
    {
      /* Display the MAC address */
      SHELL_PRINTF("STA MAC : " MACSTR "\r\n", MAC2STR(mac_addr));
      return W6X_STATUS_OK;
    }
    else
    {
      SHELL_E("Get STA MAC error\r\n");
      return W6X_STATUS_ERROR;
    }
  }
}

/** Shell command to get the STA MAC address */
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_STA_MAC, wifi_sta_mac, e.g. wifi_sta_mac);

int32_t W6X_Shell_WiFi_STA_State(int32_t argc, char **argv)
{
  W6X_StaStateType_e state = W6X_WIFI_STATE_STA_DISCONNECTED;
  W6X_Connect_t ConnectData = {0};

  if (argc != 1)
  {
    SHELL_E("Too many arguments\r\n");
    return W6X_STATUS_ERROR;
  }
  else
  {
    /* Get the STA state */
    if (W6X_WiFi_GetStaState(&state, &ConnectData) == W6X_STATUS_OK)
    {
      /* Display the STA state */
      SHELL_PRINTF("STA state : %s\r\n", W6X_WiFi_StateToStr(state));

      /* Display the connection information if connected */
      if (((uint32_t)state == W6X_WIFI_STATE_STA_CONNECTED) || ((uint32_t)state == W6X_WIFI_STATE_STA_GOT_IP))
      {
        SHELL_PRINTF("Connected to following Access Point :\r\n");
        SHELL_PRINTF("[" MACSTR "] Channel: %d | RSSI: %d | SSID: %s\r\n",
                     MAC2STR(ConnectData.MAC),
                     ConnectData.Channel,
                     ConnectData.Rssi,
                     ConnectData.SSID);
      }

      return W6X_STATUS_OK;
    }
    else
    {
      SHELL_E("Get STA state error\r\n");
      return W6X_STATUS_ERROR;
    }
  }
}

/** Shell command to get the STA state */
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_STA_State, wifi_sta_state, e.g. wifi_sta_state);

int32_t W6X_Shell_WiFi_Country_Code(int32_t argc, char **argv)
{
  uint32_t policy = 0;
  char countryString[3] = {0}; /* Alpha-2 code: 2 characters + null terminator */
  int32_t len = 0;

  if (argc == 1)
  {
    /* Get the country code information */
    if (W6X_WiFi_GetCountryCode(&policy, countryString) == W6X_STATUS_OK)
    {
      /* Display the country code information */
      SHELL_PRINTF("Country policy : %d\r\nCountry code : %s\r\n",
                   policy, countryString);
    }
    else
    {
      SHELL_E("Get Country code configuration failed\r\n");
      return W6X_STATUS_ERROR;
    }
  }
  else if (argc == 3)
  {
    /* Get the country policy mode */
    policy = ParseNumber(argv[1], NULL);

    /* Check the country policy validity */
    if (!((policy == 0) || (policy == 1)))
    {
      SHELL_E("First parameter should be 0 to disable, or 1 to enable Country policy\r\n");
      return W6X_STATUS_ERROR;
    }

    /* Get the country code */
    len = strlen(argv[2]);

    /* Check the country code length */
    if ((len < 0) || (len > 2))
    {
      SHELL_E("Second parameter length is invalid\r\n");
      return W6X_STATUS_ERROR;
    }
    memcpy(countryString, argv[2], len);

    if (W6X_WiFi_SetCountryCode(&policy, countryString) == W6X_STATUS_OK)
    {
      SHELL_PRINTF("Country code configuration succeed\r\n");
    }
    else
    {
      SHELL_E("Country code configuration failed\r\n");
      return W6X_STATUS_ERROR;
    }
  }
  else
  {
    SHELL_E("Wrong number of arguments\r\n");
    return W6X_STATUS_ERROR;
  }

  return W6X_STATUS_OK;
}

/** Shell command to get/set the country code */
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_Country_Code, wifi_country_code,
                       e.g. wifi_country_code [ 0:AP aligned country code; 1:User country code ]
                       [ Country code [CN; JP; US; EU; 00] ]);

/** Shell command to start a WiFi soft-AP */
int32_t W6X_Shell_WiFi_StartAP(int32_t argc, char **argv)
{
  W6X_ApConfig_t ap_config = {0};
  int32_t current_arg = 1;
  int32_t tmp = 0;
  ap_config.MaxConnections = W6X_WIFI_MAX_CONNECTED_STATIONS;
  ap_config.Hidden = 0;
  ap_config.Channel = 1; /* default option when not defined */

  if (argc > 11)
  {
    SHELL_E("Too many arguments\r\n");
    return W6X_STATUS_ERROR;
  }

  if (argc == 1)
  {
    if (W6X_WiFi_GetApConfig(&ap_config) == W6X_STATUS_OK)
    {
      SHELL_PRINTF("AP SSID :     %s\r\n", ap_config.SSID);
      SHELL_PRINTF("AP Channel:   %d\r\n", ap_config.Channel);
      SHELL_PRINTF("AP Security : %d\r\n", ap_config.Security);
      return W6X_STATUS_OK;
    }
    else
    {
      SHELL_E("Get Soft-AP configuration failed\r\n");
      return W6X_STATUS_ERROR;
    }
  }

  while (current_arg < argc)
  {
    /* Parse the SSID argument */
    if (strncmp(argv[current_arg], "-s", 2) == 0)
    {
      current_arg++;
      /* Check the SSID length */
      if ((current_arg == argc) || (strlen(argv[current_arg]) > W6X_WIFI_MAX_SSID_SIZE))
      {
        SHELL_E("SSID invalid\r\n");
        return W6X_STATUS_ERROR;
      }
      /* Copy the SSID */
      strncpy((char *)ap_config.SSID, argv[current_arg], sizeof(ap_config.SSID) - 1);
    }
    /* Parse the Password argument */
    else if (strncmp(argv[current_arg], "-p", 2) == 0)
    {
      current_arg++;
      /* Check the password length */
      if ((current_arg == argc) || (strlen(argv[current_arg]) > W6X_WIFI_MAX_PASSWORD_SIZE))
      {
        SHELL_E("Password invalid\r\n");
        return W6X_STATUS_ERROR;
      }
      /* Copy the password */
      strncpy((char *)ap_config.Password, argv[current_arg], sizeof(ap_config.Password) - 1);
    }
    /* Parse the Channel argument */
    else if (strncmp(argv[current_arg], "-c", 2) == 0)
    {
      current_arg++;
      if (current_arg == argc)
      {
        SHELL_E("Channel value invalid\r\n");
        return W6X_STATUS_ERROR;
      }
      /* Parse the channel */
      tmp = ParseNumber(argv[current_arg], NULL);

      /* Check the channel validity */
      if ((tmp < 1) || (tmp > 13))
      {
        SHELL_E("Channel value invalid\r\n");
        return W6X_STATUS_ERROR;
      }
      ap_config.Channel = tmp;
    }
    /* Parse the Security argument */
    else if (strncmp(argv[current_arg], "-e", 2) == 0)
    {
      current_arg++;
      if (current_arg == argc)
      {
        return W6X_STATUS_ERROR;
      }
      /* Parse the security */
      tmp = ParseNumber(argv[current_arg], NULL);

      /* Check the security validity */
      if ((tmp < 0) || (tmp > 3))
      {
        SHELL_E("Security not supported in soft-AP mode\r\n");
        return W6X_STATUS_ERROR;
      }
      ap_config.Security = (W6X_SecurityType_e)tmp;
    }
    /* Parse the Hidden argument */
    else if (strncmp(argv[current_arg], "-h", 2) == 0)
    {
      current_arg++;
      if (current_arg == argc)
      {
        return W6X_STATUS_ERROR;
      }
      /* Parse the hidden value */
      tmp = ParseNumber(argv[current_arg], NULL);

      /* Check the hidden validity */
      if ((tmp < 0) || (tmp > 1))
      {
        SHELL_E("Hidden value out of range [0;1]\r\n");
        return W6X_STATUS_ERROR;
      }
      ap_config.Hidden = (uint32_t)tmp;
    }
    else
    {
      SHELL_E("Invalid parameter\r\n");
      return W6X_STATUS_ERROR;
    }
    current_arg++;
  }

  if (ap_config.SSID[0] == '\0')
  {
    SHELL_E("SSID cannot be null\r\n");
    return W6X_STATUS_ERROR;
  }
  return W6X_WiFi_StartAp(&ap_config);
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_StartAP, wifi_ap_start,
                       e.g. wifi_ap_start [ -s SSID ] [ -p Password ]
                       [ -c channel [1; 13] ] [ -e security [0:Open; 2:WPA; 3:WPA2] ] [ -h hidden [0; 1] ]);

/** Shell command to stop a WiFi soft-AP */
int32_t W6X_Shell_WiFi_StopAP(int32_t argc, char **argv)
{
  if (argc != 1)
  {
    SHELL_E("Too many arguments\r\n");
    return W6X_STATUS_ERROR;
  }
  return W6X_WiFi_StopAp();
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_StopAP, wifi_ap_stop, e.g. wifi_ap_stop);

/** Shell command to list stations connected to the soft-AP */
int32_t W6X_Shell_WiFi_List_Stations(int32_t argc, char **argv)
{
  W6X_Connected_Sta_t connected_sta = {0};

  if (argc != 1)
  {
    SHELL_E("Too many arguments\r\n");
    return W6X_STATUS_ERROR;
  }
  if (W6X_WiFi_ListConnectedSta(&connected_sta) == W6X_STATUS_OK)
  {
    SHELL_PRINTF("Connected stations :\r\n");
    for (int32_t i = 0; i < connected_sta.Count; i++)
    {
      SHELL_PRINTF("MAC : " MACSTR " | IP : " IPSTR "\r\n",
                   MAC2STR(connected_sta.STA[i].MAC),
                   IP2STR(connected_sta.STA[i].IP));
    }
    return W6X_STATUS_OK;
  }
  return W6X_STATUS_ERROR;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_List_Stations, wifi_ap_list_sta, e.g. wifi_ap_list_sta);

/** Shell command to disconnect a station connected to the soft-AP */
int32_t W6X_Shell_WiFi_Disconnect_Station(int32_t argc, char **argv)
{
  uint32_t temp[6] = {0};
  uint8_t mac_addr[6] = {0};
  if (argc != 2)
  {
    SHELL_E("Invalid number of arguments\r\n");
    return W6X_STATUS_ERROR;
  }
  /* Parse the MAC address */
  if (sscanf(argv[1], "%02lx:%02lx:%02lx:%02lx:%02lx:%02lx",
             &temp[0], &temp[1], &temp[2], &temp[3], &temp[4], &temp[5]) != 6)
  {
    SHELL_E("MAC address is not valid.\r\n");
    return W6X_STATUS_ERROR;
  }
  /* Check the MAC address validity */
  for (int32_t i = 0; i < 6; i++)
  {
    if (temp[i] > 0xFF)
    {
      SHELL_E("MAC address is not valid.\r\n");
      return W6X_STATUS_ERROR;
    }
    mac_addr[i] = (uint8_t)temp[i];
  }
  return W6X_WiFi_DisconnectSta(mac_addr);
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_Disconnect_Station, wifi_ap_disconnect_sta, e.g. wifi_ap_disconnect_sta <MAC>);

/** Shell command to get the soft-AP IP configuration */
int32_t W6X_Shell_WiFi_Get_AP_IP(int32_t argc, char **argv)
{
  uint8_t ip_addr[4] = {0};
  uint8_t netmask_addr[4] = {0};

  if (argc != 1)
  {
    SHELL_E("Too many arguments\r\n");
    return W6X_STATUS_ERROR;
  }

  /* Get the AP IP configuration */
  if (W6X_WiFi_GetApIpAddress(ip_addr, netmask_addr) == W6X_STATUS_OK)
  {
    /* Display the IP configuration */
    SHELL_PRINTF("Soft-AP IP : " IPSTR "\r\n", IP2STR(ip_addr));
    SHELL_PRINTF("Netmask IP : " IPSTR "\r\n", IP2STR(netmask_addr));
  }
  else
  {
    SHELL_E("Get AP IP error\r\n");
    return W6X_STATUS_ERROR;
  }

  return W6X_STATUS_OK;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_Get_AP_IP, wifi_ap_ip, e.g. wifi_ap_ip);

int32_t W6X_Shell_WiFi_DHCP_Config(int32_t argc, char **argv)
{
  uint8_t start_ip[4] = {0};
  uint8_t end_ip[4] = {0};
  uint32_t lease_time = 0;
  uint32_t operate = 0;
  int32_t tmp = 0;
  W6X_DhcpType_e state = W6X_WIFI_DHCP_DISABLED;

  if ((argc == 2) || (argc > 4))
  {
    SHELL_E("Wrong number of arguments\r\n");
    return W6X_STATUS_ERROR;
  }

  if (argc == 1)
  {
    /* Get the DHCP server configuration */
    if (W6X_WiFi_GetDhcp(&state, &lease_time, start_ip, end_ip) == W6X_STATUS_OK)
    {
      /* Display the DHCP server configuration */
      SHELL_PRINTF("DHCP STA STATE :     %d\r\n", state & 0x01);
      SHELL_PRINTF("DHCP AP STATE :      %d\r\n", (state & 0x02) ? 1 : 0);
      SHELL_PRINTF("DHCP AP RANGE :      [" IPSTR " - " IPSTR "]\r\n", IP2STR(start_ip), IP2STR(end_ip));
      SHELL_PRINTF("DHCP AP LEASE TIME : %d\r\n", lease_time);
    }
    else
    {
      SHELL_E("Get DHCP server configuration failed\r\n");
      return W6X_STATUS_ERROR;
    }
  }
  else
  {
    /* Get the DHCP client requested mode */
    tmp = ParseNumber(argv[1], NULL);
    if (!((tmp == 0) || (tmp == 1)))
    {
      SHELL_E("First parameter should be 0 to disable, or 1 to enable DHCP client\r\n");
      return W6X_STATUS_ERROR;
    }

    /** Global configuration of the DHCP */
    operate = tmp;
    /* Get the DHCP client requested mask: 0b01 for STA only, 0b10 for AP only, 0b11 for STA + AP */
    tmp = ParseNumber(argv[2], NULL);
    if (!((tmp == W6X_WIFI_DHCP_STA_ENABLED) || (tmp == W6X_WIFI_DHCP_AP_ENABLED) ||
          (tmp == W6X_WIFI_DHCP_STA_AP_ENABLED)))
    {
      SHELL_E("Second parameter should be 1 to configure STA only, 2 to AP only, 3 for STA + AP. "
              "0 won't configure any\r\n");
      return W6X_STATUS_ERROR;
    }
    state = (W6X_DhcpType_e)tmp;

    if (argc == 4)
    {
      /* DHCP Server configuration */
      /* Parse the lease time */
      lease_time = ParseNumber(argv[3], NULL);

      /* Check the lease time validity */
      if (lease_time > 2880)
      {
        SHELL_E("Lease time invalid\r\n");
        return W6X_STATUS_ERROR;
      }
    }

    if (W6X_WiFi_SetDhcp(&state, &operate, lease_time) == W6X_STATUS_OK)
    {
      SHELL_PRINTF("DHCP configuration succeed\r\n");
    }
    else
    {
      SHELL_E("DHCP configuration failed\r\n");
      return W6X_STATUS_ERROR;
    }
  }

  return W6X_STATUS_OK;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_DHCP_Config, wifi_dhcp,
                       e.g. wifi_dhcp [ 0:DHCP client disabled; 1:DHCP client enabled ]
                       [ 1:STA only; 2:AP only; 3:STA + AP ] [ lease_time [1; 2880] ]);

int32_t W6X_Shell_WiFi_AP_MAC(int32_t argc, char **argv)
{
  uint8_t mac_addr[6] = {0};

  if (argc != 1)
  {
    SHELL_E("Too many arguments\r\n");
    return W6X_STATUS_ERROR;
  }
  else
  {
    /* Get the AP MAC address */
    if (W6X_WiFi_GetApMacAddress(mac_addr) == W6X_STATUS_OK)
    {
      /* Display the MAC address */
      SHELL_PRINTF("AP MAC : " MACSTR "\r\n", MAC2STR(mac_addr));
      return W6X_STATUS_OK;
    }
    else
    {
      SHELL_E("Get AP MAC error\r\n");
      return W6X_STATUS_ERROR;
    }
  }
}

/** Shell command to get the STA MAC address */
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_AP_MAC, wifi_ap_mac, e.g. wifi_ap_mac);

int32_t W6X_Shell_WiFi_DTIM(int32_t argc, char **argv)
{
  uint32_t dtim = 0;

  if (argc == 1)
  {
    W6X_WiFi_GetDTIM(&dtim);
    SHELL_PRINTF("dtim is %d\r\n", dtim);
  }
  else if (argc == 2)
  {
    uint32_t dtim = ParseNumber(argv[1], NULL);
    if (dtim > 10)
    {
      SHELL_E("dtim value should be below or equal to 10\r\n");
      return -1;
    }

    if (W6X_WiFi_SetDTIM(dtim) != W6X_STATUS_OK)
    {
      SHELL_E("could not set dtim\r\n");
    }
  }
  else
  {
    SHELL_E("Usage dtim <dtim>");
    return -1;
  }
  return 0;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_WiFi_DTIM, dtim, e.g. dtim < value [1; 10] >);

/** @} */

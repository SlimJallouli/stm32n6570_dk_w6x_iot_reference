/**
  ******************************************************************************
  * @file    w6x_mqtt_shell.c
  * @author  GPM Application Team
  * @brief   This file provides code for W6x MQTT Shell Commands
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

/* Global variables ----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/** @addtogroup ST67W6X_Private_MQTT_Functions
  * @{
  */
/**
  * @brief  MQTT Configure shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval W6X_STATUS_OK if success, W6X_STATUS_ERROR otherwise
  */
int32_t W6X_Shell_MQTT_Configure(int32_t argc, char **argv);

/**
  * @brief  MQTT Connect or connection status shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval W6X_STATUS_OK if success, W6X_STATUS_ERROR otherwise
  */
int32_t W6X_Shell_MQTT_Connect(int32_t argc, char **argv);

/**
  * @brief  MQTT Disconnect shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval W6X_STATUS_OK if success, W6X_STATUS_ERROR otherwise
  */
int32_t W6X_Shell_MQTT_Disconnect(int32_t argc, char **argv);

/**
  * @brief  MQTT Subscribe or subscription status shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval W6X_STATUS_OK if success, W6X_STATUS_ERROR otherwise
  */
int32_t W6X_Shell_MQTT_Subscribe(int32_t argc, char **argv);

/**
  * @brief  MQTT Unsubscribe shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval W6X_STATUS_OK if success, W6X_STATUS_ERROR otherwise
  */
int32_t W6X_Shell_MQTT_Unsubscribe(int32_t argc, char **argv);

/**
  * @brief  MQTT Publish shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval W6X_STATUS_OK if success, W6X_STATUS_ERROR otherwise
  */
int32_t W6X_Shell_MQTT_Publish(int32_t argc, char **argv);

/* Private Functions Definition ----------------------------------------------*/
int32_t W6X_Shell_MQTT_Configure(int32_t argc, char **argv)
{
  int32_t current_arg = 1;
  W6X_MQTT_Connect_t MQTT_Config = {0};

  if (argc >= 3)
  {
    while (current_arg < argc)
    {
      /* Parse the scheme argument */
      if ((strncmp(argv[current_arg], "-s", 2) == 0) && strlen(argv[current_arg]) == 2)
      {
        current_arg++;
        if (current_arg == argc)
        {
          goto _err;
        }
        MQTT_Config.Scheme = ParseNumber(argv[current_arg], NULL);
      }
      /* Parse the client id argument */
      else if ((strncmp(argv[current_arg], "-i", 2) == 0) && strlen(argv[current_arg]) == 2)
      {
        current_arg++;
        if (current_arg == argc)
        {
          goto _err;
        }
        strncpy((char *)MQTT_Config.MQClientId, argv[current_arg], sizeof(MQTT_Config.MQClientId) - 1);
      }
      /* Parse the username argument */
      else if ((strncmp(argv[current_arg], "-u", 2) == 0) && strlen(argv[current_arg]) == 2)
      {
        current_arg++;
        if (current_arg == argc)
        {
          goto _err;
        }
        strncpy((char *)MQTT_Config.MQUserName, argv[current_arg], sizeof(MQTT_Config.MQUserName) - 1);
      }
      /* Parse the password argument */
      else if ((strncmp(argv[current_arg], "-pw", 3) == 0) && strlen(argv[current_arg]) == 3)
      {
        current_arg++;
        if (current_arg == argc)
        {
          goto _err;
        }
        strncpy((char *)MQTT_Config.MQUserPwd, argv[current_arg], sizeof(MQTT_Config.MQUserPwd) - 1);
      }
      /* Parse the certificate argument */
      else if ((strncmp(argv[current_arg], "-c", 2) == 0) && strlen(argv[current_arg]) == 2)
      {
        current_arg++;
        if (current_arg == argc)
        {
          goto _err;
        }
        strncpy((char *)MQTT_Config.Certificate, argv[current_arg], sizeof(MQTT_Config.Certificate) - 1);
      }
      /* Parse the private key argument */
      else if ((strncmp(argv[current_arg], "-k", 2) == 0) && strlen(argv[current_arg]) == 2)
      {
        current_arg++;
        if (current_arg == argc)
        {
          goto _err;
        }
        strncpy((char *)MQTT_Config.PrivateKey, argv[current_arg], sizeof(MQTT_Config.PrivateKey) - 1);
      }
      /* Parse the CA certificate argument */
      else if ((strncmp(argv[current_arg], "-ca", 3) == 0) && strlen(argv[current_arg]) == 3)
      {
        current_arg++;
        if (current_arg == argc)
        {
          goto _err;
        }
        strncpy((char *)MQTT_Config.CACertificate, argv[current_arg], sizeof(MQTT_Config.CACertificate) - 1);
      }
      /* Parse SNI enabled */
      else if ((strncmp(argv[current_arg], "-sni", 4) == 0) && strlen(argv[current_arg]) == 4)
      {
        MQTT_Config.SNI_enabled = 1;
      }
      else
      {
        goto _err;
      }

      current_arg++;
    }

    if (W6X_STATUS_OK == W6X_MQTT_Configure(&MQTT_Config))
    {
      SHELL_PRINTF("MQTT Configuration OK\r\n");
    }
    else
    {
      SHELL_E("MQTT Configuration Failure\r\n");
      goto _err;
    }
  }

  return W6X_STATUS_OK;

_err:
  return W6X_STATUS_ERROR;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_MQTT_Configure, mqtt_configure,
                       e.g. mqtt_configure < -s Scheme > < -i ClientId > [-u Username] [-pw Password]
                       [-c Certificate] [-k PrivateKey] [-ca CACertificate] [-sni]);

int32_t W6X_Shell_MQTT_Connect(int32_t argc, char **argv)
{
  int32_t current_arg = 1;
  uint32_t dtim;
  W6X_MQTT_Connect_t MQTT_Connect = {0};

  if (argc == 1)
  {
    /* Get the current connection status */
    if ((W6X_STATUS_OK == W6X_MQTT_GetConnectionStatus(&MQTT_Connect))
        && (MQTT_Connect.State <= W6X_MQTT_STATE_CONNECTED_SUBSCRIBED))
    {
      SHELL_PRINTF("MQTT State:         %s\r\n", W6X_MQTT_StateToStr(MQTT_Connect.State));

      /* Display the connection parameters */
      if ((MQTT_Connect.State == W6X_MQTT_STATE_CONNCFG_DONE) ||
          (MQTT_Connect.State >= W6X_MQTT_STATE_CONNECTED))
      {
        SHELL_PRINTF("MQTT Host:          %s\r\n", MQTT_Connect.HostName);
        SHELL_PRINTF("MQTT Port:          %d\r\n", MQTT_Connect.HostPort);
        SHELL_PRINTF("MQTT ClientId:      %s\r\n", MQTT_Connect.MQClientId);

        /* Display the connection parameters if the scheme is greater than 0 */
        if (MQTT_Connect.Scheme > 0)
        {
          SHELL_PRINTF("MQTT UserName:      %s\r\n", MQTT_Connect.MQUserName);
          SHELL_PRINTF("MQTT UserPwd:       %s\r\n", MQTT_Connect.MQUserPwd);
        }
        if (MQTT_Connect.Scheme > 1)
        {
          SHELL_PRINTF("MQTT CACertificate: %s\r\n", MQTT_Connect.CACertificate);
        }
        if (MQTT_Connect.Scheme > 2)
        {
          SHELL_PRINTF("MQTT Certificate:   %s\r\n", MQTT_Connect.Certificate);
          SHELL_PRINTF("MQTT PrivateKey:    %s\r\n", MQTT_Connect.PrivateKey);
        }
      }
    }
    else
    {
      SHELL_E("MQTT Connect Failure\r\n");
      goto _err;
    }
  }
  else
  {
    while (current_arg < argc)
    {
      /* Parse the hostname argument */
      if ((strncmp(argv[current_arg], "-h", 2) == 0) && strlen(argv[current_arg]) == 2)
      {
        current_arg++;
        if (current_arg == argc)
        {
          goto _err;
        }
        strncpy((char *)MQTT_Connect.HostName, argv[current_arg], sizeof(MQTT_Connect.HostName) - 1);
      }
      /* Parse the port argument */
      else if ((strncmp(argv[current_arg], "-p", 2) == 0) && strlen(argv[current_arg]) == 2)
      {
        current_arg++;
        if (current_arg == argc)
        {
          goto _err;
        }
        MQTT_Connect.HostPort = ParseNumber(argv[current_arg], NULL);
      }
      else
      {
        /* Unknown argument detected, display the usage */
        SHELL_E("Usage: mqtt_connect < -h Host > < -p Port >\r\n");
        goto _err;
      }

      current_arg++;
    }

    W6X_WiFi_GetDTIM(&dtim);
    W6X_WiFi_SetDTIM(0);

    if (W6X_STATUS_OK == W6X_MQTT_Connect(&MQTT_Connect))
    {
      SHELL_PRINTF("MQTT Connect OK\r\n");
    }
    else
    {
      SHELL_E("MQTT Connect Failure\r\n");
      goto _err;
    }
    W6X_WiFi_SetDTIM(dtim);
  }

  return W6X_STATUS_OK;

_err:
  return W6X_STATUS_ERROR;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_MQTT_Connect, mqtt_connect,
                       e.g. mqtt_connect < -h Host > < -p Port >);

int32_t W6X_Shell_MQTT_Disconnect(int32_t argc, char **argv)
{
  if (argc == 1)
  {
    /* Disconnect from the MQTT broker */
    if (W6X_STATUS_OK == W6X_MQTT_Disconnect())
    {
      SHELL_PRINTF("MQTT Disconnect OK\r\n");
    }
    else
    {
      SHELL_E("MQTT Disconnect Failure\r\n");
      goto _err;
    }
  }
  else
  {
    /* Unknown argument detected, display the usage */
    SHELL_E("Usage: mqtt_disconnect\r\n");
    goto _err;
  }

  return W6X_STATUS_OK;

_err:
  return W6X_STATUS_ERROR;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_MQTT_Disconnect, mqtt_disconnect, e.g. mqtt_disconnect);

int32_t W6X_Shell_MQTT_Subscribe(int32_t argc, char **argv)
{
  if (argc == 1)
  {
    /* Get the subscribed topics */
    if (W6X_STATUS_OK != W6X_MQTT_GetSubscribedTopics())
    {
      SHELL_E("MQTT Get Subscribed Topics Failure\r\n");
      goto _err;
    }
  }
  else if (argc == 2)
  {
    /* Subscribe to the topic */
    if (W6X_STATUS_OK == W6X_MQTT_Subscribe((uint8_t *)argv[1]))
    {
      SHELL_PRINTF("MQTT Subscribe OK\r\n");
    }
    else
    {
      SHELL_E("MQTT Subscribe Failure\r\n");
      goto _err;
    }
  }
  else
  {
    /* Unknown argument detected, display the usage */
    SHELL_E("Usage: mqtt_subscribe <Topic>\r\n");
    goto _err;
  }

  return W6X_STATUS_OK;

_err:
  return W6X_STATUS_ERROR;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_MQTT_Subscribe, mqtt_subscribe, e.g. mqtt_subscribe <Topic>);

int32_t W6X_Shell_MQTT_Unsubscribe(int32_t argc, char **argv)
{
  if (argc == 2)
  {
    /* Unsubscribe from the topic */
    if (W6X_STATUS_OK == W6X_MQTT_Unsubscribe((uint8_t *)argv[1]))
    {
      SHELL_PRINTF("MQTT Unsubscribe OK\r\n");
    }
    else
    {
      SHELL_E("MQTT Unsubscribe Failure\r\n");
      goto _err;
    }
  }
  else
  {
    /* Unknown argument detected, display the usage */
    SHELL_E("Usage: mqtt_unsubscribe <Topic>\r\n");
    goto _err;
  }

  return W6X_STATUS_OK;

_err:
  return W6X_STATUS_ERROR;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_MQTT_Unsubscribe, mqtt_unsubscribe, e.g. mqtt_unsubscribe <Topic>);

int32_t W6X_Shell_MQTT_Publish(int32_t argc, char **argv)
{
  if (argc == 3)
  {
    /* Publish the message to the topic */
    if (W6X_STATUS_OK == W6X_MQTT_Publish((uint8_t *)argv[1], (uint8_t *)argv[2],
                                          (uint32_t)strlen((char *)argv[2]) - 1))
    {
      SHELL_PRINTF("MQTT Publish OK\r\n");
    }
    else
    {
      SHELL_E("MQTT Publish Failure\r\n");
      goto _err;
    }
  }
  else
  {
    /* Unknown argument detected, display the usage */
    SHELL_E("Usage: mqtt_publish <Topic> <Message>\r\n");
    goto _err;
  }

  return W6X_STATUS_OK;

_err:
  return W6X_STATUS_ERROR;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_MQTT_Publish, mqtt_publish, e.g. mqtt_publish <Topic> <Message>);

/** @} */

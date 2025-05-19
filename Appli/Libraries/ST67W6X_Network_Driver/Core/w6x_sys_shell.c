/**
  ******************************************************************************
  * @file    w6x_sys_shell.c
  * @author  GPM Application Team
  * @brief   This file provides code for W6x System Shell Commands
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

/* Global variables ----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/** @addtogroup ST67W6X_Private_System_Constants
  * @{
  */

#ifndef HAL_SYS_RESET
/** HAL System software reset function */
extern void HAL_NVIC_SystemReset(void);
/** HAL System software reset macro */
#define HAL_SYS_RESET() do{ HAL_NVIC_SystemReset(); } while(0);
#endif /* HAL_SYS_RESET */

/** @} */

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/** @addtogroup ST67W6X_Private_System_Functions
  * @{
  */

/**
  * @brief  Get info shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0
  */
int32_t W6X_Shell_GetInfo(int32_t argc, char **argv);

/**
  * @brief  Reset shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0
  */
int32_t W6X_Shell_Reset(int32_t argc, char **argv);

/**
  * @brief  Write file shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0
  */
int32_t W6X_Shell_FS_WriteFile(int32_t argc, char **argv);

/**
  * @brief  Read file shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0
  */
int32_t W6X_Shell_FS_ReadFile(int32_t argc, char **argv);

/**
  * @brief  Get files list shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0
  */
int32_t W6X_Shell_FS_ListFiles(int32_t argc, char **argv);

/**
  * @brief  Low power shell function
  * @param  argc: number of arguments
  * @param  argv: pointer to the arguments
  * @retval 0
  */
int32_t W6X_Shell_LowPower(int32_t argc, char **argv);

/* Private Functions Definition ----------------------------------------------*/
int32_t W6X_Shell_GetInfo(int32_t argc, char **argv)
{
  if (W6X_ModuleInfoDisplay() != W6X_STATUS_OK)
  {
    SHELL_E("Module info not available\r\n");
    return -1;
  }

  return 0;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_GetInfo, info, e.g. display ST67W6X info);

int32_t W6X_Shell_Reset(int32_t argc, char **argv)
{
#if 0 /* W61_NCP_STORE_MODE == 1 */
  /* optional argument 0:HAL_Reset, 1:NCP_Reset, 2:NCP_Restore*/
  if (argc == 2)
  {
    if (strcmp(argv[1], "0") == 0)
    {
      HAL_SYS_RESET();
    }
    else if (strcmp(argv[1], "1") == 0)
    {
      if (W6X_ResetModule() != W6X_STATUS_OK)
      {
        SHELL_E("Error: Unable to reset module\r\n");
        return -1;
      }
    }
    else if (strcmp(argv[1], "2") == 0)
    {
      if (W6X_SetModuleDefault() != W6X_STATUS_OK)
      {
        SHELL_E("Error: Unable to restore default configuration\r\n");
        return -1;
      }
    }
    else
    {
      SHELL_E("Error: Invalid argument\r\n");
      return -1;
    }
  }
  else
  {
    HAL_SYS_RESET();
  }
#else
  HAL_SYS_RESET();
#endif /* W61_NCP_STORE_MODE */

  return 0;
}

#if 0 /* W61_NCP_STORE_MODE == 1 */
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_Reset, reset, e.g. reset < 0: HAL_Reset; 1: NCP_Reset; 2: NCP_Restore >);
#else
SHELL_CMD_EXPORT_ALIAS(W6X_Shell_Reset, reset, e.g. reset);
#endif /* W61_NCP_STORE_MODE */

int32_t W6X_Shell_FS_WriteFile(int32_t argc, char **argv)
{
  if (argc != 2)
  {
    SHELL_E("Usage: write <filename>\r\n");
    return -1;
  }

  if (W6X_FS_WriteFile(argv[1]) != W6X_STATUS_OK)
  {
    SHELL_E("Error: Unable to write file\r\n");
    return -1;
  }
  return 0;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_FS_WriteFile, fs_write, e.g. fs_write <filename>);

int32_t W6X_Shell_FS_ReadFile(int32_t argc, char **argv)
{
  uint8_t buf[257] = {0};
  uint32_t size = 0;
  uint32_t read_offset = 0;

  if (argc != 2)
  {
    SHELL_E("Usage: read <filename>\r\n");
    return -1;
  }

  if (W6X_FS_GetSizeFile(argv[1], &size) != W6X_STATUS_OK)
  {
    SHELL_E("Error: Unable to get file size\r\n");
    return -1;
  }

  if (size == 0)
  {
    SHELL_E("Error: File is empty\r\n");
    return -1;
  }
  SHELL_PRINTF("Data:\r\n");
  while (read_offset < size)
  {
    if ((size - read_offset) < 256)
    {
      W6X_FS_ReadFile(argv[1], read_offset, buf, size - read_offset);
      buf[size - read_offset] = '\0';
      SHELL_PRINTF("%s", buf);
      break;
    }
    W6X_FS_ReadFile(argv[1], read_offset, buf, 256);
    buf[256] = '\0';
    SHELL_PRINTF("%s", buf);
    read_offset += 256;
  }

  return 0;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_FS_ReadFile, fs_read, e.g. fs_read <filename>);

int32_t W6X_Shell_FS_ListFiles(int32_t argc, char **argv)
{
  W6X_FS_FilesListFull_t *files_list = NULL;
  if (W6X_FS_ListFiles(&files_list) != W6X_STATUS_OK)
  {
    SHELL_E("Error: Unable to list files\r\n");
    return -1;
  }

#if (LFS_ENABLE == 1)
  SHELL_PRINTF("Host LFS Files:\r\n");
  for (uint32_t i = 0; i < files_list->nb_files; i++)
  {
    SHELL_PRINTF("%s (size: %d)\r\n", files_list->lfs_files_list[i].name, files_list->lfs_files_list[i].size);
  }
#endif /* LFS_ENABLE */

  SHELL_PRINTF("\r\nNCP Files:\r\n");
  for (uint32_t i = 0; i < files_list->ncp_files_list.nb_files; i++)
  {
    SHELL_PRINTF("%s\r\n", files_list->ncp_files_list.filename[i]);
  }

  return 0;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_FS_ListFiles, ls, e.g. ls);

int32_t W6X_Shell_LowPower(int32_t argc, char **argv)
{
  uint32_t enable = 0;

  if (argc == 1)
  {
    W6X_GetPowerSaveAuto(&enable);
    SHELL_PRINTF("powersave mode is %s\r\n", enable ? "enabled" : "disabled");
  }
  else if (argc == 2)
  {
    enable = ParseNumber(argv[1], NULL);
    W6X_SetPowerSaveAuto(enable);
    W6X_SetPowerMode(enable);
  }
  else
  {
    SHELL_E("Usage powersave [0:disable, 1:enable]\r\n");
    return -1;
  }

  return 0;
}

SHELL_CMD_EXPORT_ALIAS(W6X_Shell_LowPower, powersave, e.g. powersave [0: disable; 1: enable]);

/** @} */

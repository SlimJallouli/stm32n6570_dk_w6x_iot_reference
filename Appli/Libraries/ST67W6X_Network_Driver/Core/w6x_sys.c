/**
  ******************************************************************************
  * @file    w6x_sys.c
  * @author  GPM Application Team
  * @brief   This file provides code for W6x System API
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
#include "w6x_version.h"
#include "w6x_internal.h"
#include "w61_at_api.h"    /* Prototypes of the functions called by this file */
#include "w61_io.h"        /* Prototypes of the BUS functions to be registered */
#include "common_parser.h" /* Common Parser functions */

/* Private typedef -----------------------------------------------------------*/
/** @defgroup ST67W6X_Private_System_Types ST67W6X System Types
  * @ingroup  ST67W6X_Private_System
  */

/* Private defines -----------------------------------------------------------*/
/** @defgroup ST67W6X_Private_System_Constants ST67W6X System Constants
  * @ingroup  ST67W6X_Private_System
  */

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/** @defgroup ST67W6X_Private_System_Variables ST67W6X System Variables
  * @ingroup  ST67W6X_Private_System
  * @{
  */

static W61_Object_t *p_DrvObj = NULL;                 /*!< Global W61 context pointer */

static W6X_App_Cb_t W6X_InternalCbHandler;            /*!< for the moment no W6X Obj, waiting Zephyr API */

static W6X_FS_FilesListFull_t *W6X_FilesList = NULL;  /*!< List of files */

static W6X_ModuleInfo_t *p_module_info = NULL;        /*!< W61 module info */

/** @} */

/* Private function prototypes -----------------------------------------------*/
/** @defgroup ST67W6X_Private_System_Functions ST67W6X System Functions
  * @ingroup  ST67W6X_Private_System
  */

/* Functions Definition ------------------------------------------------------*/
/** @addtogroup ST67W6X_API_System_Public_Functions
  * @{
  */

W6X_Status_t W6X_Init(void)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;

  p_DrvObj = W61_ObjGet();
  if (p_DrvObj == NULL)
  {
    LogError("Error: W61_Object is not defined");
    goto _err;
  }

  /* Register the Bus IO callbacks */
  ret = TranslateErrorStatus(W61_RegisterBusIO(p_DrvObj,
                                               BusIo_SPI_Init,
                                               BusIo_SPI_DeInit,
                                               BusIo_SPI_Delay,
                                               BusIo_SPI_Send,
                                               BusIo_SPI_Receive));
  W61_LowPowerConfig(p_DrvObj, 28, 2);

  if (ret != W6X_STATUS_OK)
  {
    LogError("Register Bus IO failed");
    goto _err;
  }

  /* Initialize the W61 module */
  ret = TranslateErrorStatus(W61_Init(p_DrvObj));
  if (ret != W6X_STATUS_OK)
  {
    LogError("W61 Init failed");
    goto _err;
  }

  /* Get the W61 info */
  ret = TranslateErrorStatus(W61_GetModuleInfo(p_DrvObj));
  if (ret != W6X_STATUS_OK)
  {
    LogError("Get W61 Info failed");
    goto _err;
  }

  /* retrieve W61 info */
  p_module_info = (W6X_ModuleInfo_t *)&p_DrvObj->ModuleInfo;

  /* Display the W61 info in banner */
  (void)W6X_ModuleInfoDisplay();

#if (LFS_ENABLE == 1)
  /* Initialize the File system with user certificates */
  easyflash_init();
#endif /* LFS_ENABLE */

  (void)W61_SetWakeUpPin(p_DrvObj, p_DrvObj->LowPowerCfg.WakeUpPinIn);

#if (W6X_POWER_SAVE_AUTO == 1)
  /* Set the power save mode to automatically enter low power mode in case of inactivity */
  (void)W6X_SetPowerSaveAuto(1);

  /* Enable the power mode */
  (void)W6X_SetPowerMode(1);
#endif /* W6X_POWER_SAVE_AUTO */

  ret = W6X_STATUS_OK;

_err:
  return ret;
}

W6X_Status_t W6X_DeInit(void)
{
  W6X_Status_t ret;

  /* De-Init the W61 module */
  ret = TranslateErrorStatus(W61_DeInit(p_DrvObj));
  if (ret == W6X_STATUS_OK)
  {
    p_DrvObj = NULL;
  }

  if (W6X_FilesList != NULL)
  {
    /* Free the files list */
    vPortFree(W6X_FilesList);
    W6X_FilesList = NULL;
  }

  p_module_info = NULL; /* Reset the module info */

  return ret;
}

W6X_Status_t W6X_ModuleInfoDisplay(void)
{
  if (p_module_info == NULL)
  {
    LogError("Module info not available");
    goto _err;
  }

  LogInfo("--------------- ST67W6X info ------------");
  LogInfo("ST67W6X MW Version:       " W6X_VERSION_STR);
  LogInfo("AT Version:               %s", p_module_info->AT_Version);
  LogInfo("SDK Version:              %s", p_module_info->SDK_Version);
  LogInfo("MAC Version:              %s", p_module_info->MAC_Version);
  LogInfo("Build Date:               %s", p_module_info->Build_Date);
  LogInfo("Module ID:                %s", p_module_info->ModuleID);
  LogInfo("BOM ID:                   %d", p_module_info->BomID);
  LogInfo("Manufacturing Year:       20%02d", p_module_info->Manufacturing_Year);
  LogInfo("Manufacturing Week:       %02d", p_module_info->Manufacturing_Week);
  LogInfo("Battery Voltage:          %d.%d V", p_module_info->BatteryVoltage / 1000,
          p_module_info->BatteryVoltage % 1000);
  LogInfo("Trim Wi-Fi hp:            %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
          p_module_info->trim_wifi_hp[0], p_module_info->trim_wifi_hp[1],
          p_module_info->trim_wifi_hp[2], p_module_info->trim_wifi_hp[3],
          p_module_info->trim_wifi_hp[4], p_module_info->trim_wifi_hp[5],
          p_module_info->trim_wifi_hp[6], p_module_info->trim_wifi_hp[7],
          p_module_info->trim_wifi_hp[8], p_module_info->trim_wifi_hp[9],
          p_module_info->trim_wifi_hp[10], p_module_info->trim_wifi_hp[11],
          p_module_info->trim_wifi_hp[12], p_module_info->trim_wifi_hp[13]);
  LogInfo("Trim Wi-Fi lp:            %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
          p_module_info->trim_wifi_lp[0], p_module_info->trim_wifi_lp[1],
          p_module_info->trim_wifi_lp[2], p_module_info->trim_wifi_lp[3],
          p_module_info->trim_wifi_lp[4], p_module_info->trim_wifi_lp[5],
          p_module_info->trim_wifi_lp[6], p_module_info->trim_wifi_lp[7],
          p_module_info->trim_wifi_lp[8], p_module_info->trim_wifi_lp[9],
          p_module_info->trim_wifi_lp[10], p_module_info->trim_wifi_lp[11],
          p_module_info->trim_wifi_lp[12], p_module_info->trim_wifi_lp[13]);
  LogInfo("Trim BLE:                 %d,%d,%d,%d,%d", p_module_info->trim_ble[0],
          p_module_info->trim_ble[1], p_module_info->trim_ble[2],
          p_module_info->trim_ble[3], p_module_info->trim_ble[4]);
  LogInfo("Trim XTAL:                %d", p_module_info->trim_xtal);
  LogInfo("MAC Address:              " MACSTR, MAC2STR(p_module_info->Mac_Address));
  LogInfo("Anti-rollback Bootloader: %d", p_module_info->AntiRollbackBootloader);
  LogInfo("Anti-rollback App:        %d", p_module_info->AntiRollbackApp);
  LogInfo("-----------------------------------------");

  return W6X_STATUS_OK;

_err:
  return W6X_STATUS_ERROR;
}

W6X_App_Cb_t *W6X_GetCbHandler(void)
{
  /* Return the W6X callback handler */
  return &W6X_InternalCbHandler;
}

W6X_ModuleInfo_t *W6X_GetModuleInfo(void)
{
  /* Return the W6X module info */
  return p_module_info;
}

W6X_Status_t W6X_RegisterAppCb(App_wifi_Cb    APP_wifi_cb,
                               App_net_Cb     APP_net_cb,
                               App_mqtt_Cb    APP_mqtt_cb,
                               App_ble_Cb     APP_ble_cb)
{
  /* Register the application Wi-Fi callback */
  if (APP_wifi_cb)
  {
    W6X_InternalCbHandler.APP_wifi_cb = APP_wifi_cb;
  }

  /* Register the application Net callback */
  if (APP_net_cb)
  {
    W6X_InternalCbHandler.APP_net_cb = APP_net_cb;
  }

  /* Register the application MQTT callback */
  if (APP_mqtt_cb)
  {
    W6X_InternalCbHandler.APP_mqtt_cb = APP_mqtt_cb;
  }

  /* Register the application BLE callback */
  if (APP_ble_cb)
  {
    W6X_InternalCbHandler.APP_ble_cb = APP_ble_cb;
  }

  return W6X_STATUS_OK;
}

W6X_Status_t W6X_ResetModule(void)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Reset the module */
  return TranslateErrorStatus(W61_ResetModule(p_DrvObj));
}

W6X_Status_t W6X_SetModuleDefault(void)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Restore the factory settings */
  return TranslateErrorStatus(W61_ResetToFactoryDefault(p_DrvObj));
}

W6X_Status_t W6X_FS_WriteFile(char *filename)
{
#if (LFS_ENABLE == 1)
  W6X_FS_FilesListFull_t *files_list = NULL;
  uint32_t file_lfs_index = 0;
  uint32_t file_ncp_index = 0;
  uint8_t buf[257] = {0};
  int32_t read_len = 0;
  uint32_t offset = 0;
  uint32_t file_lfs_size = 0;
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  ret = TranslateErrorStatus(W6X_FS_ListFiles(&files_list));
  if (ret != W6X_STATUS_OK)
  {
    if (ret == W6X_STATUS_ERROR)
    {
      LogError("Error: Unable to list files");
    }
    goto _err;
  }

  /* Check if the file exists in the lfs_files_list to be copied in NCP */
  for (; file_lfs_index < files_list->nb_files; file_lfs_index++)
  {
    if (strncmp(files_list->lfs_files_list[file_lfs_index].name, filename, strlen(filename)) == 0)
    {
      /* File found in the lfs_files_list */
      file_lfs_size = files_list->lfs_files_list[file_lfs_index].size;
      break;
    }
  }

  if (file_lfs_index == files_list->nb_files)
  {
    /* File not found in the lfs_files_list */
    LogError("Error: File not found in the Host LFS. Verify the filename and littlefs.bin generation");
    goto _err;
  }

  /* Check if the file exists in the NCP */
  for (; file_ncp_index < files_list->ncp_files_list.nb_files; file_ncp_index++)
  {
    if (strncmp(files_list->ncp_files_list.filename[file_ncp_index], filename, strlen(filename)) == 0)
    {
      uint32_t size = 0;
      /* Get the NCP file size */
      ret = TranslateErrorStatus(W61_FS_GetSizeFile(p_DrvObj, filename, &size));
      if (ret != W6X_STATUS_OK)
      {
        goto _err;
      }

      /* File found. Must to check if the size of Host lfs file and NCP file are equal */
      if (files_list->lfs_files_list[file_lfs_index].size == size)
      {
        /* File already exists in the NCP and the size is the same */
        /* TODO need to compare all bytes to be more efficient */
        LogInfo("File already exists in the NCP");
        return W6X_STATUS_OK;
      }
      else
      {
        /* File already exists in the NCP but the size is different: Delete operation requested */
        W61_FS_DeleteFile(p_DrvObj, filename);
      }
      break;
    }
  }

  /* Create the file entry */
  ret = TranslateErrorStatus(W61_FS_CreateFile(p_DrvObj, filename));
  if (ret != W6X_STATUS_OK)
  {
    if (ret == W6X_STATUS_ERROR)
    {
      LogError("Error: Unable to create file in NCP");
    }
    goto _err;
  }

  /* Copy the file content */
  do
  {
    /* Read data in Host lfs */
    read_len = ef_get_env_blob_offset(filename, buf, 256, NULL, offset);

    if (read_len <= 0)
    {
      LogError("Error: Unable to read file in Host LFS");
      goto _err;
    }

    /* Write data to the file */
    ret = TranslateErrorStatus(W61_FS_WriteFile(p_DrvObj, filename, offset, buf, read_len));
    if (ret != W6X_STATUS_OK)
    {
      if (ret == W6X_STATUS_ERROR)
      {
        LogError("Error: Unable to write file in NCP");
      }
      goto _err;
    }
    offset += read_len;
  } while (offset < file_lfs_size);

  LogInfo("File copied to NCP");

_err:
  return ret;
#else
  /* Cannot write data to file if no LFS service is available */
  LogError("Error: Host LFS service is not available");
  return W6X_STATUS_NOT_SUPPORTED;
#endif /* LFS_ENABLE */
}

W6X_Status_t W6X_FS_ReadFile(char *filename, uint32_t offset, uint8_t *data, uint32_t len)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Read data from the file */
  return TranslateErrorStatus(W61_FS_ReadFile(p_DrvObj, filename, offset, data, len));
}

W6X_Status_t W6X_FS_GetSizeFile(char *filename, uint32_t *size)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  /* Get the size of the file */
  return TranslateErrorStatus(W61_FS_GetSizeFile(p_DrvObj, filename, size));
}

W6X_Status_t W6X_FS_ListFiles(W6X_FS_FilesListFull_t **files_list)
{
  W6X_Status_t ret = W6X_STATUS_ERROR;
  NULL_ASSERT(p_DrvObj, W6X_Obj_Null_str);

  if (W6X_FilesList == NULL)
  {
    W6X_FilesList = pvPortCalloc(sizeof(W6X_FS_FilesListFull_t), 1);
  }
  else
  {
    /* Clear the list */
    memset(W6X_FilesList, 0, sizeof(W6X_FS_FilesListFull_t));
  }

  if (W6X_FilesList == NULL)
  {
    LogError("Error: Unable to allocate memory for files list");
    goto _err;
  }

  /* List the user files */
#if (LFS_ENABLE == 1)
  ef_print_env(W6X_FilesList->lfs_files_list, &W6X_FilesList->nb_files);
#endif /* LFS_ENABLE */

  /* List the NCP files */
  ret = TranslateErrorStatus(W61_FS_ListFiles(p_DrvObj, (W61_FS_FilesList_t *)&W6X_FilesList->ncp_files_list));
  if (ret != W6X_STATUS_OK)
  {
    goto _err;
  }

  *files_list = W6X_FilesList;

_err:
  return ret;
}

W6X_Status_t W6X_SetPowerMode(uint32_t ps_mode)
{
  return TranslateErrorStatus(W61_SetPowerMode(p_DrvObj, (ps_mode == 0) ? 0 : 2, 0));
}

W6X_Status_t W6X_GetPowerMode(uint32_t *ps_mode)
{
  uint32_t mode = 0;
  W61_Status_t ret = W61_GetPowerMode(p_DrvObj, &mode);
  *ps_mode = (mode == 0) ? 0 : 1;

  return TranslateErrorStatus(ret);
}

W6X_Status_t W6X_SetPowerSaveAuto(uint32_t enable)
{
  return TranslateErrorStatus(W61_SetPowerSaveAuto(p_DrvObj, (enable == 0) ? 0 : 1));
}

W6X_Status_t W6X_GetPowerSaveAuto(uint32_t *enable)
{
  return TranslateErrorStatus(W61_GetPowerSaveAuto(p_DrvObj, enable));
}

/** @} */

/** @addtogroup ST67W6X_Private_Common_Functions
  * @{
  */

W6X_Status_t TranslateErrorStatus(uint32_t ret61)
{
  switch (ret61)
  {
    case W61_STATUS_OK:
      return W6X_STATUS_OK;
    case W61_STATUS_BUSY:
      return W6X_STATUS_BUSY;
    default:  /* TODO:  ticket 200132 still to be implemented */
      return W6X_STATUS_ERROR;
  }
}

/** @} */

/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : w6x_fs.c
 * @date           :
 * @brief          :
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
#include "FreeRTOS.h"
#include "event_groups.h"
#include "logging.h"

#include "sys_evt.h"

#include "w6x_fs.h"

#include "w61_at_api.h"
#include "w6x_api.h"
#include "w6x_types.h"
#include <string.h>

/* Private typedef -----------------------------------------------------------*/

/* Private Macro -------------------------------------------------------------*/

/* Private Variables ---------------------------------------------------------*/
static W61_Object_t *p_DrvObj = NULL;                 /*!< Global W61 context pointer */

/* Private Function prototypes -----------------------------------------------*/

/* User code -----------------------------------------------------------------*/

W61_Object_t *W6X_GetDefaultFsCtx(void)
{
  if(p_DrvObj == NULL)
  {
    W6X_FileInit();
  }

  return p_DrvObj;
}

W6X_Status_t W6X_file_stat(W61_Object_t *pLfsCtx, const char *pcFileName, uint32_t *puFileSize)
{
  return W6X_GetFileSize(pcFileName, puFileSize);
}

W6X_Status_t W6X_file_validate(W61_Object_t *pLfsCtx, const char *pcFileName)
{
  return W6X_CheckFile(pcFileName);
}

W6X_Status_t W6X_file_read(W61_Object_t *pLfsCtx, const char *pcFileName, char* pData, uint32_t *puFileSize)
{
  return W6X_ReadFile(pcFileName, pData, puFileSize);
}

W6X_Status_t W6X_file_write(W61_Object_t *pLfsCtx, const char *pcFileName, char* pData, uint32_t uFileSize)
{
  return W6X_WriteFile(pcFileName, pData, &uFileSize);
}

W6X_Status_t W6X_file_create(W61_Object_t *pLfsCtx, const char *pcFileName)
{
  return W6X_CreateFile(pcFileName);
}

W6X_Status_t W6X_file_delete(W61_Object_t *pLfsCtx, const char *pcFileName)
{
  return W6X_DeleteFile(pcFileName);
}

W6X_Status_t  W6X_file_list(W61_Object_t *pLfsCtx, W6X_FS_FilesListFull_t **ppcFileList)
{
  W6X_Status_t xStatus = W6X_STATUS_ERROR;

  xStatus = W6X_FS_ListFiles(ppcFileList);

  if (xStatus != W6X_STATUS_OK)
  {
    LogError("Error: Unable to list files");
  }

  return xStatus;
}

/***************************************************************/
bool W6X_FileInit(void)
{
  (void) xEventGroupWaitBits(xSystemEvents, EVT_MASK_NET_INIT, pdFALSE, pdTRUE, portMAX_DELAY);

  p_DrvObj = W61_ObjGet();

  if (!p_DrvObj)
  {
    LogError("Error: W61_Object is not defined");
    return false;
  }

  return true;
}

W6X_Status_t W6X_CheckFile(const char *pcFileName)
{
  W6X_Status_t xStatus = W6X_STATUS_ERROR;
  W6X_FS_FilesListFull_t *files_list = NULL;

  xStatus = W6X_FS_ListFiles(&files_list);

  if (xStatus != W6X_STATUS_OK)
  {
    LogError("Error: Unable to list files");
  }

  if (xStatus == W6X_STATUS_OK)
  {
    xStatus = W6X_STATUS_ERROR;

    for (uint32_t index = 0; index < files_list->ncp_files_list.nb_files; index++)
    {
      if (strncmp(files_list->ncp_files_list.filename[index], pcFileName, strlen(pcFileName)) == 0)
      {
        xStatus = W6X_STATUS_OK;
        break;
      }
    }
  }

  return xStatus;
}

W6X_Status_t W6X_ReadFile(const char *pcFileName, char *file_data, uint32_t *puFileSize)
{
  uint32_t offset = 0;
  W6X_Status_t xStatus;

  xStatus = W6X_GetFileSize((char *)pcFileName, puFileSize);

  if(W6X_STATUS_OK == xStatus)
  {
    LogInfo("Reading %s file with size: 0x%08X",pcFileName, *puFileSize);

    xStatus = W61_FS_ReadFile(p_DrvObj, (char *)pcFileName, offset, (uint8_t *)file_data, *puFileSize);

    if(W6X_STATUS_OK != xStatus)
    {
      LogError("Failed to read the %s file", pcFileName);
    }
  }

  return xStatus;
}

W6X_Status_t W6X_WriteFile(const char *pcFileName, char *file_data, uint32_t *puFileSize)
{
  uint32_t offset = 0;
  W6X_Status_t xStatus;

  xStatus = W61_FS_WriteFile(p_DrvObj, (char *)pcFileName, offset, (uint8_t *)file_data, *puFileSize);

  if(W6X_STATUS_OK != xStatus)
  {
    LogError("Failed to write the %s file size", pcFileName);
  }

  return xStatus;
}

W6X_Status_t W6X_GetFileSize(const char * pcFileName, uint32_t *puFileSize)
{
  W6X_Status_t xStatus;

  xStatus = W61_FS_GetSizeFile(p_DrvObj, (char *)pcFileName, puFileSize);

  if(W6X_STATUS_OK != xStatus)
  {
    LogError("Failed to read the %s file size", pcFileName);
  }

  return xStatus;
}

W6X_Status_t W6X_DeleteFile(const char * pcFileName)
{
  W6X_Status_t xStatus;

  xStatus = W61_FS_DeleteFile(p_DrvObj, (char *)pcFileName);

  if(W6X_STATUS_OK != xStatus)
  {
    LogError("Failed to delete the %s file", pcFileName);
  }

  return xStatus;
}

W6X_Status_t W6X_CreateFile(const char * pcFileName)
{
  W6X_Status_t xStatus;

  xStatus = W61_FS_CreateFile(p_DrvObj, (char *)pcFileName);

  if(W6X_STATUS_OK != xStatus)
  {
    LogError("Failed to create the %s file", pcFileName);
  }

  return xStatus;
}

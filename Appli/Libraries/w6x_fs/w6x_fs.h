/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : w6x_fs.h
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
#ifndef _W6X_FS_
#define _W6X_FS_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "w6x_types.h"
#include "w61_at_api.h"

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
bool W6X_FileInit(void);
W6X_Status_t W6X_CheckFile  (const char *file_name);
W6X_Status_t W6X_ReadFile   (const char *file_name, char *file_data, uint32_t *puFileSize);
W6X_Status_t W6X_WriteFile  (const char *file_name, char *file_data, uint32_t *puFileSize);
W6X_Status_t W6X_GetFileSize(const char *file_name, uint32_t *puFileSize);

W6X_Status_t W6X_DeleteFile (const char *file_name);
W6X_Status_t W6X_CreateFile (const char *file_name);

W61_Object_t *W6X_GetDefaultFsCtx(void);
W6X_Status_t  W6X_file_stat    (W61_Object_t *pLfsCtx, const char *pcFileName, uint32_t *puFileSize);
W6X_Status_t  W6X_file_validate(W61_Object_t *pLfsCtx, const char *pcFileName);
W6X_Status_t  W6X_file_read    (W61_Object_t *pLfsCtx, const char *pcFileName, char* pData, uint32_t *puFileSize);
W6X_Status_t  W6X_file_write   (W61_Object_t *pLfsCtx, const char *pcFileName, char* pData, uint32_t uFileSize);
W6X_Status_t  W6X_file_create  (W61_Object_t *pLfsCtx, const char *pcFileName);
W6X_Status_t  W6X_file_delete  (W61_Object_t *pLfsCtx, const char *pcFileName);
W6X_Status_t  W6X_file_list    (W61_Object_t *pLfsCtx, W6X_FS_FilesListFull_t **ppcFileList);

#endif /* _W6X_FS_ */

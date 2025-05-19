/**
  ******************************************************************************
  * @file    w6x_internal.h
  * @author  GPM Application Team
  * @brief   This file contains the internal definitions of the API
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
#ifndef W6X_INTERNAL_H
#define W6X_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/

/** @defgroup ST67W6X_Private ST67W6X Internal
  */

/** @defgroup ST67W6X_Private_Common ST67W6X Common
  * @ingroup  ST67W6X_Private
  */

/** @defgroup ST67W6X_Private_System ST67W6X System
  * @ingroup  ST67W6X_Private
  */

/** @defgroup ST67W6X_Private_WiFi ST67W6X Wi-Fi
  * @ingroup  ST67W6X_Private
  */

/** @defgroup ST67W6X_Private_Net ST67W6X Net
  * @ingroup  ST67W6X_Private
  */

/** @defgroup ST67W6X_Private_HTTP ST67W6X HTTP
  * @ingroup  ST67W6X_Private
  */

/** @defgroup ST67W6X_Private_MQTT ST67W6X MQTT
  * @ingroup  ST67W6X_Private
  */

/** @defgroup ST67W6X_Private_BLE ST67W6X BLE
  * @ingroup  ST67W6X_Private
  */

/** @defgroup ST67W6X_Private_OTA ST67W6X OTA
  * @ingroup  ST67W6X_Private
  */

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/** @defgroup ST67W6X_Private_Common_Constants ST67W6X Commons Constants
  * @ingroup  ST67W6X_Private_Common
  * @{
  */

/** W61 context pointer error string */
static const char W6X_Obj_Null_str[] = "Object pointer not initialized";

/** @} */

/* Exported variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/** @defgroup ST67W6X_Private_Common_Macros ST67W6X Commons Macros
  * @ingroup  ST67W6X_Private_Common
  * @{
  */

/** Pointer NULL check and return error */
#define NULL_ASSERT(p, s) if ((p) == NULL) { LogError("%s", (s)); return ret; }

/** Translate W61 to W6X status and return error */
#define RETURN_ERROR_STATUS(err61, str) \
  if ((ret = TranslateErrorStatus((err61))) == W6X_STATUS_ERROR) { LogError("%s", (str)); } return ret;

/** @} */

/* Exported functions ------------------------------------------------------- */
/** @defgroup ST67W6X_Private_Common_Functions ST67W6X Commons Functions
  * @ingroup  ST67W6X_Private_Common
  * @{
  */

/**
  * @brief  Translate W61 status to W6X status
  * @param  ret61: W61 status
  * @retval W6X status
  */
W6X_Status_t TranslateErrorStatus(uint32_t ret61);

/** @} */

/** @addtogroup ST67W6X_Private_HTTP_Functions
  * @{
  */

/**
  * @brief  This is a temporary HTTP client API for the FOTA feature, is not compatible with other usage.
  *         Does an HTTP request based on URI and settings information passed in parameters.
  *         Return in the last parameter the content length of the resource at the URI specified.
  *         Currently, only GET request is supported
  * @note   This is a temporary HTTP client API for the FOTA feature, is not compatible in other usage.
  * @param  sock:           Number of the sock to use for the HTTP request
  * @param  server_name:    Server target for the given URI. It is either the IP of the server or the name of server
                            example: www.somethingwithoutURI.com
  *                         It is preferred but not mandatory to have a server name that can be resolved by the server.
  *                         This field (Host field in HTTP RFC standard) is mandatory.
  * @param  uri:            URI to request (Uniform Resource Identifier)
  * @param  settings:       Settings to use for the HTTP request
  * @param  content_length: Content length of the resource at the URI specified.
                            Is 0 if there is no body content to fetch.
  * @param  body_start:     Buffer containing the start of the HTTP request body if any.
  *                         If none is expected or yet received, body_start is unchanged.
  *                         Buffer allocated size should be greater or equal to HTTP_HEAD_MAX_RESP_BUFFER_SIZE
  *                         if data is expected.
  * @param  body_len:       Gives the length of the HTTP request body already received in buffer.
  *                         Equals 0 if no body expected or not yet received.
  * @return W6X_STATUS_OK if success, W6X_STATUS_ERROR otherwise
  */
W6X_Status_t W6X_HTTP_OTA_Client_Request(int32_t sock, const char *server_name, const uint8_t *uri,
                                         const W6X_http_connection_t *settings, uint32_t *content_length,
                                         uint8_t *body_start, size_t *body_len);

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* W6X_INTERNAL_H */

/**
  ******************************************************************************
  * @file    w61_at_common.h
  * @author  GPM Application Team
  * @brief   This file provides the common definitions of the AT driver
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
#ifndef W61_AT_COMMON_H
#define W61_AT_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include "w61_at_api.h"

/** @defgroup ST67W61_AT_Common ST67W61 AT Driver Common
  * @ingroup  ST67W61_AT
  */

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/** @defgroup ST67W61_AT_Common_Constants ST67W61 AT Driver Common Constants
  * @ingroup  ST67W61_AT_Common
  * @{
  */

/** Parameter Store Mode to maintain the user NCP configuration after boot
  * TODO to be removed when the store mode feature is fully functional
  */
#define W61_NCP_STORE_MODE                0

/** Debug check to detect miss-usage of the AT driver by the application */
#define W61_CHECK_AT_LOCK                 0

/** Maximum size of header for sending data */
#define W61_MAX_SEND_DATA_HEADER_SIZE     48

/** Maximum size of header for receiving data */
#define W61_MAX_RECEIVE_DATA_HEADER_SIZE  24

#ifndef W61_ASSERT_EN
/** it detect miss-usage of the driver by the application */
#define W61_ASSERT_EN                     1
#endif /* W61_ASSERT_EN */

#ifndef W61_MAX_AT_LOG_LENGTH
/** Maximum size of AT log */
#define W61_MAX_AT_LOG_LENGTH             30
#endif /* W61_MAX_AT_LOG_LENGTH */

#ifndef W61_AT_LOG_ENABLE
/** Enable AT log */
#define W61_AT_LOG_ENABLE                 0
#endif /* W61_AT_LOG_ENABLE */

#if (W61_AT_LOG_ENABLE == 1)
#define AT_LOG_HOST_OUT(pBuf, len)        ATlogger( pBuf, len, ">")

#define AT_LOG_HOST_IN(pBuf, len)         ATlogger( pBuf, len, "<")
#else
/** AT log trace to announce an AT command sent to the module */
#define AT_LOG_HOST_OUT(pBuf, len)

/** AT log trace to announce an AT message received from the module */
#define AT_LOG_HOST_IN(pBuf, len)
#endif /* W61_AT_LOG_ENABLE */

/** Cmd/query default timeouts */
#ifndef AT_LOCK_TIMEOUT
/** Time to wait before returning BUSY status */
#define  AT_LOCK_TIMEOUT                  2000
#endif

/** Cmd/query default timeouts */
#ifndef W61_NCP_TIMEOUT
/** Timeout for reply/execute of NCP */
#define  W61_NCP_TIMEOUT                  100
#endif /* W61_NCP_TIMEOUT */

#ifndef W61_SYS_TIMEOUT
/** Timeout for special cases like flash write, OTA, etc */
#define  W61_SYS_TIMEOUT                  200
#endif /* W61_SYS_TIMEOUT */

#ifndef W61_WIFI_TIMEOUT
/** Timeout for remote WIFI device operation (e.g. AP) */
#define  W61_WIFI_TIMEOUT                 2000
#endif /* W61_WIFI_TIMEOUT */

#ifndef W61_NET_TIMEOUT
/** Timeout for remote network operation (e.g. server) */
#define  W61_NET_TIMEOUT                  2000
#endif /* W61_NET_TIMEOUT */

#ifndef W61_BLE_TIMEOUT
/** Timeout for remote BLE device operation */
#define  W61_BLE_TIMEOUT                  2000
#endif /* W61_BLE_TIMEOUT */

/** Carriage return and line feed */
#define CRLF                              "\r\n"

/** OK message string received from the module after all commands */
#define AT_OK_STRING                      "OK\r\n"

/** Error message string received from the module after a command error */
#define AT_ERROR_STRING                   "ERROR\r\n"

/** @} */

/* Exported variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/** @defgroup ST67W61_AT_Common_Macros ST67W61 AT Driver Common Macros
  * @ingroup  ST67W61_AT_Common
  * @{
  */

#if (W61_ASSERT_EN == 1)
/** Assert function */
#define  W61_ASSERT(expr) ((expr) ? (void)0U : W61_assert_failed((uint8_t *)__FILE__, __LINE__))
#else
#define  W61_ASSERT(expr)
#endif /* W61_ASSERT_EN */

/** Check if the response buffer contains the OK string */
#define IS_STRING_OK (strstr((char *)Obj->CmdResp, AT_OK_STRING)!=NULL)

/* TODO it might be changed to: (need to check if regressions)
 * #define IS_STRING_OK strncmp((char *)Obj->CmdResp, "\r\nOK\r\n", sizeof("\r\nOK\r\n") - 1)
 */

/** @} */

/* Exported functions ------------------------------------------------------- */
/** @defgroup ST67W61_AT_Common_Functions ST67W61 AT Driver Common Functions
  * @ingroup  ST67W61_AT_Common
  * @{
  */

/**
  * @brief  This function is called when an assert error occurs.
  * @param  file: pointer to the source file name
  * @param  line: assert error line source number
  */
void W61_assert_failed(uint8_t *file, uint32_t line);

/**
  * @brief  lock the bus resource in case of several application threads (Wi-Fi, Ble, Sockets, etc)
  * @param  Obj: pointer to module handle, so to the mutex handle
  * @param  busy_timeout_ms: time to wait before returning busy state (in ms)
  * @retval bool: TRUE if the semaphore was obtained. FALSE if timeout expired before the semaphore was obtained.
  */
bool ATlock(W61_Object_t *Obj, uint32_t busy_timeout_ms);

/**
  * @brief  Release the above mentioned lock
  * @param  Obj: pointer to module handle, so to the mutex handle
  * @retval bool: TRUE semaphore released. FALSE if fails to release the mutex.
  */
bool ATunlock(W61_Object_t *Obj);

/**
  * @brief  Send Data to the ST67W611M module over the SPI interface.
  * @param  Obj: pointer to module handle
  * @param  pBuf: a buffer inside which the data will be read.
  * @param  len: the Maximum size of the data to receive.
  * @param  timeout_ms: the actual data size that has been received.
  * @retval int32_t: the actual data size that has been sent.
  */
int32_t ATsend(W61_Object_t *Obj, uint8_t *pBuf, uint32_t len, uint32_t timeout_ms);

/**
  * @brief  Check if response OK or ERROR.
  * @param  p_resp: pointer to string
  * @return Operation status.
  */
W61_Status_t AT_ParseOkErr(char *p_resp);

/**
  * @brief  Send the AT command for Set and Execute mode, and check the status response.
  * @param  Obj: pointer to module handle
  * @param  p_cmd: pointer to pass command string
  * @param  timeout_ms: timeout for the ATrecv (ATsend uses Obj->NcpTimeout)
  * @return Operation status.
  */
W61_Status_t AT_SetExecute(W61_Object_t *Obj, uint8_t *p_cmd, uint32_t timeout_ms);

/**
  * @brief  Send the AT command for Queries, wait Query response and check the status response.
  * @param  Obj: pointer to module handle
  * @param  p_cmd: pointer to pass command string
  * @param  p_resp: pointer to  receive the response
  * @param  timeout_ms: timeout for the first ATrecv (ATsend and second ATreceive use Obj->NcpTimeout)
  * @return Operation status.
  */
W61_Status_t AT_Query(W61_Object_t *Obj, uint8_t *p_cmd, uint8_t *p_resp, uint32_t timeout_ms);

/**
  * @brief  Execute AT command with data.
  * @param  Obj: pointer to module handle
  * @param  len: binary data length
  * @param  pdata: pointer to returned data
  * @param  timeout_ms: timeout in milliseconds
  * @param  is_recv_bytes: flag to check if receive bytes must be evaluated
  * @retval Bytes actually sent.
  */
W61_Status_t AT_RequestSendData(W61_Object_t *Obj, uint8_t *pdata, uint32_t len, uint32_t timeout_ms,
                                bool is_recv_bytes);

/**
  * @brief  Log AT command.
  * @param  pBuf: pointer to buffer
  * @param  len: buffer length
  * @param  inOut: pointer to string
  */
void ATlogger(uint8_t *pBuf, uint32_t len, char *inOut);

#if defined(__ICCARM__) || defined(__ICCRX__) /* For IAR Compiler */
/**
  * @brief  function locates the first occurrence of the  null-terminated string little in the string big.
  */
char *strnstr(const char *big, const char *little, size_t len);
#endif /* __ICCARM__ */

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* W61_AT_COMMON_H */

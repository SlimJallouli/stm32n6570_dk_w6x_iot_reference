/**
  ******************************************************************************
  * @file    w61_at_rx_parser.h
  * @author  GPM Application Team
  * @brief   This file provides the low layer definitions of the AT driver
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
#ifndef W61_AT_RX_PARSER_H
#define W61_AT_RX_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "w61_at_api.h"
#include "w61_driver_config.h"

/** @defgroup ST67W61_AT_RX_Parser ST67W61 AT Driver Rx Parser
  * @ingroup  ST67W61_AT
  */

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/** @defgroup ST67W61_AT_RX_Parser_Constants ST67W61 AT Driver Rx Parser Constants
  * @ingroup  ST67W61_AT_RX_Parser
  * @{
  */

/**
  * @brief  TODO since some messages (to be fixed on coprocessor) miss the CRLF characters at its end
  *         this work-around allows such messages to use the CRLF in front to the next OK message
  *         to complete their own (by concatenating it)
  *         on next OK, OK+CRLF will be accepted instead of CRLF+OK+CRLF
  */
#define WORKAROUND_FOR_MISSING_CRLF             1

/**
  * @brief  TODO sometime DATA is followed by CRLF (NET, BLE) sometime not ((HTTP)
  *         waiting Coprocessor to decide one of the two strategy
  */
#define WORKAROUND_FOR_DATA_CRLF                0

#ifndef W61_ATD_RX_TASK_STACK_SIZE_BYTES
/** stack required especially for Log messages */
#define W61_ATD_RX_TASK_STACK_SIZE_BYTES           (64 * 1280)// @SJ I added 16 *. printf generates hard fault
#endif /* W61_ATD_RX_TASK_STACK_SIZE_BYTES */

#ifndef W61_ATD_EVENTS_TASK_STACK_SIZE_BYTES
/**
  * @brief  Notice that callbacks are executed on this task, so some margin is taken when sizing the stack.
  *         However, if users (when writing their application callbacks) need more space,
  *         e.g. application buffers, printf, etc
  *         the W61_ATD_EVENTS_TASK_STACK_SIZE_BYTES should be redefined in the "w61_driver_config.h"
  *         600 bytes is the minimum (event callbacks should do nothing)
  */
#define W61_ATD_EVENTS_TASK_STACK_SIZE_BYTES       2048
#endif /* W61_ATD_EVENTS_TASK_STACK_SIZE_BYTES */

#ifndef W61_ATD_RX_TASK_PRIO
/** W61 AT Rx parser task priority, recommended to be higher than application tasks */
#define W61_ATD_RX_TASK_PRIO                   54
#endif /* W61_ATD_RX_TASK_PRIO */

#ifndef W61_ATD_EVENTS_TASK_PRIO
/** W61 AT Rx parser task priority should be lower than W61_ATD_RX_TASK_PRIO */
#define W61_ATD_EVENTS_TASK_PRIO               (W61_ATD_RX_TASK_PRIO-4)
#endif /* W61_ATD_EVENTS_TASK_PRIO */

#ifndef W61_ATD_CMDRSP_XBUF_SIZE
/** Size of the x-buffer used to queue the AT commands-responses */
#define W61_ATD_CMDRSP_XBUF_SIZE                512
#endif /* W61_ATD_CMDRSP_XBUF_SIZE */

#ifndef W61_ATD_CMDRSP_STRING_SIZE
/** Max size of the string containing AT commands or AT responses
    It shall be =<  W61_ATD_CMDRSP_XBUF_SIZE */
#define W61_ATD_CMDRSP_STRING_SIZE              192
#endif /* W61_ATD_CMDRSP_STRING_SIZE */

#ifndef W61_ATD_EVENTS_XBUF_SIZE
/** Size of the x-buffer used to queue the AT events */
#define W61_ATD_EVENTS_XBUF_SIZE                512
#endif /* W61_ATD_EVENTS_XBUF_SIZE */

#ifndef W61_ATD_EVENT_STRING_SIZE
/**  Max size of the string containing AT events
    It shall be =<  W61_ATD_EVENTS_XBUF_SIZE */
#define W61_ATD_EVENT_STRING_SIZE               192
#endif /* W61_ATD_EVENT_STRING_SIZE */

/** when calling ATRecv() this is given as max size param */
#define AT_RSP_SIZE                             W61_ATD_CMDRSP_STRING_SIZE

/** the dispatcher doesn't need the entire string to dispatch nominal events */
#define AT_EVT_SEGMENT_NEEDED_BY_DISPATCHER     10

/**
  * Should contain one of the following:
  * +CIPRECVDATA:[size],
  * +HTTPC:[size],
  * +BLE:GATTWRITE:y,y,y,[size]
  * +MQTT:SUBRECV,y,[size1],[size2]
  */
#define MAX_SIZEOF_RECV_DATA_HEADER             28

/** @} */

/* Exported variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
/** @defgroup ST67W61_AT_RX_Parser_Functions ST67W61 AT Driver Rx Parser Functions
  * @ingroup  ST67W61_AT_RX_Parser
  * @{
  */

/**
  * @brief  Pool Rx data from BusIO_Receive buffer and call AT_Parser/dispatcher
  * @param  arg: pointer to module handle
  */
void ATD_RxPooling_task(void *arg);

/**
  * @brief  Pool Events from ATD_Evt_xMessageBuffer and dispatch it to the correspondent module
  * @param  arg: pointer to module handle
  */
void ATD_EventsPooling_task(void *arg);

/**
  * @brief  Receive Response from the Obj->ATD_Resp_xMessageBuffer.
  *         This function receives data from the Obj->ATD_Resp_xMessageBuffer, the
  *         data is fetched from a ring message buffer that is asynchronously
  *         and continuously filled with by the ATD_RxPooling_task().
  * @note   Internal function shall only be called by the CMD/RSP function (applicative task).
  *         and within the ATlock ATunlock protection.
  * @param  Obj: pointer to module handle, reserved for future use
  * @param  pBuf: a buffer inside which the data will be read.
  * @param  len: the Maximum size of the data to receive.
  * @param  timeout_ms: the actual data size that has been received.
  * @retval int32_t: the actual data size that has been received.
  */
int32_t ATrecv(W61_Object_t *Obj, uint8_t *pBuf, uint32_t len, uint32_t timeout_ms);

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* W61_AT_RX_PARSER_H */

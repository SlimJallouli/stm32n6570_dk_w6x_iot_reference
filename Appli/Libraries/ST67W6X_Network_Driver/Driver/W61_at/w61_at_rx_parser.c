/**
  ******************************************************************************
  * @file    w61_at_rx_parser.c
  * @author  GPM Application Team
  * @brief   This file provides the low layer implementations of the AT driver
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
#include "w61_at_rx_parser.h"
#include "w61_at_api.h"
#include "w61_at_common.h"
#include "w61_at_internal.h"
#include "w61_io.h" /* SPI_XFER_MTU_BYTES */
#include "common_parser.h" /* Common Parser functions */
#include "message_buffer.h"

#if (SYS_DBG_ENABLE_TA4 >= 1)
#include "trcRecorder.h"
#endif /* SYS_DBG_ENABLE_TA4 */

/* Global variables ----------------------------------------------------------*/
extern uint32_t ActivePassiveMode; /* 0 active, 1 passive */ /* TODO temporary workaround */

/* Private typedef -----------------------------------------------------------*/
/** @defgroup ST67W61_AT_RX_Parser_Types ST67W61 AT Driver Rx Parser Types
  * @ingroup  ST67W61_AT_RX_Parser
  * @{
  */

/**
  * @brief  AT events list structure
  */
typedef struct
{
  /** Event keyword */
  const char evtDetectCharacters[AT_EVT_SEGMENT_NEEDED_BY_DISPATCHER];
  /** Length of the event keyword */
  const uint8_t evtDetectCharactersLen;
} W61_AtEvtIdList_t;

/**
  * @brief  AT data events list structure
  */

typedef struct
{
  /** Event keyword */
  const char evtDataKeyword[MAX_SIZEOF_RECV_DATA_HEADER];
  /** Length of the event keyword */
  const uint8_t evtDataKeywordLen;
  /** Offset where the parameter length is situated in the string */
  const uint8_t evtParamLenOffset;
  /** Data mode */
  const uint8_t DataMode;
} W61_AtDataEvtIdList_t;

/* Data string vocabulary:
  "DataHeader" is the part of message that precede the <data>, example:
                         +BLE:GATTWRITE:x,y,z,<param len>,<......data.....>
  evtDataKeywordLen      |<----------->|
  evtParamLenOffset      |<----------------->|
  data_offset            |<----------------------------->|

 "string" is here intended as data received by the IoBus each shot of Obj->fops.IO_Receive
  |<                    ATD_RecvBuf                                                                >|
  |<     string_1                   >|<     string_2        >|<         string_3         >|
  |<    DataHeader    >|<          Data  (e.g. 48 bytes)                   >|<   msg_X   >|
When string_3 is received, before calling the ATD_RxDispatcher_func() these are the values of the param:
                       |<    DataReceivedInPreviousStrings  >|
  "LenExtractedFromDataHeader" is 48, <param len> maintained after exiting the ATD_RxDispatcher_func()
  "RetainRecvDataHeader" is DataHeader maintained after exiting the ATD_RxDispatcher_func()
  "data_offset" will be 0, because string_3 starts with Data
  "next_loop_index"  0 when entering the function, will become offset to    |<   msg_X   >|
PS: ABOVE EXPLANATION REQUIRES THE TEXT EDITOR TO USE SAME SPACE BETWEEN CHARACTER LIKE "Courier New".
*/

/** @} */

/* Private defines -----------------------------------------------------------*/
/** @addtogroup ST67W61_AT_RX_Parser_Constants
  * @{
  */

/** size of /r/n string */
#define CRLF_SIZE  2

/** offset from which to start parsing for finding /r/n end of message delimiter */
#define CRLF_SEARCH_OFFSET  2

/** Number of items in the AT events list */
#define ITEMS_IN_EVT_LIST   (sizeof(AtEvtList) / sizeof(W61_AtEvtIdList_t))

/** Number of items in the AT data events list */
#define ITEMS_IN_DATA_EVT_LIST  (sizeof(AtDataEvtList) / sizeof(W61_AtDataEvtIdList_t))

/** Data buffer size */
#define RX_DATA_BUFFER_SIZE           SPI_XFER_MTU_BYTES

/** Timeout to receive an event from the stream buffer */
#define EVENT_TIMEOUT                 portMAX_DELAY

/** Timeout to receive a response from the stream buffer */
#define RESPONSE_TIMEOUT_MS           10000

/** Timeout to send data to the stream buffer */
#define POSTING_X_MSG_TIMEOUT         portMAX_DELAY

/** Maximum number of parser loops */
#define MAX_PARSER_LOOPS              100

/** Receive command, response or events mode */
#define RECV_DATA_MODE_NONE           0

/** Receive Net data mode */
#define RECV_DATA_MODE_NET            1

/** Receive HTTP data mode */
#define RECV_DATA_MODE_HTTP           2

/** Receive MQTT data mode */
#define RECV_DATA_MODE_MQTT           3

/** Receive BLE data mode */
#define RECV_DATA_MODE_BLE            4

/** TODO Temporary workaround waiting the NCP release v2.0.61 */
#define CIPRECVDATA_IP_PORT_REMOVAL_TMP_FIX 1

/** TODO Temporary workaround waiting the NCP release v2.0.61 */
#define WORKAROUND_FOR_UNWANTED_CRLF_WITH_2_0_61 1

/** @} */

/* Private macros ------------------------------------------------------------*/
/** @defgroup ST67W61_AT_RX_Parser_Macros ST67W61 AT Driver Rx Parser Macros
  * @ingroup  ST67W61_AT_RX_Parser
  * @{
  */

/** Check if the character is CR */
#define IS_CHAR_CR(ch)                ((ch)=='\r')

/** Check if the character is LF */
#define IS_CHAR_LF(ch)                ((ch)=='\n')

/** Check if the character is '>' */
#define IS_CHAR_SEND(ch)              ((ch)=='>')

/** @} */

/* Private variables ---------------------------------------------------------*/
/** @defgroup ST67W61_AT_RX_Parser_Variables ST67W61 AT Driver Rx Parser Variables
  * @ingroup  ST67W61_AT_RX_Parser
  * @{
  */

/** 0: in non-data, mode, 1: Wi-Fi data, 2: BLE data */
static int8_t  RecvDataMode;

/** Data received from the AT module can be fragmented, this buffer retains the DataHeader */
static uint8_t RetainRecvDataHeader[MAX_SIZEOF_RECV_DATA_HEADER];

/** Value extracted from the DataHeader of the events listed in AtDataEvtList[] */
static int32_t LenExtractedFromDataHeader;

/** Used when data announced by LenExtractedFromDataHeader does not arrive in one shot but fragmented */
static int32_t DataReceivedInPreviousStrings;

/** Wi-Fi AT events list */

/** AT events list */
static const W61_AtEvtIdList_t AtEvtList[] =  /* Used by w61_at_rx_parser.c */
{
  { "+IPD:", sizeof("+IPD:") - 1 },
  { "+CIP:", sizeof("+CIP:") - 1 },
  { "+MQTT:", sizeof("+MQTT:") - 1 },
  { "+BLE:", sizeof("+BLE:") - 1 },
  { "+CW:", sizeof("+CW:") - 1 },
  { "+CWLAP:", sizeof("+CWLAP:") - 1 }
};

/** AT data events list */
static const W61_AtDataEvtIdList_t AtDataEvtList[] =
{
  { "+CIPRECVDATA:",   sizeof("+CIPRECVDATA:") - 1,   sizeof("+CIPRECVDATA:") - 1,         RECV_DATA_MODE_NET},
  { "+HTTPC:",         sizeof("+HTTPC:") - 1,         sizeof("+HTTPC:y,") - 1,             RECV_DATA_MODE_HTTP},
  { "+HTTPCGET:",      sizeof("+HTTPCGET:") - 1,      sizeof("+HTTPCGET:y,") - 1,          RECV_DATA_MODE_HTTP},
  { "+HTTPRECVDATA:",  sizeof("+HTTPRECVDATA:") - 1,  sizeof("+HTTPRECVDATA:") - 1,        RECV_DATA_MODE_HTTP},
  { "+MQTT:SUBRECV:",  sizeof("+MQTT:SUBRECV:") - 1,  sizeof("+MQTT:SUBRECV:y,") - 1,      RECV_DATA_MODE_MQTT},
  { "+BLE:GATTWRITE:", sizeof("+BLE:GATTWRITE:") - 1, sizeof("+BLE:GATTWRITE:y,y,y,") - 1, RECV_DATA_MODE_BLE},
  { "+BLE:GATTREAD:",  sizeof("+BLE:GATTREAD:") - 1,  sizeof("+BLE:GATTREAD:y,y,y,") - 1,  RECV_DATA_MODE_BLE},
  { "+BLE:NOTIDATA:",  sizeof("+BLE:NOTIDATA:") - 1,  sizeof("+BLE:NOTIDATA:") - 1,        RECV_DATA_MODE_BLE}
};

/** @} */

/* Private function prototypes -----------------------------------------------*/
/** @addtogroup ST67W61_AT_RX_Parser_Functions
  * @{
  */
/**
  * @brief  Check if the parameter length to be extracted from the string is complete and return the offset
  * @param  p_msg: pointer to string
  * @param  string_len: length of the entire received string from which to extract the parameter length
  * @param  param_len_offset: offset where the parameter length is situated in the string
  * @retval int32_t: 0 if parameter length is incomplete, otherwise the number of chars of <param len + ,>
  */
static int32_t ATD_GetDataOffset(uint8_t *p_msg, int32_t string_len, int32_t param_len_offset);

/**
  * @brief  Check if /r/n are present at the begin of the message and return how many
  * @param  p_msg: pointer to string
  * @retval uint32_t: number of /r/n at the begin of the message, e.i. if /r/n/r/n crlf_count=2
  */
static uint32_t ATD_CountCrlfAtBeginMsg(uint8_t *p_msg);

/**
  * @brief  Extract parameter length value, retains msg header and return Data Offset.
  * @param  p_msg: pointer to string
  * @param  string_len: length of the entire received string
  * @param  Recv_Data_Mode: pointer used to return the value of the RECV_DATA_MODE
  * @param  Len_Extracted_From_Data_Header: pointer used to return the value of the RECV_DATA_MODE
  * @retval int32_t: 0 evt not found, -1 error, otherwise data_offset is the number of bytes after which data starts
  */
static int32_t ATD_ProcessDataHeader(uint8_t *p_msg, int32_t string_len, int8_t *Recv_Data_Mode,
                                     int32_t *Len_Extracted_From_Data_Header);

/**
  * @brief  Check if the received and isolated single message is a classical Event and queue it.
  * @param  Obj: pointer to module handle
  * @param  p_evt: pointer to string
  * @param  single_evt_length: length of the isolated (decoupled) string to be parsed
  * @retval bool: False if event not found, True if event detected. .
  */
static bool ATD_DetectEvent(W61_Object_t *Obj, uint8_t *p_evt, int32_t single_evt_length);

/**
  * @brief  Pre_parse, decompose string and dispatch received strings
  * @param  Obj: pointer to module handle, reserved for future use
  * @param  p_input: pointer within the ATD_RecvBuf buffer to the begin of the unprocessed part.
  * @param  string_len: size of the unprocessed part of the ATD_RecvBuf to be parsed.
  * @retval int32_t remaining_len: 0 if msg ends with CRLF else length of incomplete message
  */
static int32_t ATD_RxDispatcher_func(W61_Object_t *Obj, uint8_t *p_input, int32_t string_len);

/* Functions Definition ------------------------------------------------------*/
/* --------------------------------------------------------------------------------------------------------------------
ATD_RxPooling_task() local variables vocabulary  and Concatenation description:
 "string" is intended as data received by the IoBus each shot of Obj->fops.IO_Receive
 "msg" is intended as RESPONSE or ASYNC_EVENT (not for DATA)
Each time a new string arrives the ATD_RxDispatcher_func() is called.
In nominal case, strings contain full messages, a typical sequence will be.
  |<                    ATD_RecvBuf                                                           >|
  |<     string_1     >|
  |<      msg_1       >|
  |<        string_2       >|
  |<   msg_2   >|<  msg_3  >|
  |<   string_3    >|
  |<     msg_4     >|
In case of DATA there is no concatenation,
each time it arrives is processed and next restart from begin of the ATD_RecvBuf
  |<                     string_4                      >|
  |<   msg_5    >|<              data                  >|
  |<         string_5         >|
  |<        data              >|
However the code covers also the case the string would contain just part of messages by concatenating the strings
If a msg is not complete, next string gets concatenated, until :
 "string end" equals with a "message end", or any data segment is received or ATD_RecvBuf gets full
  |<                    ATD_RecvBuf                                                           >|
  |<     string_1     >|<     string_2        >|<       string_3       >|
  |<   msg_1   >|<   msg_2  >|<        msg_3         >|<     msg_4     >|
Following terminology is used.
When string_3 is received, before calling the ATD_RxDispatcher_func() these are the values of the param:
  |<                           concat_len                              >|
  |<      processed_len     >|
                             |< remaining_len >|
                                               |<      string_len      >|
                             |<      concat_len - processed_len        >|
PS: ABOVE EXPLANATION REQUIRES THE TEXT EDITOR TO USE SAME SPACE BETWEEN CHARACTER LIKE "Courier New".
-------------------------------------------------------------------------------------------------------------------- */
void ATD_RxPooling_task(void *arg)
{
  W61_Object_t *Obj = arg;
  int32_t string_len = 0;
  int32_t concat_len = 0;      /* Concatenation length */
  int32_t remaining_len = 0;
  int32_t processed_len = 0;

  RecvDataMode = 0;
  DataReceivedInPreviousStrings = 0;

  while (1)
  {
#if (SYS_DBG_ENABLE_TA4 >= 1)
    vTracePrintF("ATD_RxPooling_task", "rx_task: Entering spisync_read Sleep ");
#endif /* SYS_DBG_ENABLE_TA4 */
    /* When the rsp/evt is not complete (\r\n not received) AT_RxDispatcher_func() returns a value != 0 */
    /* Which means it waits until \r\n before processing the ATD_RecvBuf[] content */
    /* The IO_Receive function will keep previous data in ATD_RecvBuf[] and add next data after it (concat_len shift) */
    string_len = Obj->fops.IO_Receive((uint8_t *)(Obj->ATD_RecvBuf + concat_len),
                                      RX_DATA_BUFFER_SIZE - concat_len, pdMS_TO_TICKS(RESPONSE_TIMEOUT_MS));

    if (string_len > 0)
    {
      AT_LOG_HOST_IN(Obj->ATD_RecvBuf + concat_len, string_len);  /* Log info */
#if (SYS_DBG_ENABLE_TA4 >= 1)
      vTracePrintF("ATD_RxPooling_task", "rx_task: Exiting spisync_read: data received");
#endif /* SYS_DBG_ENABLE_TA4 */
      concat_len += string_len;
      remaining_len = ATD_RxDispatcher_func(Obj, Obj->ATD_RecvBuf + processed_len, concat_len - processed_len);
      if (remaining_len == 0)
      {
        /* When last message in the pipe ends with "\r\n" the pointers can be reset to the begin of the ATD_RecvBuf */
        memset(Obj->ATD_RecvBuf, 0,
               concat_len);  /* Line not necessary but cleaner: line can be removed to optimize performance */
        concat_len = 0;
        processed_len = 0;
      }
      else /* It can only happen if in the last part of the string there is un uncompleted Resp or Evt */
      {
        /* Processed_len covers the case where several msgs are in the pipe ATD_RecvBuf and last msg is not complete */
        /* Example if ATD_RecvBuf[] contains "+CW:CONNECTED\r\n+CW:GOT" the dispatcher process "+CW:CONNECTED\r\n" */
        /* But needs to wait for the missing "IP\r\n" before processing next msg */
        /* In such scenario processed_len will be 15 (i.e. 22 - 7) */
        processed_len = concat_len - remaining_len;
      }

      if (concat_len == RX_DATA_BUFFER_SIZE) /* No more space in the (Obj->ATD_RecvBuf). it can only happen if */
      {
        /* In the last part of the buffer there is un uncompleted Resp or Evt */
        /* In case of DATA remaining_len always 0 so also concat_len become 0 */
        memcpy(Obj->ATD_RecvBuf, Obj->ATD_RecvBuf + processed_len, remaining_len);
        concat_len = remaining_len;
        processed_len = 0;
        LogDebug("DBG: Obj->ATD_RecvBuf full, copy last fragmented message to the begin");
      }
    }
    else
    {
#if (SYS_DBG_ENABLE_TA4 >= 1)
      vTracePrintF("ATD_RxPooling_task", "rx_task: Exiting spisync_read: Sleep expired");
#endif /* SYS_DBG_ENABLE_TA4 */
    }
  }
}

void ATD_EventsPooling_task(void *arg)
{
  W61_Object_t *Obj = arg;
  int32_t x_next_event_len = 0;
  char event_type;

  while (1)
  {
#if (SYS_DBG_ENABLE_TA4 >= 1)
    vTracePrintF("ATD_EventsPooling_task", "event_task: Entering Sleep ");
#endif /* SYS_DBG_ENABLE_TA4 */

    x_next_event_len = xMessageBufferReceive(Obj->ATD_Evt_xMessageBuffer, Obj->EventsBuf,
                                             W61_ATD_CMDRSP_STRING_SIZE, EVENT_TIMEOUT);
    if (x_next_event_len > 0)
    {
      event_type = (char) Obj->EventsBuf[2];
      switch (event_type)
      {
        case 'P':  /* IPD (Net) */
          AT_Net_Event(Obj, Obj->EventsBuf, x_next_event_len);
          break;

        case 'I':  /* CIP (Net) */
          AT_Net_Event(Obj, Obj->EventsBuf, x_next_event_len);
          break;

        case 'Q':  /* MQTT */
          AT_MQTT_Event(Obj, Obj->EventsBuf, x_next_event_len);
          break;

        case 'L':  /* BLE */
          AT_Ble_Event(Obj, Obj->EventsBuf, x_next_event_len);
          break;

        case 'W':  /* CW (Wi-Fi) */
          AT_WiFi_Event(Obj, Obj->EventsBuf, x_next_event_len);
          break;

        default:
          LogDebug("WARN: Event not decoded correctly");
          break;
      }
    }
  }
}

int32_t ATrecv(W61_Object_t *Obj, uint8_t *pBuf, uint32_t len, uint32_t timeout_ms)
{
  /* This function reads a received AT message via an xMessageBuffer that synchronizes */
  /* The ATD_RxPooling_task with the task calling the W61 AT API (typically the application task). */
  /* Calling ATrecv() from ATD_RxPooling_task_handle() would cause the task to loop infinitely waiting for itself */
  /* That scenario would happen if users writing the application calls the W61 API functions on Event-callbacks */
  /* An ASSERT it is placed in the ATlock() function to prevent this to happen */
  /* It is assumed that ATrecv() is called only when the xCmdMutex is set by ATlock() */
  /* Internal check : code to be removed at release time, checking the semaphore reduces performance */
#if W61_CHECK_AT_LOCK
  W61_ASSERT(xTaskGetCurrentTaskHandle() !=  Obj->ATD_RxPooling_task_handle);
  W61_ASSERT(xSemaphoreTake(Obj->xCmdMutex, 1) == 0);
#endif /* W61_CHECK_AT_LOCK */
  return xMessageBufferReceive(Obj->ATD_Resp_xMessageBuffer, pBuf, len, pdMS_TO_TICKS(timeout_ms));
}

/* __weak functions in case w61_at_wifi.c, w61_at_net.c or w61_at_ble.c are not part of the project */

__attribute__((weak)) void AT_WiFi_Event(W61_Object_t *Obj, const uint8_t *p_evt, int32_t evt_len)
{
}

__attribute__((weak)) void AT_Net_Event(W61_Object_t *Obj, const uint8_t *p_evt, int32_t evt_len)
{
}

__attribute__((weak)) void AT_MQTT_Event(W61_Object_t *Obj, const uint8_t *p_evt, int32_t evt_len)
{
}

__attribute__((weak)) void AT_Ble_Event(W61_Object_t *Obj, const uint8_t *p_evt, int32_t evt_len)
{
}

/* Private Functions Definition ----------------------------------------------*/
static int32_t ATD_GetDataOffset(uint8_t *p_msg, int32_t string_len, int32_t param_len_offset)
{
  int32_t i;
  for (i = param_len_offset; i < string_len; i++) /* Don't go over the string_len received */
  {
    /* Make sure the whole number is received and not just a part of it, i.e. detect next comma */
    if (p_msg[i] == ',')
    {
      return (i + 1); /* Offset of the comma after the parameter length +1 (i.e. where data starts) */
    }
  }
  return 0;   /* Comma not found, parameter length is incomplete, cannot be extracted from the string yet */
}

static uint32_t ATD_CountCrlfAtBeginMsg(uint8_t *p_msg)
{
  uint32_t crlf_count = 0; /* count in case of several consecutive CRLF, e.i. if \r\n\r\n crlf_count=2 */
  uint32_t p_msg_len = strlen((char *)p_msg);
  if (p_msg_len > CRLF_SIZE)
  {
    while (1)
    {
      /* !IS_CHAR_SEND avoids to remove /r/n in from to ">", "/r/n>" is considered as a msg,
       * too risky consider only ">" */
      if (IS_CHAR_CR(p_msg[crlf_count]) && IS_CHAR_LF(p_msg[crlf_count + 1]) &&
          !IS_CHAR_SEND(p_msg[crlf_count + CRLF_SIZE]))
      {
        /* skip \r\n by incrementing the p_msg pointer by CRLF_SIZE */
        crlf_count += CRLF_SIZE;
      }
      else
      {
        break;
      }
    }
  }
  return crlf_count;
}

static int32_t ATD_ProcessDataHeader(uint8_t *p_msg, int32_t string_len, int8_t *Recv_Data_Mode,
                                     int32_t *Len_Extracted_From_Data_Header)
{
  uint32_t i;
  int32_t data_offset = 0;
  int32_t param_len_offset = 0;
  int32_t mqtt_topic_offset = 0;
  int32_t topic_len_extracted_from_mqtt_header = 0;

  for (i = 0; i < ITEMS_IN_DATA_EVT_LIST; i++)
  {
    /* TODO Temporary workaround skip HTTPCGET for HTT passive mode which should consider only HTTPRECVDATA */
    if (((ActivePassiveMode == 1) && ((i == 1) || (i == 2))) != true)
    {
      /* In case of fragmentation checking string_len is to avoid data existing
         in ATD_RecvBuf from previous command misleads the check */
      if (string_len >= AtDataEvtList[i].evtParamLenOffset + 2)
      {
        if (strncmp((char *) p_msg, AtDataEvtList[i].evtDataKeyword, AtDataEvtList[i].evtDataKeywordLen) == 0)
        {
          /* Data messages are the only that needs to be fully decoded by the AT_RxDispatcher_func() */
          *Len_Extracted_From_Data_Header = 0;
          param_len_offset = AtDataEvtList[i].evtParamLenOffset;
          if (AtDataEvtList[i].DataMode == RECV_DATA_MODE_MQTT)
          {
            mqtt_topic_offset = ATD_GetDataOffset(p_msg, string_len, param_len_offset);
            if (mqtt_topic_offset > 0) /* Check comma after parameter length is found */
            {
              /* Converting the number (topic_len_extracted_from_mqtt_header) only if comma is received */
              topic_len_extracted_from_mqtt_header = ParseNumber((char *) p_msg + param_len_offset, NULL);
              if (topic_len_extracted_from_mqtt_header < 0)
              {
                return -1;  /* Error wrong Data length conversion */
              }

              /* Increment the size of 3 to include the comma and the two quotes */
              topic_len_extracted_from_mqtt_header += 3;
              param_len_offset = mqtt_topic_offset; /* Update for next parameter */
            }
            else
            {
              return 0; /* Comma not found */
            }
          }

          data_offset = ATD_GetDataOffset(p_msg, string_len, param_len_offset);
          if (data_offset > 0) /* Check comma after parameter length is found */
          {
            /* Converting the number (*Len_Extracted_From_Data_Header) only if comma is received */
            *Len_Extracted_From_Data_Header = ParseNumber((char *) p_msg + param_len_offset, NULL);
            /* Prepare string to be sent to upper layer (xMessageBuf or Event) */
            memcpy(RetainRecvDataHeader, p_msg, MAX_SIZEOF_RECV_DATA_HEADER);
            if (*Len_Extracted_From_Data_Header >= 0)
            {
              *Recv_Data_Mode = AtDataEvtList[i].DataMode;
              *Len_Extracted_From_Data_Header += topic_len_extracted_from_mqtt_header;
            }
            else
            {
              return -1; /* Error wrong Data length conversion */
            }

#if (CIPRECVDATA_IP_PORT_REMOVAL_TMP_FIX  == 1)
            /* remove ip and port params after data_len in case of RECV_DATA_MODE_NET */
            if (AtDataEvtList[i].DataMode == RECV_DATA_MODE_NET)
            {
              uint32_t nr_of_specific_params = 2;
              int32_t next_param_offset;
              while (nr_of_specific_params > 0)
              {
                nr_of_specific_params--;
                next_param_offset = ATD_GetDataOffset(p_msg, string_len, data_offset);
                /* offset after next param or 0 if comma not reached (param fragmented) */
                data_offset = next_param_offset;
                if (data_offset == 0)
                {
                  break;  /* message arrived fragmented, i.e. data-header still incomplete, exit the loop */
                }
              }
            }
#endif /* CIPRECVDATA_IP_PORT_REMOVAL_TMP_FIX */
          }

          return data_offset;
        }
      }
    }
  }
  return 0;
}

static bool ATD_DetectEvent(W61_Object_t *Obj, uint8_t *p_evt, int32_t single_evt_length)
{
  uint32_t i = 0;

  for (i = 0; i < ITEMS_IN_EVT_LIST; i++)
  {
    /* Note strncmp() has been chosen instead of strstr() to avoid problems when messages arrive concatenated
     * Drawback is not being able to recover in case of unexpected (remaining) values
     * at the begin of the buffer (it should not happen)
     */
    if (strncmp((char *) p_evt, AtEvtList[i].evtDetectCharacters,
                AtEvtList[i].evtDetectCharactersLen) == 0)  /* Check if the event is in the list */
    {
      xMessageBufferSend(Obj->ATD_Evt_xMessageBuffer, p_evt, single_evt_length,
                         POSTING_X_MSG_TIMEOUT);
      return true;
    }
  }

  return false;
}

static int32_t ATD_RxDispatcher_func(W61_Object_t *Obj, uint8_t *p_input, int32_t string_len)
{
  int32_t single_msg_length = 0;
  int32_t remainLen = string_len; /* part of the string not yet processed */
  int32_t next_loop_index = 0;
  int32_t i;
  uint32_t unwanted_crlf_count; /* count in case of several consecutive CRLF, e.i. if \r\n\r\n crlf_count=2 */
  bool crlf_found;
  uint8_t *p_msg = p_input;
  uint32_t loops_count = 0; /* String cannot contain more the MAX_PARSER_LOOPS messages */
  int32_t data_offset = 0;
  int32_t app_buffer_remaining_space = 0;
  int32_t max_copy_len = 0;
  int32_t len_data_header_plus_crlf;

  while (remainLen > 0)
  {
    /* All flags are reset each loop cycle */
    loops_count++;  /* Loop to check if next message in the string */
    crlf_found = false;

    /* ------------------- Process the DATA ------------------------------------------------------------------------ */
    if (RecvDataMode != 0) /* Special case when pulling data (set by CheckIfStartDataEvent) */
    {
      if (string_len > data_offset) /* Check if DATA available at the p_input pointer in the ATD_RecvBuf buffer */
      {
        uint8_t *recv_data = NULL;

        /* Data_offset is always 0 a part when ATD_ProcessDataHeader() detects a data_header and loops */
        /* Copy the received data in the pointer provided by the application */
        max_copy_len = configMIN(string_len - data_offset, LenExtractedFromDataHeader - DataReceivedInPreviousStrings);

        switch (RecvDataMode)
        {
          case RECV_DATA_MODE_NET:
            app_buffer_remaining_space = Obj->NetCtx.AppBuffRecvDataSize - DataReceivedInPreviousStrings;
            recv_data = Obj->NetCtx.AppBuffRecvData;
            break;
          case RECV_DATA_MODE_HTTP:
            app_buffer_remaining_space = Obj->HTTPCtx.AppBuffRecvDataSize - DataReceivedInPreviousStrings;
            recv_data = Obj->HTTPCtx.AppBuffRecvData;
            break;
          case RECV_DATA_MODE_MQTT:
            app_buffer_remaining_space = Obj->MQTTCtx.AppBuffRecvDataSize - DataReceivedInPreviousStrings;
            recv_data = Obj->MQTTCtx.AppBuffRecvData;
            break;
          case RECV_DATA_MODE_BLE:
            app_buffer_remaining_space = Obj->BleCtx.AppBuffRecvDataSize - DataReceivedInPreviousStrings;
            recv_data = Obj->BleCtx.AppBuffRecvData;
            break;
          default:
            break;
        }

        if (recv_data != NULL)
        {
          if (app_buffer_remaining_space < max_copy_len)
          {
            max_copy_len = app_buffer_remaining_space;
            /* Notice that even in the application buffer there is no space to copy all received data, */
            /* All expected data (LenExtractedFromDataHeader) shall be received before exiting the RecvDataMode */
            LogInfo("Warning: Not enough space in the application buffer to copy all received data.");
          }
          memcpy(recv_data + DataReceivedInPreviousStrings, p_msg + data_offset, max_copy_len);
        }

        len_data_header_plus_crlf = LenExtractedFromDataHeader + strlen("\r\n");

        /* Check if all data (as expected from len_data_header_plus_crlf) has been received */
        if ((string_len - data_offset) < (len_data_header_plus_crlf - DataReceivedInPreviousStrings))
        {
          /* Not all data has been received */
          /* Update accumulation for next string and return */
          DataReceivedInPreviousStrings += (string_len - data_offset - next_loop_index);
          return 0; /* Return to the ATD_RxPooling_task to wait the rest of the data */
        }
        else /* All len_data_header_plus_crlf data received */
        {
          if ((RecvDataMode == RECV_DATA_MODE_NET) || (RecvDataMode == RECV_DATA_MODE_HTTP))
          {
            /* The Data Header message is forwarded as a RESP to the blocking CMD
             * via the Obj->ATD_Resp_xMessageBuffer */
            xMessageBufferSend(Obj->ATD_Resp_xMessageBuffer, RetainRecvDataHeader, MAX_SIZEOF_RECV_DATA_HEADER,
                               POSTING_X_MSG_TIMEOUT);
          }
          else if (RecvDataMode == RECV_DATA_MODE_MQTT)
          {
            /* The Data Header message is forwarded as EVENT to the ww61_at_mqtt.c by calling AT_Mqtt_Event() */
            AT_MQTT_Event(Obj, RetainRecvDataHeader, MAX_SIZEOF_RECV_DATA_HEADER);
          }
          else if (RecvDataMode == RECV_DATA_MODE_BLE)
          {
            /* The Data Header message is forwarded as EVENT to the w61_at_ble.c by calling AT_Ble_Event() */
            AT_Ble_Event(Obj, RetainRecvDataHeader, MAX_SIZEOF_RECV_DATA_HEADER);
          }
          RecvDataMode = 0;   /* All data received: switch back to Msg mode */
          /* Check if after data some other messages are in the pipe */
          if ((string_len - data_offset) > (len_data_header_plus_crlf - DataReceivedInPreviousStrings))
          {
            next_loop_index += data_offset + len_data_header_plus_crlf - DataReceivedInPreviousStrings; /* Loops */
          }
          else
          {
            return 0;  /* Nominal case: no other messages after DATA, return to the ATD_RxPooling_task */
          }
        }
      }
      else /* No data available */
      {
        return 0; /* Return to the ATD_RxPooling_task to wait the rest of the data */
      }
    }
    /* ------------------- Process the MESSAGES -------------------------------------------------------------------- */
    else /* (!RecvDataMode) Nominal case when receiving AT Messages */
    {
      /* First priority to check if new data arrived, by checking if the complete Data Header arrived and looping */
      /* By complete data header it means the keyword + the len + the comma, e.g. +CIPRECVDATA:<len>, */
      /* Otherwise the ATD_ProcessDataHeader function returns 0 */
      /* RecvDataMode and LenExtractedFromDataHeader are set by this function */

      /* \r\n at the begin of the string are removed, except "\r\n>" which does not have \r\n afterwards */
      unwanted_crlf_count = ATD_CountCrlfAtBeginMsg(p_msg);
      p_msg += unwanted_crlf_count;
      next_loop_index += unwanted_crlf_count;

      data_offset = ATD_ProcessDataHeader(p_msg, string_len, &RecvDataMode, &LenExtractedFromDataHeader);
      if (data_offset < 0) /* Error case, it should never happen */
      {
        LogError("Error wrong Data length conversion");
        return 0;  /*TODO should return negative ? , to study if need to manage the case or ignore */
      }
      if (data_offset > 0) /* Data_offset != 0 means a new complete DataHeader has been found */
      {
        /* DataReceivedInPreviousStrings is reset and code loops to retrieve the data */
        DataReceivedInPreviousStrings = 0;
      }
      else /* String content is not a "DataHeader" or the "DataHeader" is incomplete */
      {
        /* Below code checks if message is complete by searching \r\n */
        /* if message ("DataHeader", Event or Response) is incomplete, function returns to the ATD_RxPooling_task */
        /* if message is a special case "\r\n>" it calls xMessageBufferSend(Obj->ATD_Resp_xMessageBuffer, ...) */
        /* if message is an event it ends up calling xMessageBufferSend(Obj->ATD_Evt_xMessageBuffer, ...) */
        /* if message is a response it ends up calling xMessageBufferSend(Obj->ATD_Resp_xMessageBuffer, ...) */
        if (IS_CHAR_CR(p_msg[0]) && IS_CHAR_LF(p_msg[1]) && IS_CHAR_SEND(p_msg[2]))
        {
          single_msg_length = strlen("\r\n>");
          xMessageBufferSend(Obj->ATD_Resp_xMessageBuffer, p_msg, single_msg_length, POSTING_X_MSG_TIMEOUT);
          next_loop_index += strlen("\r\n>");
        }
        else
        {
          /* Events or responses have always at least CRLF_SEARCH_OFFSET characters before ending \r\n */
          for (i = CRLF_SEARCH_OFFSET; i < (string_len - next_loop_index); i++)
          {
            /* Search the end of the message to be sure msg line is complete */
            /* Starting with i > 1 avoids the "/r/n before >" to be considered as message completed symbol */
            if (IS_CHAR_CR(p_msg[i - 1]) && IS_CHAR_LF(p_msg[i]))
            {
              crlf_found = true;
              break;
            }
          }
          if (crlf_found) /* \r\n found, message is complete */
          {
            single_msg_length = (i + 1);
            /* Check if the message is an Event, in such case it will be queued in the Obj->ATD_Evt_xMessageBuffer */
            if (!ATD_DetectEvent(Obj, p_msg, single_msg_length))
            {
              /* it was not an Event so it is a response and it is queued in the Obj->ATD_Resp_xMessageBuffer */
              xMessageBufferSend(Obj->ATD_Resp_xMessageBuffer, p_msg, single_msg_length, POSTING_X_MSG_TIMEOUT);
            }
            next_loop_index += (i + 1);  /* +1 because index start from zero +1 to position after LF */ /* Loops */
          }
          else /* Crlf not found */
          {
            /* Return to the ATD_RxPooling_task to wait the rest of the message */
            return string_len - next_loop_index;
          }
        }

      }
#if (SYS_DBG_ENABLE_TA4 >= 1)
      vTracePrintF("AT_RxDispatcher_func", "rx_task: Send to Response xMessageBuffer");
#endif /* SYS_DBG_ENABLE_TA4 */
    }

    /* Execution can get up to here either if CR-LF was found,
     * in such case it checks if other messages are in the pipe
     * Or when all DATA is arrived but the string is not completely parsed (other messages behind it)
     */
    remainLen = string_len - next_loop_index;
    if (remainLen > 0)
    {
      /* Update pointer to next message */
      p_msg = &p_input[next_loop_index];
    }
    if (loops_count > MAX_PARSER_LOOPS)
    {
      LogError("Error ATD_RxDispatcher_func() is stuck in the while loop");
      return 0;
    }
  }
  return 0;
}

/** @} */

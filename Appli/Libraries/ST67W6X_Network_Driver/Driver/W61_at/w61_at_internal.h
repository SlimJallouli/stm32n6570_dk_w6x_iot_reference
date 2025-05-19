/**
  ******************************************************************************
  * @file    w61_at_internal.h
  * @author  GPM Application Team
  * @brief   This file provides the internal definitions of the AT driver
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
#ifndef W61_AT_INTERNAL_H
#define W61_AT_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include <string.h>

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/** @addtogroup ST67W61_AT_Common_Macros
  * @{
  */

/** Macro to check if the pointer is NULL and return the error code */
#define W61_NULL_ASSERT(p) if ((p) == NULL) { return ret; }

/** Macro to check if the pointer is NULL and return the error code with error string log */
#define W61_NULL_ASSERT_STR(p, s) if ((p) == NULL) { LogError("%s", (s)); return ret; }

/** @} */

/* Exported functions ------------------------------------------------------- */
/**
  * @brief  Parses WiFi event and call related callback.
  * @param  Obj: pointer to module handle
  * @param  p_evt: pointer to event buffer
  * @param  evt_len: event length
  */
void AT_WiFi_Event(W61_Object_t *Obj, const uint8_t *p_evt, int32_t evt_len);

/**
  * @brief  Parses WiFi Network event and call related callback.
  * @param  Obj: pointer to module handle
  * @param  p_evt: pointer to event buffer
  * @param  evt_len: event length
  */
void AT_Net_Event(W61_Object_t *Obj, const uint8_t *p_evt, int32_t evt_len);

/**
  * @brief  Parses MQTT event and call related callback.
  * @param  Obj: pointer to module handle
  * @param  p_evt: pointer to event buffer
  * @param  evt_len: event length
  */
void AT_MQTT_Event(W61_Object_t *Obj, const uint8_t *p_evt, int32_t evt_len);

/**
  * @brief  Parses BLE event and call related callback.
  * @param  Obj: pointer to module handle
  * @param  p_evt: pointer to event buffer
  * @param  evt_len: event length
  */
void AT_Ble_Event(W61_Object_t *Obj, const uint8_t *p_evt, int32_t evt_len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*W61_AT_INTERNAL_H */

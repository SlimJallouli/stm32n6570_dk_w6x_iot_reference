/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : ping.h
 * @brief          : TCP‑based connectivity check task
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

#ifndef _PING_H_
#define _PING_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Exported function prototypes --------------------------------------------- */

/**
 * @brief FreeRTOS task performing TCP‑based connectivity checks.
 *
 * Call with:
 *     xTaskCreate(ping_task, "PingTask", stack, NULL, prio, NULL);
 */
void ping_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* _PING_H_ */

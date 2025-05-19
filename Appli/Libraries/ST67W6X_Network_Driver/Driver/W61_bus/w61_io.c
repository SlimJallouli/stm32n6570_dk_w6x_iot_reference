/**
  ******************************************************************************
  * @file    w61_io.c
  * @author  GPM Application Team
  * @brief   This file provides the IO operations to deal with the STM32W61 module.
  *          It mainly initialize and de-initialize the SPI interface.
  *          Send and receive data over it.
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
#include "logging.h"
#include "w61_io.h"
#include "FreeRTOS.h"
#include "task.h"

/* Global variables ----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
#define SPISYNC_WAIT_TIMEOUT_MS   (3000)

/* Private macros ------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Functions Definition ------------------------------------------------------*/
int32_t BusIo_SPI_Init(void)
{
  return spi_transaction_init();
}

int32_t BusIo_SPI_DeInit(void)
{
  return 0;
}

void BusIo_SPI_Delay(uint32_t Delay)
{
  vTaskDelay(pdMS_TO_TICKS(Delay));
}

int32_t BusIo_SPI_Send(uint8_t *pBuf, uint16_t length, uint32_t Timeout)
{
  struct spi_msg m_t;
  SPI_MSG_INIT(m_t, pBuf, length, NULL, 0);
  return spi_write(&m_t, Timeout);
}

int32_t BusIo_SPI_Receive(uint8_t *pBuf, uint16_t length, uint32_t Timeout)
{
  struct spi_msg m_t;
  SPI_MSG_INIT(m_t, pBuf, length, NULL, 0);
  return spi_read(&m_t, Timeout);
}

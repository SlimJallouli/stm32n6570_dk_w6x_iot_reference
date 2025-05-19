/**
  ******************************************************************************
  * @file    spi_port.c
  * @brief   SPI bus interface porting layer implementation
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

/**
  * This file is based on QCC74xSDK provided by Qualcomm, which is licensed under the BSD-3-Clause license,
  * that can be found in the LICENSE_SOURCES file.
  * See https://git.codelinaro.org/clo/qcc7xx/QCCSDK-QCC74x for more information.
  *
  * Reference source: examples/stm32_spi_host/QCC743_SPI_HOST/src/spi_port.c
  */

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "app_freertos.h"
#include "app_config.h"

#include "spi_port.h"
#include "main.h"

/* Global variables ----------------------------------------------------------*/
extern SPI_HandleTypeDef w6x_spi;

/* Private variables ---------------------------------------------------------*/
static spi_transaction_complete_t spi_port_transaction_complete_cb = NULL;

/* Functions Definition ------------------------------------------------------*/
#ifdef __ICCARM__
void *spi_port_memcpy(void *dest, const void *src, unsigned int len)
{
  __asm volatile(
    /* Initialize local variables. */
    "MOV r0, %0\n"
    "MOV r1, %1\n"
    "MOV r2, %2\n"

    /* Nothing to do if length is 0. */
    "CMP r2, #0\n"
    "BEQ 6f\n"

    /* Handle unaligned destination address. */
    "ANDS r3, r0, #3\n"
    "BEQ 2f\n"

    /* Byte copy until the destination address is aligned. */
    "1:\n"
    "LDRB r3, [r1], #1\n"
    "STRB r3, [r0], #1\n"
    "SUBS r2, r2, #1\n"
    "BEQ 6f\n"
    "ANDS r3, r0, #3\n"
    "BNE 1b\n"

    /* Now handle word copy if length >= 4 */
    "2:\n"
    "CMP r2, #4\n"
    "BLT 4f\n"

    "3:\n"
    "LDR r3, [r1], #4\n"
    "STR r3, [r0], #4\n"
    "SUBS r2, r2, #4\n"
    "CMP r2, #4\n"
    "BGE 3b\n"

    /* Handle remaining bytes (if any). */
    "4:\n"
    "ANDS r3, r2, #3\n"
    "BEQ 6f\n"
    "5:\n"
    "LDRB r3, [r1], #1\n"
    "STRB r3, [r0], #1\n"
    "SUBS r2, r2, #1\n"
    "BNE 5b\n"

    /* Done. */
    "6:\n"
    :
    : "r"(dest), "r"(src), "r"(len)
    : "r0", "r1", "r2", "r3", "memory"
  );

  return dest;
}
#endif /* __ICCARM__ */

#ifdef __GNUC__
void *memcpy(void *dest, const void *src, unsigned int len)
{
  __asm__ volatile(
    /* Initialize local variables. */
    "mov r0, %[dest]              \n"
    "mov r1, %[src]               \n"
    "mov r2, %[len]               \n"

    /* Nothing to do if length is 0. */
    "cmp r2, #0                   \n"
    "beq 6f                       \n"

    /* Handle unaligned destination address. */
    "ands r3, r0, #3              \n"
    "beq 2f                       \n"

    /* Byte copy until the destination address is aligned. */
    "1:                           \n"
    "ldrb r3, [r1], #1            \n"
    "strb r3, [r0], #1            \n"
    "subs r2, r2, #1              \n"
    "beq 6f                       \n"
    "ands r3, r0, #3              \n"
    "bne 1b                       \n"

    /* Now handle word copy if length >= 4 */
    "2:                           \n"
    "cmp r2, #4                   \n"
    "blt 4f                       \n"

    "3:                           \n"
    "ldr r3, [r1], #4             \n"
    "str r3, [r0], #4             \n"
    "subs r2, r2, #4              \n"
    "cmp r2, #4                   \n"
    "bge 3b                       \n"

    /* Handle remaining bytes (if any). */
    "4:                           \n"
    "ands r3, r2, #3              \n"
    "beq 6f                       \n"
    "5:                           \n"
    "ldrb r3, [r1], #1            \n"
    "strb r3, [r0], #1            \n"
    "subs r2, r2, #1              \n"
    "bne 5b                       \n"

    /* Done. */
    "6:                           \n"
    :
    : [dest] "r"(dest), [src] "r"(src), [len] "r"(len)
    : "r0", "r1", "r2", "r3", "memory"
  );

  return dest;
}
#endif /* __GNUC__ */

int32_t spi_port_init(spi_transaction_complete_t transaction_complete_cb)
{
  /* Powering up the NCP using GPIO CHIP_EN */
  HAL_GPIO_WritePin(CHIP_EN_GPIO_Port, CHIP_EN_Pin, GPIO_PIN_SET);

  if (transaction_complete_cb != NULL)
  {
    spi_port_transaction_complete_cb = transaction_complete_cb;
  }

  return 0;
}

int32_t spi_port_transfer(void *tx_buf, void *rx_buf, uint16_t len, uint32_t timeout)
{
  HAL_StatusTypeDef status;

#if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
  SCB_CleanInvalidateDCache_by_Addr(rx_buf, len);
#endif /* __DCACHE_PRESENT */

  /* Check whether host data is to be transmitted to the NCP, otherwise read only data from the NCP */
  if (tx_buf != NULL)
  {
#if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    SCB_CleanDCache_by_Addr(tx_buf, len);
#endif /* __DCACHE_PRESENT */
    status = HAL_SPI_TransmitReceive(&w6x_spi, tx_buf, rx_buf, len, timeout);
  }
  else
  {
    status = HAL_SPI_Receive(&w6x_spi, rx_buf, len, timeout);
  }

  return (status == HAL_OK ? 0 : -1);
}

int32_t spi_port_transfer_dma(void *tx_buf, void *rx_buf, uint16_t len)
{
  HAL_StatusTypeDef status;

#if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
  SCB_CleanInvalidateDCache_by_Addr(rx_buf, len);
#endif /* __DCACHE_PRESENT */

  /* Check whether host data is to be transmitted to the NCP, otherwise read only data from the NCP */
  if (tx_buf != NULL)
  {
#if defined (__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    SCB_CleanDCache_by_Addr(tx_buf, len);
#endif /* __DCACHE_PRESENT */
    status = HAL_SPI_TransmitReceive_DMA(&w6x_spi, tx_buf, rx_buf, len);
  }
  else
  {
    status = HAL_SPI_Receive_DMA(&w6x_spi, rx_buf, len);
  }
  DisableSuppressTicksAndSleep(1 << CFG_LPM_SPIIF_ID);

  return (status == HAL_OK ? 0 : -1);
}

int32_t spi_port_is_ready(void)
{
  /* Check whether NCP data are available on the SPI bus */
  return (int32_t)HAL_GPIO_ReadPin(SPI_SLAVE_DATA_RDY_GPIO_Port, SPI_SLAVE_DATA_RDY_Pin);
}

#define LOOP_NB_INSTR 6
#define WAIT_US(us) do{                                                                                         \
                        for (__IO int32_t i = ((us)*(SystemCoreClock / 1000000)) / (LOOP_NB_INSTR); i > 0; i--) \
                        {}                                                                                      \
                      } while (0)

int32_t spi_port_set_cs(int32_t state)
{
  if (state)
  {
    /* Activate Chip Select before starting transfer */
    HAL_GPIO_WritePin(LP_WAKEUP_GPIO_Port, LP_WAKEUP_Pin, GPIO_PIN_SET);
    WAIT_US(20);
  }
  else
  {
    /* Disable Chip Select when transfer is complete */
    WAIT_US(20);
    HAL_GPIO_WritePin(LP_WAKEUP_GPIO_Port, LP_WAKEUP_Pin, GPIO_PIN_RESET);
  }

  return 0;
}

uint32_t spi_port_itm(uint32_t ch)
{
#if 0
  return ITM_SendChar(ch);
#endif /* 0 */
  return 0;
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  EnableSuppressTicksAndSleep(1 << CFG_LPM_SPIIF_ID);
  spi_port_transaction_complete_cb();
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
  EnableSuppressTicksAndSleep(1 << CFG_LPM_SPIIF_ID);
  spi_port_transaction_complete_cb();
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
  EnableSuppressTicksAndSleep(1 << CFG_LPM_SPIIF_ID);
  spi_port_transaction_complete_cb();
}

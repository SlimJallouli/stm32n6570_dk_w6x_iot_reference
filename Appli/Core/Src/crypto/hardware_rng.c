/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    hardware_rng.c
 * @author  GPM Application Team
 * @version V1.2.1
 * @date    14-April-2017
 * @brief   mbedtls alternate entropy data function.
 *          the mbedtls_hardware_poll() is customized to use the STM32 RNG
 *          to generate random data, required for TLS encryption algorithms.
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
#include MBEDTLS_CONFIG_FILE

#include "main.h"
#include "string.h"

#if defined(__USE_STSAFE__)
extern uint8_t SAFEA1_GenerateRandom(uint8_t size, uint8_t *random);

UBaseType_t uxRand(void)
{
  // Return a secure random value that is uniformly-distributed.
  uint32_t uRNGValue = 0;
  uint8_t status;

  status = SAFEA1_GenerateRandom(4, (uint8_t *)&uRNGValue);

  configASSERT(status);

  return (UBaseType_t)uRNGValue;
}
#else
extern RNG_HandleTypeDef hrng;
extern HAL_StatusTypeDef HAL_RNG_GenerateRandomNumber(RNG_HandleTypeDef *hrng, uint32_t *random32bit);

UBaseType_t uxRand(void)
{
  // Return a secure random value that is uniformly-distributed.
  uint32_t uRNGValue = 0;
  (void) HAL_RNG_GenerateRandomNumber(&hrng, &uRNGValue);

  return (UBaseType_t)uRNGValue;
}
#endif

#if defined(MBEDTLS_ENTROPY_HARDWARE_ALT)
int mbedtls_hardware_poll(void *Data,
                          unsigned char *Output,
                          size_t Len,
                          size_t *oLen)
{
    size_t produced = 0;

    while (produced < Len)
    {
        uint32_t rnd = uxRand();

        /* Write up to 4 bytes from rnd into Output */
        for (size_t i = 0; i < 4 && produced < Len; i++)
        {
            Output[produced++] = (unsigned char)(rnd >> (i * 8));
        }
    }

    *oLen = produced;
    return 0;
}
#endif


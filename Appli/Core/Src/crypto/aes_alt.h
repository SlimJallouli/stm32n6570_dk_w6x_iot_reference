/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    aes_alt.h
 * @author  GPM Application Team
 * @version V1.0
 * @date    31-March-2026
 * @brief   mbedTLS AES hardware acceleration context definition.
 *          Provides alternate context structure for STM32N6570 hardware
 *          AES offloading (ECB and CBC modes supported).
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
#ifndef MBEDTLS_AES_ALT_H
#define MBEDTLS_AES_ALT_H

#include MBEDTLS_CONFIG_FILE

#if defined(MBEDTLS_AES_C) && defined(MBEDTLS_AES_ALT)

typedef struct
{
    uint32_t aes_key[8];
    uint32_t keybits;
} mbedtls_aes_context;

#endif /* MBEDTLS_AES_ALT */
#endif /* MBEDTLS_AES_ALT_H */

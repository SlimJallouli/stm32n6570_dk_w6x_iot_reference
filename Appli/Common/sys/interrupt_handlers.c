/*
 * FreeRTOS STM32 Reference Integration
 *
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#include "FreeRTOS.h"
#include "task.h"
#include "interrupt_handlers.h"
//typedef void ( * GPIOInterruptCallback_t ) ( void * pvContext );

static GPIOInterruptCallback_t volatile xGpio_RisingCallbacks[ 16 ] = { NULL };
static void * volatile xGpio_RisingCallbackContext[ 16 ] = { NULL };

static GPIOInterruptCallback_t volatile xGpio_FallingCallbacks[ 16 ] = { NULL };
static void * volatile xGpio_FallingCallbackContext[ 16 ] = { NULL };

/* STM32U5xx Peripheral Interrupt Handlers */

/*
 * @brief Register a callback function for a given gpio.
 * @param usGpioPinMask The target gpio pin's bitmask
 * @param pvCallback Callback function pointer
 * @param pvContext User provided context pointer
 */
void GPIO_EXTI_Register_Callback( uint16_t usGpioPinMask, GPIOInterruptCallback_t pvCallback, void * pvContext )
{
    uint32_t ulIndex = POSITION_VAL( usGpioPinMask );

    configASSERT( ulIndex < 16 );

    xGpio_RisingCallbacks[ ulIndex ] = pvCallback;
    xGpio_RisingCallbackContext[ ulIndex ] = pvContext;
}

/*
 * @brief Register a callback function for a given gpio.
 * @param usGpioPinMask The target gpio pin's bitmask
 * @param pvCallback Callback function pointer
 * @param pvContext User provided context pointer
 */
void GPIO_EXTI_Register_Rising_Callback( uint16_t usGpioPinMask, GPIOInterruptCallback_t pvCallback, void * pvContext )
{
    uint32_t ulIndex = POSITION_VAL( usGpioPinMask );

    configASSERT( ulIndex < 16 );

    xGpio_RisingCallbacks[ ulIndex ] = pvCallback;
    xGpio_RisingCallbackContext[ ulIndex ] = pvContext;
}

/*
 * @brief Register a callback function for a given gpio.
 * @param usGpioPinMask The target gpio pin's bitmask
 * @param pvCallback Callback function pointer
 * @param pvContext User provided context pointer
 */
void GPIO_EXTI_Register_Falling_Callback( uint16_t usGpioPinMask, GPIOInterruptCallback_t pvCallback, void * pvContext )
{
    uint32_t ulIndex = POSITION_VAL( usGpioPinMask );

    configASSERT( ulIndex < 16 );

    xGpio_FallingCallbacks[ ulIndex ] = pvCallback;
    xGpio_FallingCallbackContext[ ulIndex ] = pvContext;
}

/**
 * @brief  EXTI line rising detection callback.
 * @param  GPIO_Pin: Specifies the port pin connected to Thank y EXTI line.
 * @retval None
 */
void HAL_GPIO_EXTI_Falling_Callback( uint16_t usGpioPinMask )
{
  uint32_t ulIndex = POSITION_VAL( usGpioPinMask );

  if( xGpio_FallingCallbacks[ ulIndex ] != NULL )
  {
      ( *( xGpio_FallingCallbacks[ ulIndex ] ) )( xGpio_FallingCallbackContext[ ulIndex ] );
  }
}

/**
 * @brief  EXTI line rising detection callback.
 * @param  GPIO_Pin: Specifies the port pin connected to corresponding EXTI line.
 * @retval None
 */
void HAL_GPIO_EXTI_Rising_Callback( uint16_t usGpioPinMask )
{
    uint32_t ulIndex = POSITION_VAL( usGpioPinMask );

    if( xGpio_RisingCallbacks[ ulIndex ] != NULL )
    {
        ( *( xGpio_RisingCallbacks[ ulIndex ] ) )( xGpio_RisingCallbackContext[ ulIndex ] );
    }
}

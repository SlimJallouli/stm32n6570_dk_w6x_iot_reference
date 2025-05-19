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

#ifndef _XSPI_NOR_DRV
#define _XSPI_NOR_DRV

#if defined(HAL_XSPI_MODULE_ENABLED)
/*
 *  512 Mbit = 64 MByte
 *  1024 Blocks of 64KByte
 *  16 4096 byte Sectors per Block
 */
#define MX66LM_RESERVED_BLOCKS       ( 64 )
#define MX66LM_BLOCK_SZ              ( 64 * 1024 )
#define MX66LM_SECTOR_SZ             ( 4  * 1024 )
#define MX66LM_NUM_BLOCKS            ( 1024 )
#define MX66LM_SECTORS_PER_BLOCK     ( 16 )
#define MX66LM_NUM_SECTORS           ( MX66LM_NUM_BLOCKS * MX66LM_SECTORS_PER_BLOCK )
#define MX66LM_MEM_SZ_BYTES          ( MX66LM_NUM_BLOCKS * MX66LM_BLOCK_SZ )

#define XPI_START_ADDRESS            ( MX66LM_RESERVED_BLOCKS * MX66LM_BLOCK_SZ )

#define MX66LM_NUM_SECTOR_USABLE     ( MX66LM_NUM_BLOCKS - MX66LM_RESERVED_BLOCKS )
#define MX66LM_MEM_SZ_USABLE         ( MX66LM_NUM_SECTOR_USABLE * MX66LM_SECTOR_SZ )

#if 1
#define MX66LM_DEFAULT_TIMEOUT_MS    ( 1000 )


#define MX66LM_8READ_DUMMY_CYCLES    ( 20 )

/* SPI mode command codes */
#define MX66LM_SPI_WREN              ( 0x06 )
#define MX66LM_SPI_WRCR2             ( 0x72 )
#define MX66LM_SPI_RDSR              ( 0x05 )

/* CR2 register definition */
#define MX66LM_REG_CR2_0_SPI         ( 0x00 )
#define MX66LM_REG_CR2_0_SOPI        ( 0x01 )
#define MX66LM_REG_CR2_0_DOPI        ( 0x02 )

#define MX66LM_REG_SR_WIP            ( 0x01 )   /* Write in progress  */
#define MX66LM_REG_SR_WEL            ( 0x02 )   /* Write enable latch */

/* OPI mode commands */
#define MX66LM_OPI_RDSR              ( 0x05FA )
#define MX66LM_OPI_WREN              ( 0x06F9 )
#define MX66LM_OPI_8READ             ( 0xEC13 )
#define MX66LM_OPI_PP                ( 0x12ED ) /* Page Program, starting address must be 0 in DTR OPI mode */
#define MX66LM_PROGRAM_FIFO_LEN      ( 256 )
#define MX66LM_OPI_SE                ( 0x21DE ) /* Sector Erase */

#define MX66LM_WRITE_TIMEOUT_MS      ( 10 * 1000 )
#define MX66LM_ERASE_TIMEOUT_MS      ( 10 * 1000 )
#define MX66LM_READ_TIMEOUT_MS       ( 10 * 1000 )
#endif

#endif /* defined(HAL_XSPI_MODULE_ENABLED)  */
#endif /* _XSPI_NOR_DRV */

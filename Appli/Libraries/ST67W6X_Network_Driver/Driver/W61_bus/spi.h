/**
  ******************************************************************************
  * @file    spi.h
  * @brief   SPI bus interface definition
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
  * Reference source: examples/stm32_spi_host/QCC743_SPI_HOST/inc/spi.h
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SPI_H
#define SPI_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
struct spi_msg
{
  void *data;
  uint32_t data_len;
  void *ctrl;
  uint32_t ctrl_len;
  uint32_t flags;
};

struct spi_stat
{
  uint64_t tx_pkts;
  uint64_t tx_bytes;
  uint64_t rx_pkts;
  uint64_t rx_bytes;
  uint64_t rx_drop;
  uint64_t io_err;
  uint64_t hdr_err;
  uint64_t wait_txn_timeouts;
  uint64_t wait_msg_xfer_timeouts;
  uint64_t wait_hdr_ack_timeouts;
  uint64_t mem_err;
  uint64_t rx_stall;
};

struct spi_buffer
{
  void *data;
  /* Length of the data. */
  uint32_t len;
  /* Capacity of the buffer, >= len. */
  uint32_t cap;
#define SPI_BUF_F_PUSHED  0x1
  uint32_t flags;
};

/* Exported constants --------------------------------------------------------*/
#define SPI_MSG_F_TRUNCATED 0x1

/** Maximum SPI buffer size */
#define SPI_XFER_MTU_BYTES        (6 * 1024)

/* Exported macro ------------------------------------------------------------*/
#define SPI_MSG_INIT(m, d, dl, c, cl) do {                            \
                                           struct spi_msg *_m = &(m); \
                                             _m->data = d;            \
                                             _m->data_len = dl;       \
                                             _m->ctrl = c;            \
                                             _m->ctrl_len = cl;       \
                                             _m->flags = 0;           \
                                         } while (0)

/* Exported functions ------------------------------------------------------- */
int32_t spi_transaction_init(void);

struct spi_buffer *spi_buffer_alloc(uint32_t size, uint32_t reserve);

struct spi_buffer *spi_tx_buffer_alloc(uint32_t size);

void spi_buffer_free(struct spi_buffer *buf);

static inline void *spi_buffer_data(struct spi_buffer *buf)
{
  return buf->data;
}

static inline uint32_t spi_buffer_len(struct spi_buffer *buf)
{
  return buf->len;
}

int32_t spi_read(struct spi_msg *msg, int32_t timeout_ms);

/* Caller is supposed to free the buffer by calling spi_buffer_free. */
int32_t spi_read_buffer(struct spi_buffer **buffer, int32_t timeout_ms);

int32_t spi_write(struct spi_msg *msg, int32_t timeout_ms);

int32_t spi_write_buffer(struct spi_buffer *buffer, int32_t timeout_ms);

int32_t spi_wait_event(void *evnt_ctx, uint32_t event, int32_t timeout_ms);

void spi_show_throutput_enable(int32_t en);

int32_t spi_on_transaction_ready(void);

int32_t spi_on_txn_data_ready(void);

int32_t spi_on_header_ack(void);

void spi_dump(void);

int32_t spi_get_stats(struct spi_stat *stat);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SPI_H */

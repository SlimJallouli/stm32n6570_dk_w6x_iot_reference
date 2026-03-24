#ifndef STM32_SHA256_ALT_H
#define STM32_SHA256_ALT_H

#include <stddef.h>
#include <stdint.h>

/*
 * SHA256 ALT context for STM32 hardware-accelerated hashing.
 *
 * We keep total[], state[], buffer[], and is224 to match the original
 * mbedtls_sha256_context layout expectations for mbedTLS itself, but
 * all hashing is implemented as:
 *
 *   - Buffered message accumulation in hw_msg_buf
 *   - One-shot HAL_HASH_Start() in mbedtls_sha256_finish()
 *
 * Incremental HAL HASH (suspend/resume, Accumulate) is intentionally
 * not used because it is not reliable for chunked updates and breaks
 * TLS transcript hashing and X.509 verification in real workloads.
 */
typedef struct mbedtls_sha256_context
{
    uint32_t total[2];          /*!< The number of Bytes processed.  */
    uint32_t state[8];          /*!< The intermediate digest state.  */
    unsigned char buffer[64];   /*!< The data block being processed. */
    int is224;                  /*!< 0: SHA-256, 1: SHA-224.        */

    /* Buffered hardware mode: message storage for one-shot HAL hashing. */
    uint8_t *hw_msg_buf;
    size_t   hw_msg_len;
    size_t   hw_msg_cap;
}
mbedtls_sha256_context;

#endif /* STM32_SHA256_ALT_H */

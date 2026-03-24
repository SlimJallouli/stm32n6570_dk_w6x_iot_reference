#ifndef STM32_SHA256_ALT_H
#define STM32_SHA256_ALT_H

#include <stddef.h>
#include <stdint.h>

/*
 * Match the original mbedtls_sha256_context layout so that any
 * existing code that relies on sizeof/clone/etc. stays valid.
 *
 * We only actively use total[] and buffer[] in the HW offload,
 * but keeping state[] and is224 preserves ABI compatibility.
 */
typedef struct mbedtls_sha256_context
{
    uint32_t total[2];          /*!< The number of Bytes processed.  */
    uint32_t state[8];          /*!< The intermediate digest state.  */
    unsigned char buffer[64];   /*!< The data block being processed. */
    int is224;                  /*!< 0: SHA-256, 1: SHA-224.        */
}
mbedtls_sha256_context;

#endif /* STM32_SHA256_ALT_H */

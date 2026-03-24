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

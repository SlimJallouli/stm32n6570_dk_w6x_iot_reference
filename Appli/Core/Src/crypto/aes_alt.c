#include "mbedtls/aes.h"
#include "mbedtls/error.h"

#include "stm32n6xx_hal.h"
#include "stm32n6xx_hal_cryp.h"
#include <string.h>

#if defined(MBEDTLS_AES_C) && defined(MBEDTLS_AES_ALT)

#define AES_TIMEOUT 0xFFU

/* mbedtls_aes_context is expected to be:
 *
 * typedef struct {
 *     uint32_t aes_key[8];
 *     uint32_t keybits;
 * } mbedtls_aes_context;
 */

extern CRYP_HandleTypeDef hcryp;

void mbedtls_aes_init( mbedtls_aes_context *ctx )
{
    if( ctx == NULL )
        return;

    memset( ctx, 0, sizeof( *ctx ) );
}

void mbedtls_aes_free( mbedtls_aes_context *ctx )
{
    if( ctx == NULL )
        return;

    memset( ctx, 0, sizeof( *ctx ) );
}

static int aes_set_key( mbedtls_aes_context *ctx,
                        const unsigned char *key,
                        unsigned int keybits )
{
    if( ctx == NULL || key == NULL )
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;

    if( keybits != 128U && keybits != 256U )
        return MBEDTLS_ERR_AES_INVALID_KEY_LENGTH;

    ctx->keybits = keybits;

    /* Convert key to 32‑bit words (big‑endian) */
    for( unsigned i = 0; i < keybits / 32U; i++ )
    {
        ctx->aes_key[i] =
              ( (uint32_t) key[4U * i    ] << 24 )
            | ( (uint32_t) key[4U * i + 1] << 16 )
            | ( (uint32_t) key[4U * i + 2] <<  8 )
            | ( (uint32_t) key[4U * i + 3]       );
    }

    return 0;
}

int mbedtls_aes_setkey_enc( mbedtls_aes_context *ctx,
                            const unsigned char *key,
                            unsigned int keybits )
{
    return aes_set_key( ctx, key, keybits );
}

int mbedtls_aes_setkey_dec( mbedtls_aes_context *ctx,
                            const unsigned char *key,
                            unsigned int keybits )
{
    /* Same key material; direction handled by CRYP */
    return aes_set_key( ctx, key, keybits );
}

int mbedtls_aes_crypt_ecb( mbedtls_aes_context *ctx,
                           int mode,
                           const unsigned char input[16],
                           unsigned char output[16] )
{
    if( ctx == NULL || input == NULL || output == NULL )
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;

    if( mode != MBEDTLS_AES_ENCRYPT && mode != MBEDTLS_AES_DECRYPT )
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;

    CRYP_ConfigTypeDef cfg;
    memset( &cfg, 0, sizeof( cfg ) );

    cfg.DataType      = CRYP_BYTE_SWAP;
    cfg.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
    cfg.KeySize       = ( ctx->keybits == 128U ) ? CRYP_KEYSIZE_128B
                                                 : CRYP_KEYSIZE_256B;
    cfg.pKey          = ctx->aes_key;
    cfg.pInitVect     = NULL;
    cfg.Algorithm     = CRYP_AES_ECB;

    if( HAL_CRYP_DeInit( &hcryp ) != HAL_OK )
        return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;

    hcryp.Instance = CRYP;
    if( HAL_CRYP_Init( &hcryp ) != HAL_OK )
        return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;

    if( HAL_CRYP_SetConfig( &hcryp, &cfg ) != HAL_OK )
        return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;

    HAL_StatusTypeDef st;

    if( mode == MBEDTLS_AES_ENCRYPT )
    {
        st = HAL_CRYP_Encrypt( &hcryp,
                               (uint32_t *) input,
                               16U,
                               output,
                               HAL_MAX_DELAY );
    }
    else
    {
        st = HAL_CRYP_Decrypt( &hcryp,
                               (uint32_t *) input,
                               16U,
                               output,
                               HAL_MAX_DELAY );
    }

    return ( st == HAL_OK ) ? 0 : MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
}

#if defined(MBEDTLS_CIPHER_MODE_CBC)
int mbedtls_aes_crypt_cbc( mbedtls_aes_context *ctx,
                           int mode,
                           size_t length,
                           unsigned char iv[16],
                           const unsigned char *input,
                           unsigned char *output )
{
    if( ctx == NULL || iv == NULL || input == NULL || output == NULL )
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;

    if( mode != MBEDTLS_AES_ENCRYPT && mode != MBEDTLS_AES_DECRYPT )
        return MBEDTLS_ERR_AES_BAD_INPUT_DATA;

    if( ( length % 16U ) != 0U )
        return MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH;

    CRYP_ConfigTypeDef cfg;
    memset( &cfg, 0, sizeof( cfg ) );

    cfg.DataType      = CRYP_BYTE_SWAP;
    cfg.DataWidthUnit = CRYP_DATAWIDTHUNIT_BYTE;
    cfg.KeySize       = ( ctx->keybits == 128U ) ? CRYP_KEYSIZE_128B
                                                 : CRYP_KEYSIZE_256B;
    cfg.pKey          = ctx->aes_key;
    cfg.Algorithm     = CRYP_AES_CBC;

    uint32_t iv_words[4];
    iv_words[0] = ( (uint32_t) iv[0]  << 24 ) |
                  ( (uint32_t) iv[1]  << 16 ) |
                  ( (uint32_t) iv[2]  <<  8 ) |
                  ( (uint32_t) iv[3]        );
    iv_words[1] = ( (uint32_t) iv[4]  << 24 ) |
                  ( (uint32_t) iv[5]  << 16 ) |
                  ( (uint32_t) iv[6]  <<  8 ) |
                  ( (uint32_t) iv[7]        );
    iv_words[2] = ( (uint32_t) iv[8]  << 24 ) |
                  ( (uint32_t) iv[9]  << 16 ) |
                  ( (uint32_t) iv[10] <<  8 ) |
                  ( (uint32_t) iv[11]       );
    iv_words[3] = ( (uint32_t) iv[12] << 24 ) |
                  ( (uint32_t) iv[13] << 16 ) |
                  ( (uint32_t) iv[14] <<  8 ) |
                  ( (uint32_t) iv[15]       );

    cfg.pInitVect = iv_words;

    if( HAL_CRYP_DeInit( &hcryp ) != HAL_OK )
        return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;

    hcryp.Instance = CRYP;
    if( HAL_CRYP_Init( &hcryp ) != HAL_OK )
        return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;

    if( HAL_CRYP_SetConfig( &hcryp, &cfg ) != HAL_OK )
        return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;

    HAL_StatusTypeDef st;

    if( mode == MBEDTLS_AES_ENCRYPT )
    {
        st = HAL_CRYP_Encrypt( &hcryp,
                               (uint8_t *) input,
                               (uint32_t) length,
                               output,
                               HAL_MAX_DELAY );
        if( st != HAL_OK )
            return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;

        memcpy( iv, &output[ length - 16U ], 16U );
    }
    else
    {
        st = HAL_CRYP_Decrypt( &hcryp,
                               (uint8_t *) input,
                               (uint32_t) length,
                               output,
                               HAL_MAX_DELAY );
        if( st != HAL_OK )
            return MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;

        memcpy( iv, &input[ length - 16U ], 16U );
    }

    return 0;
}
#endif /* MBEDTLS_CIPHER_MODE_CBC */

#endif /* MBEDTLS_AES_C && MBEDTLS_AES_ALT */

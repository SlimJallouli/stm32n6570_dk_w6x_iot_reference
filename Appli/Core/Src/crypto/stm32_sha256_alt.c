#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "mbedtls/sha256.h"
#include "main.h"
#include <string.h>

extern HASH_HandleTypeDef hhash;

void mbedtls_sha256_init( mbedtls_sha256_context *ctx )
{
    memset( ctx, 0, sizeof( *ctx ) );
}

void mbedtls_sha256_free( mbedtls_sha256_context *ctx )
{
    (void) ctx;
}

void mbedtls_sha256_clone( mbedtls_sha256_context *dst,
                           const mbedtls_sha256_context *src )
{
    /* For now, just copy the struct; your ctx is effectively empty anyway */
    *dst = *src;
}

static int hash_status_to_mbedtls( HAL_StatusTypeDef st )
{
    if( st == HAL_OK )
        return 0;

    return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
}

int mbedtls_sha256_starts( mbedtls_sha256_context *ctx, int is224 )
{
    HAL_StatusTypeDef st;

    (void) ctx;

    if( is224 != 0 )
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;

    st = HAL_HASH_DeInit( &hhash );
    if( st != HAL_OK )
        return hash_status_to_mbedtls( st );

    st = HAL_HASH_Init( &hhash );
    if( st != HAL_OK )
        return hash_status_to_mbedtls( st );

    return 0;
}

int mbedtls_sha256_update( mbedtls_sha256_context *ctx,
                           const unsigned char *input,
                           size_t ilen )
{
    size_t fill = ctx->total[0] & 0x3F;   // bytes currently in buffer
    size_t left = 64 - fill;

    ctx->total[0] += ilen;

    if( fill && ilen >= left )
    {
        memcpy( ctx->buffer + fill, input, left );

        // Process full 64-byte block
        HAL_StatusTypeDef st = HAL_HASH_Accumulate(
            &hhash,
            ctx->buffer,
            64,
            HAL_MAX_DELAY
        );
        if( st != HAL_OK )
            return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;

        input += left;
        ilen  -= left;
        fill   = 0;
    }

    // Process as many full blocks as possible
    while( ilen >= 64 )
    {
        HAL_StatusTypeDef st = HAL_HASH_Accumulate(
            &hhash,
            input,
            64,
            HAL_MAX_DELAY
        );
        if( st != HAL_OK )
            return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;

        input += 64;
        ilen  -= 64;
    }

    // Buffer remaining bytes
    if( ilen > 0 )
        memcpy( ctx->buffer + fill, input, ilen );

    return 0;
}


int mbedtls_sha256_finish( mbedtls_sha256_context *ctx,
                           unsigned char *output )
{
    size_t fill = ctx->total[0] & 0x3F;

    HAL_StatusTypeDef st = HAL_HASH_AccumulateLast(
        &hhash,
        fill ? ctx->buffer : NULL,
        fill,
        output,
        HAL_MAX_DELAY
    );

    return (st == HAL_OK) ? 0 : MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
}


/* One-shot helper used by md.c */
int mbedtls_sha256( const unsigned char *input,
                    size_t ilen,
                    unsigned char *output,
                    int is224 )
{
    mbedtls_sha256_context ctx;
    int ret = 0;

    mbedtls_sha256_init( &ctx );

    ret = mbedtls_sha256_starts( &ctx, is224 );

    if( ret == 0 )
    {
      ret = mbedtls_sha256_update( &ctx, input, ilen );
    }

    if( ret == 0 )
    {
      ret = mbedtls_sha256_finish( &ctx, output );
    }

    mbedtls_sha256_free( &ctx );

    return ret;
}

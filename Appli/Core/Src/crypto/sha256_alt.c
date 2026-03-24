#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "mbedtls/sha256.h"
#include "main.h"
#include "logging_levels.h"
#define LOG_LEVEL LOG_INFO
#include "logging.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include <string.h>

#if defined(MBEDTLS_SHA256_ALT)

extern HASH_HandleTypeDef hhash;

/* --------------------------------------------------------------------------
 * SHA256 ALT
 *
 * Incremental HAL SHA256 is intentionally removed.
 *
 * Reason:
 *   - STM32 HAL HASH suspend/resume path is unreliable for chunked updates.
 *   - HAL_HASH_Accumulate() requires strict 4-byte alignment and block sizes.
 *   - TLS transcript hashing (AWS IoT, X.509 verification) requires perfect
 *     determinism; even a 1‑bit mismatch breaks certificate validation.
 *
 * Buffered one-shot hardware hashing is stable, validated, and production-safe.
 * -------------------------------------------------------------------------- */

#define SHA256_ALT_MAX_MESSAGE_SIZE   (1024 * 1024) /* 1 MB cap */

static SemaphoreHandle_t xHashEngineMutex = NULL;
static StaticSemaphore_t xHashEngineMutexStorage;

/* Mutex wrapper */
static int prvHashEngineLock(void)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
        return 0;

    if (xHashEngineMutex == NULL)
    {
        taskENTER_CRITICAL();
        if (xHashEngineMutex == NULL)
            xHashEngineMutex = xSemaphoreCreateMutexStatic(&xHashEngineMutexStorage);
        taskEXIT_CRITICAL();
    }

    if (xHashEngineMutex == NULL)
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;

    if (xSemaphoreTake(xHashEngineMutex, portMAX_DELAY) != pdTRUE)
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;

    return 0;
}

static void prvHashEngineUnlock(void)
{
    if ((xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) &&
        (xHashEngineMutex != NULL))
    {
        (void)xSemaphoreGive(xHashEngineMutex);
    }
}

/* -------------------------------------------------------------------------- */
/* SHA256 ALT API                                                             */
/* -------------------------------------------------------------------------- */

void mbedtls_sha256_init(mbedtls_sha256_context *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

void mbedtls_sha256_free(mbedtls_sha256_context *ctx)
{
    if (ctx != NULL && ctx->hw_msg_buf != NULL)
    {
        /* Zeroize before free */
        memset(ctx->hw_msg_buf, 0, ctx->hw_msg_len);
        vPortFree(ctx->hw_msg_buf);
        ctx->hw_msg_buf = NULL;
    }

    if (ctx != NULL)
    {
        ctx->hw_msg_len = 0U;
        ctx->hw_msg_cap = 0U;
    }
}

void mbedtls_sha256_clone(mbedtls_sha256_context *dst,
                          const mbedtls_sha256_context *src)
{
    uint8_t *old = dst->hw_msg_buf;

    *dst = *src;

    if (old != NULL)
        vPortFree(old);

    if (src->hw_msg_buf != NULL)
    {
        dst->hw_msg_buf = pvPortMalloc(src->hw_msg_cap);
        if (dst->hw_msg_buf != NULL)
        {
            memcpy(dst->hw_msg_buf, src->hw_msg_buf, src->hw_msg_len);
        }
        else
        {
            dst->hw_msg_len = 0U;
            dst->hw_msg_cap = 0U;
        }
    }
}

/* Ensure buffer capacity with exponential growth */
static int prvEnsureMsgCapacity(mbedtls_sha256_context *ctx, size_t needed)
{
    if (needed > SHA256_ALT_MAX_MESSAGE_SIZE)
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;

    if (needed <= ctx->hw_msg_cap)
        return 0;

    size_t newCap = (ctx->hw_msg_cap == 0U) ? 256U : ctx->hw_msg_cap;

    while (newCap < needed)
    {
        if (newCap > ((size_t)-1 / 2U))
            return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
        newCap *= 2U;
    }

    uint8_t *pNew = pvPortMalloc(newCap);
    if (pNew == NULL)
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;

    if (ctx->hw_msg_buf != NULL && ctx->hw_msg_len > 0U)
    {
        memcpy(pNew, ctx->hw_msg_buf, ctx->hw_msg_len);
        vPortFree(ctx->hw_msg_buf);
    }

    ctx->hw_msg_buf = pNew;
    ctx->hw_msg_cap = newCap;

    return 0;
}

int mbedtls_sha256_starts(mbedtls_sha256_context *ctx, int is224)
{
    if (is224 != 0)
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;

    memset(ctx->total, 0, sizeof(ctx->total));
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
    ctx->is224 = 0;
    ctx->hw_msg_len = 0;

    return 0;
}

int mbedtls_sha256_update(mbedtls_sha256_context *ctx,
                          const unsigned char *input,
                          size_t ilen)
{
    if (ilen == 0)
        return 0;

    if (input == NULL)
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;

    size_t needed = ctx->hw_msg_len + ilen;

    int ret = prvEnsureMsgCapacity(ctx, needed);
    if (ret != 0)
        return ret;

    memcpy(ctx->hw_msg_buf + ctx->hw_msg_len, input, ilen);
    ctx->hw_msg_len = needed;

    return 0;
}

int mbedtls_sha256_finish(mbedtls_sha256_context *ctx,
                          unsigned char *output)
{
    if (output == NULL)
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;

    int ret = prvHashEngineLock();
    if (ret != 0)
        return ret;

    HAL_StatusTypeDef st = HAL_HASH_Start(
        &hhash,
        (const uint8_t *)ctx->hw_msg_buf,
        (uint32_t)ctx->hw_msg_len,
        output,
        HAL_MAX_DELAY);

    prvHashEngineUnlock();

    return (st == HAL_OK) ? 0 : MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;
}

/* One-shot helper */
int mbedtls_sha256(const unsigned char *input,
                   size_t ilen,
                   unsigned char *output,
                   int is224)
{
    if ((input == NULL && ilen != 0U) || output == NULL || is224 != 0)
        return MBEDTLS_ERR_SHA256_BAD_INPUT_DATA;

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    int ret = mbedtls_sha256_starts(&ctx, is224);
    if (ret == 0)
        ret = mbedtls_sha256_update(&ctx, input, ilen);
    if (ret == 0)
        ret = mbedtls_sha256_finish(&ctx, output);

    mbedtls_sha256_free(&ctx);
    return ret;
}
#endif

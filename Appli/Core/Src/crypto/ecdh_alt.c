/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    ecdh_alt.c
 * @author  GPM Application Team
 * @version V1.0
 * @date    31-March-2026
 * @brief   mbedTLS ECDH hardware acceleration implementation.
 *          Hardware-accelerated ECDH public key generation and shared secret
 *          computation using STM32N6570 PKA peripheral for P-256 curves.
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
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "logging_levels.h"
#define LOG_LEVEL LOG_ERROR
#include "logging.h"

#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"
#include "mbedtls/error.h"
#include "stm32n6xx_hal.h"
#include <string.h>
#include "logging.h"

#if defined(MBEDTLS_ECDH_GEN_PUBLIC_ALT) || defined(MBEDTLS_ECDH_COMPUTE_SHARED_ALT)

extern PKA_HandleTypeDef hpka;

#define EC_P256_BYTES 32

/* a = -3 → |a| = 3 */
static const uint8_t P256_A_ABS[EC_P256_BYTES] = {
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,3
};

#ifndef PKA_ECC_COEF_SIGN_POSITIVE
#define PKA_ECC_COEF_SIGN_POSITIVE 0U
#endif
#ifndef PKA_ECC_COEF_SIGN_NEGATIVE
#define PKA_ECC_COEF_SIGN_NEGATIVE 1U
#endif

/* ----------------- helpers ----------------- */

static int mpi_to_be32(const mbedtls_mpi *X, uint8_t out[EC_P256_BYTES])
{
    int ret;
    size_t len = mbedtls_mpi_size(X);

    if(len > EC_P256_BYTES)
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

    memset(out, 0, EC_P256_BYTES);
    ret = mbedtls_mpi_write_binary(X, out + (EC_P256_BYTES - len), len);
    return ret;
}

static int be32_to_mpi(mbedtls_mpi *X, const uint8_t in[EC_P256_BYTES])
{
    return mbedtls_mpi_read_binary(X, in, EC_P256_BYTES);
}

static int point_to_uncompressed(const mbedtls_ecp_point *P,
                                 uint8_t out[1 + 2*EC_P256_BYTES])
{
    int ret;
    out[0] = 0x04;
    MBEDTLS_MPI_CHK(mpi_to_be32(&P->X, out + 1));
    MBEDTLS_MPI_CHK(mpi_to_be32(&P->Y, out + 1 + EC_P256_BYTES));
cleanup:
    return ret;
}

static int uncompressed_to_point(mbedtls_ecp_point *P,
                                 const uint8_t in[1 + 2*EC_P256_BYTES])
{
    if(in[0] != 0x04)
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

    int ret;
    MBEDTLS_MPI_CHK(be32_to_mpi(&P->X, in + 1));
    MBEDTLS_MPI_CHK(be32_to_mpi(&P->Y, in + 1 + EC_P256_BYTES));
    MBEDTLS_MPI_CHK(mbedtls_mpi_lset(&P->Z, 1));
cleanup:
    return ret;
}

static int ec_group_to_hw_p256(const mbedtls_ecp_group *grp,
                               uint8_t p[EC_P256_BYTES],
                               uint8_t gx[EC_P256_BYTES],
                               uint8_t gy[EC_P256_BYTES],
                               uint8_t n[EC_P256_BYTES],
                               uint8_t b[EC_P256_BYTES])
{
    int ret = 0;

    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&grp->P,   p,  EC_P256_BYTES));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&grp->G.X, gx, EC_P256_BYTES));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&grp->G.Y, gy, EC_P256_BYTES));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&grp->N,   n,  EC_P256_BYTES));
    MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary(&grp->B,   b,  EC_P256_BYTES));

cleanup:
    return ret;
}

/* ----------------- ECDH GEN PUBLIC ----------------- */

#if defined(MBEDTLS_ECDH_GEN_PUBLIC_ALT)
int mbedtls_ecdh_gen_public(mbedtls_ecp_group *grp,
                            mbedtls_mpi *d,
                            mbedtls_ecp_point *Q,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng)
{
    int ret = 0;

    uint8_t d_buf[EC_P256_BYTES];
    uint8_t p[EC_P256_BYTES];
    uint8_t gx[EC_P256_BYTES];
    uint8_t gy[EC_P256_BYTES];
    uint8_t n[EC_P256_BYTES];
    uint8_t b[EC_P256_BYTES];

    uint8_t q_buf[1 + 2*EC_P256_BYTES];

    PKA_ECCMulInTypeDef in = {0};
    PKA_ECCMulOutTypeDef out = {0};

    if(grp->id != MBEDTLS_ECP_DP_SECP256R1)
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

    MBEDTLS_MPI_CHK(mbedtls_ecp_gen_privkey(grp, d, f_rng, p_rng));
    MBEDTLS_MPI_CHK(mpi_to_be32(d, d_buf));

    MBEDTLS_MPI_CHK(ec_group_to_hw_p256(grp, p, gx, gy, n, b));

    in.scalarMulSize = EC_P256_BYTES;
    in.modulusSize   = EC_P256_BYTES;

    in.coefSign = PKA_ECC_COEF_SIGN_NEGATIVE;
    in.coefA    = P256_A_ABS;
    in.coefB    = b;

    in.modulus   = p;
    in.pointX    = gx;
    in.pointY    = gy;
    in.scalarMul = d_buf;
    in.primeOrder = n;

    LogDebug("ECDH_GEN_PUBLIC: aSign=%u a=3 B[31]=0x%02X n[0]=0x%02X",
             in.coefSign, b[31], n[0]);

    if(HAL_PKA_ECCMul(&hpka, &in, HAL_MAX_DELAY) != HAL_OK)
    {
        uint32_t err = HAL_PKA_GetError(&hpka);
        LogError("HAL_PKA_ECCMul failed, err=0x%08lx", (unsigned long)err);
        ret = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
        goto cleanup;
    }

    out.ptX = pvPortMalloc(PKA_ECDSA_SIGN_OUT_FINAL_POINT_X);
    out.ptY = pvPortMalloc(PKA_ECDSA_SIGN_OUT_FINAL_POINT_Y);

    HAL_PKA_ECCMul_GetResult(&hpka, &out);

    q_buf[0] = 0x04;
    memcpy(q_buf + 1, out.ptX, EC_P256_BYTES);
    memcpy(q_buf + 1 + EC_P256_BYTES, out.ptY, EC_P256_BYTES);

    MBEDTLS_MPI_CHK(uncompressed_to_point(Q, q_buf));

cleanup:
  if (out.ptX != NULL)
  {
	vPortFree(out.ptX);
  }

  if (out.ptY != NULL)
  {
	vPortFree(out.ptY);
  }

    return ret;
}
#endif

/* ----------------- ECDH COMPUTE SHARED ----------------- */

#if defined(MBEDTLS_ECDH_COMPUTE_SHARED_ALT)
int mbedtls_ecdh_compute_shared(mbedtls_ecp_group *grp,
                                mbedtls_mpi *z,
                                const mbedtls_ecp_point *Q,
                                const mbedtls_mpi *d,
                                int (*f_rng)(void *, unsigned char *, size_t),
                                void *p_rng)
{
    (void)f_rng;
    (void)p_rng;

    int ret = 0;

    uint8_t d_buf[EC_P256_BYTES];
    uint8_t q_buf[1 + 2*EC_P256_BYTES];
    uint8_t s_buf[EC_P256_BYTES];

    uint8_t p[EC_P256_BYTES];
    uint8_t gx[EC_P256_BYTES];
    uint8_t gy[EC_P256_BYTES];
    uint8_t n[EC_P256_BYTES];
    uint8_t b[EC_P256_BYTES];

    PKA_ECCMulInTypeDef in = {0};
    PKA_ECCMulOutTypeDef out = {0};

    if(grp->id != MBEDTLS_ECP_DP_SECP256R1)
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

    MBEDTLS_MPI_CHK(mpi_to_be32(d, d_buf));
    MBEDTLS_MPI_CHK(point_to_uncompressed(Q, q_buf));
    MBEDTLS_MPI_CHK(ec_group_to_hw_p256(grp, p, gx, gy, n, b));

    in.scalarMulSize = EC_P256_BYTES;
    in.modulusSize   = EC_P256_BYTES;

    in.coefSign = PKA_ECC_COEF_SIGN_NEGATIVE;
    in.coefA    = P256_A_ABS;
    in.coefB    = b;

    in.modulus   = p;
    in.pointX    = q_buf + 1;
    in.pointY    = q_buf + 1 + EC_P256_BYTES;
    in.scalarMul = d_buf;
    in.primeOrder = n;

    LogDebug("ECDH_COMPUTE_SHARED: B[31]=0x%02X n[0]=0x%02X", b[31], n[0]);

    if(HAL_PKA_ECCMul(&hpka, &in, HAL_MAX_DELAY) != HAL_OK)
    {
        uint32_t err = HAL_PKA_GetError(&hpka);
        LogError("HAL_PKA_ECCMul failed, err=0x%08lx", (unsigned long)err);
        ret = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
        goto cleanup;
    }

    out.ptX = pvPortMalloc(PKA_ECDSA_SIGN_OUT_FINAL_POINT_X);
    out.ptY = pvPortMalloc(PKA_ECDSA_SIGN_OUT_FINAL_POINT_Y);

    HAL_PKA_ECCMul_GetResult(&hpka, &out);

    memcpy(s_buf, out.ptX, EC_P256_BYTES);
    MBEDTLS_MPI_CHK(be32_to_mpi(z, s_buf));

cleanup:
  if (out.ptX != NULL)
  {
	vPortFree(out.ptX);
  }

  if (out.ptY != NULL)
  {
	vPortFree(out.ptY);
  }

    return ret;
}
#endif

#endif /* ALT */

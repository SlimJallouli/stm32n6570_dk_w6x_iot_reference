/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    ecdsa_alt.c
 * @author  GPM Application Team
 * @version V1.0
 * @date    31-March-2026
 * @brief   mbedTLS ECDSA hardware acceleration implementation.
 *          Hardware-accelerated ECDSA signature generation and verification
 *          using STM32N6570 PKA peripheral for P-256 curves.
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
#define LOG_LEVEL LOG_DEBUG
#include "logging.h"

#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"

#include "mbedtls/platform_util.h"
#include "stm32n6xx_hal.h"
#include <string.h>
#include "FreeRTOS.h"

#if defined(MBEDTLS_ECDSA_SIGN_ALT) || defined(MBEDTLS_ECDSA_VERIFY_ALT)

extern PKA_HandleTypeDef hpka;

#define EC_P256_BYTES 32

#ifndef MBEDTLS_ERR_ECP_HW_ACCEL_FAILED
#define MBEDTLS_ERR_ECP_HW_ACCEL_FAILED MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED
#endif

/* For NIST P-256: a = -3 mod p -> coefSign = NEGATIVE, coef = |a| = 3 */
static const uint8_t P256_A_ABS[EC_P256_BYTES] = {
    /* 0x0000000000000000000000000000000000000000000000000000000000000003 */
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03
};

/* On N6, coefSign is 0 for positive, 1 for negative (check header if you want to be explicit) */
#ifndef PKA_ECC_COEF_SIGN_NEGATIVE
#define PKA_ECC_COEF_SIGN_NEGATIVE 1U
#endif

/* Convert mbedtls_ecp_group (P-256) to big-endian buffers for PKA */
static int ec_group_to_hw_p256(const mbedtls_ecp_group *grp,
                               uint8_t p[EC_P256_BYTES],
                               uint8_t b[EC_P256_BYTES],
                               uint8_t gx[EC_P256_BYTES],
                               uint8_t gy[EC_P256_BYTES],
                               uint8_t n[EC_P256_BYTES])
{
    int ret = 0;

    MBEDTLS_MPI_CHK( mbedtls_mpi_write_binary(&grp->P,   p,  EC_P256_BYTES) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_write_binary(&grp->B,   b,  EC_P256_BYTES) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_write_binary(&grp->G.X, gx, EC_P256_BYTES) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_write_binary(&grp->G.Y, gy, EC_P256_BYTES) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_write_binary(&grp->N,   n,  EC_P256_BYTES) );

cleanup:
    return ret;
}

/* --- helpers --- */

static int mpi_to_be32(const mbedtls_mpi *X, uint8_t out[EC_P256_BYTES])
{
    int ret;
    size_t len = mbedtls_mpi_size(X);

    if (len > EC_P256_BYTES)
    {
    	LogError("%d > EC_P256_BYTES", len);
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    memset(out, 0, EC_P256_BYTES);
    ret = mbedtls_mpi_write_binary(X, out + (EC_P256_BYTES - len), len);
    return ret;
}

static int be32_to_mpi(mbedtls_mpi *X, const uint8_t in[EC_P256_BYTES])
{
    return mbedtls_mpi_read_binary(X, in, EC_P256_BYTES);
}

static int point_to_uncompressed(const mbedtls_ecp_point *P, uint8_t out[1 + 2 * EC_P256_BYTES])
{
    int ret;

    out[0] = 0x04; /* uncompressed */
    MBEDTLS_MPI_CHK( mpi_to_be32(&P->X, out + 1) );
    MBEDTLS_MPI_CHK( mpi_to_be32(&P->Y, out + 1 + EC_P256_BYTES) );

cleanup:
    return ret;
}

static int uncompressed_to_point(mbedtls_ecp_point *P, const uint8_t in[1 + 2 * EC_P256_BYTES])
{
    if (in[0] != 0x04)
    {
    	LogError("MBEDTLS_ERR_ECP_BAD_INPUT_DATA");
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    int ret;
    MBEDTLS_MPI_CHK( be32_to_mpi(&P->X, in + 1) );
    MBEDTLS_MPI_CHK( be32_to_mpi(&P->Y, in + 1 + EC_P256_BYTES) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_lset(&P->Z, 1) );

cleanup:
    return ret;
}

static int hash_to_p256(const unsigned char *buf, size_t blen, uint8_t out[EC_P256_BYTES])
{
    /* If hash is longer than curve order size, keep the leftmost bytes (MSB side). */
    if (blen > EC_P256_BYTES)
    {
        blen = EC_P256_BYTES;
    }

    memset(out, 0, EC_P256_BYTES);
    /* Right-align in big-endian, but using the *first* blen bytes of the hash. */
    memcpy(out + (EC_P256_BYTES - blen), buf, blen);
    return 0;
}

/* generate random k in [1, n-1] using MbedTLS and convert to BE32 */
static int gen_k_p256(const mbedtls_ecp_group *grp,
                      uint8_t k_buf[EC_P256_BYTES],
                      int (*f_rng)(void *, unsigned char *, size_t),
                      void *p_rng)
{
    int ret;
    mbedtls_mpi k;

    mbedtls_mpi_init(&k);

    ret = mbedtls_ecp_gen_privkey(grp, &k, f_rng, p_rng);

    if (ret != 0)
    {
    	LogError("ecp_gen_privkey %d", ret);

        mbedtls_mpi_free(&k);

        return ret;
    }

    ret = mpi_to_be32(&k, k_buf);
    mbedtls_mpi_free(&k);

    return ret;
}

/* --- ALT implementations --- */
#if defined(MBEDTLS_ECDSA_SIGN_ALT)
int mbedtls_ecdsa_sign( mbedtls_ecp_group *grp,
                        mbedtls_mpi *r, mbedtls_mpi *s,
                        const mbedtls_mpi *d,
                        const unsigned char *buf, size_t blen,
                        int (*f_rng)(void *, unsigned char *, size_t),
                        void *p_rng )
 {
	int ret = 0;
	uint8_t d_buf[EC_P256_BYTES];
	uint8_t hash[EC_P256_BYTES];
	uint8_t k_buf[EC_P256_BYTES];
	uint8_t r_buf[EC_P256_BYTES];
	uint8_t s_buf[EC_P256_BYTES];

	uint8_t p[EC_P256_BYTES];
	uint8_t b[EC_P256_BYTES];
	uint8_t gx[EC_P256_BYTES];
	uint8_t gy[EC_P256_BYTES];
	uint8_t n[EC_P256_BYTES];

	PKA_ECDSASignOutTypeDef out = { 0 };
	PKA_ECDSASignOutExtParamTypeDef outExt = { 0 };

	if (grp->id != MBEDTLS_ECP_DP_SECP256R1)
	{
		LogError("grp->id != MBEDTLS_ECP_DP_SECP256R1");
		return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
	}

	MBEDTLS_MPI_CHK(mpi_to_be32(d, d_buf));
	MBEDTLS_MPI_CHK(hash_to_p256(buf, blen, hash));
	MBEDTLS_MPI_CHK(gen_k_p256(grp, k_buf, f_rng, p_rng));
	MBEDTLS_MPI_CHK(ec_group_to_hw_p256(grp, p, b, gx, gy, n));

	PKA_ECDSASignInTypeDef in = { 0 };
	in.primeOrderSize = EC_P256_BYTES;
	in.modulusSize    = EC_P256_BYTES;
	in.coefSign       = PKA_ECC_COEF_SIGN_NEGATIVE; /* a = -3 */
	in.coef           = P256_A_ABS; /* |a| = 3 */
	in.coefB          = b;
	in.modulus        = p;
	in.integer        = k_buf;
	in.basePointX     = gx;
	in.basePointY     = gy;
	in.hash           = hash;
	in.privateKey     = d_buf;
	in.primeOrder     = n;

	if (HAL_PKA_ECDSASign(&hpka, &in, HAL_MAX_DELAY) != HAL_OK)
	{
		// Optional: inspect hardware error
		uint32_t err = HAL_PKA_GetError(&hpka);

		LogError("ECDSA_SIGN: HAL_PKA_ECDSASign failed, err=0x%08lx\n", (unsigned long )err);

		ret = MBEDTLS_ERR_ECP_HW_ACCEL_FAILED;
		goto cleanup;
	}

	out.RSign = pvPortMalloc(EC_P256_BYTES);
	out.SSign = pvPortMalloc(EC_P256_BYTES);
	outExt.ptX = pvPortMalloc(EC_P256_BYTES);
	outExt.ptY = pvPortMalloc(EC_P256_BYTES);
	if ((out.RSign == NULL) || (out.SSign == NULL) || (outExt.ptX == NULL) || (outExt.ptY == NULL))
	{
		LogError("ECDSA_SIGN: output buffer alloc failed");
		ret = MBEDTLS_ERR_ECP_ALLOC_FAILED;
		goto cleanup;
	}

	HAL_PKA_ECDSASign_GetResult(&hpka, &out, &outExt);

	memcpy(r_buf, out.RSign, EC_P256_BYTES);
	memcpy(s_buf, out.SSign, EC_P256_BYTES);

	MBEDTLS_MPI_CHK(be32_to_mpi(r, r_buf));
	MBEDTLS_MPI_CHK(be32_to_mpi(s, s_buf));

cleanup:
	if (out.RSign != NULL)
	{
		vPortFree(out.RSign);
	}

	if (out.SSign != NULL)
	{
		vPortFree(out.SSign);
	}

	if (outExt.ptX != NULL)
	{
		vPortFree(outExt.ptX);
	}

	if (outExt.ptY != NULL)
	{
		vPortFree(outExt.ptY);
	}

	return ret;
}
#endif /* MBEDTLS_ECDSA_SIGN_ALT */

int mbedtls_ecdsa_can_do( mbedtls_ecp_group_id gid )
{
    /* We only accelerate plain ECDSA over P-256 */
    if( gid == MBEDTLS_ECP_DP_SECP256R1 )
    {
        return 1;
    }
    else
    {
    	LogError("Curve not supported");
        return 0;
    }
}

#if defined(MBEDTLS_ECDSA_VERIFY_ALT)
int mbedtls_ecdsa_verify( mbedtls_ecp_group *grp,
                          const unsigned char *buf, size_t blen,
                          const mbedtls_ecp_point *Q,
                          const mbedtls_mpi *r, const mbedtls_mpi *s )
{
    int ret = 0;
    uint8_t q_buf[1 + 2 * EC_P256_BYTES];
    uint8_t hash[EC_P256_BYTES];
    uint8_t r_buf[EC_P256_BYTES];
    uint8_t s_buf[EC_P256_BYTES];

    uint8_t p[EC_P256_BYTES];
    uint8_t b[EC_P256_BYTES];
    uint8_t gx[EC_P256_BYTES];
    uint8_t gy[EC_P256_BYTES];
    uint8_t n[EC_P256_BYTES];

    if (grp->id != MBEDTLS_ECP_DP_SECP256R1)
    {
    	LogError("grp->id != MBEDTLS_ECP_DP_SECP256R1");

        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    MBEDTLS_MPI_CHK( point_to_uncompressed( Q, q_buf ) );
    MBEDTLS_MPI_CHK( mpi_to_be32( r, r_buf ) );
    MBEDTLS_MPI_CHK( mpi_to_be32( s, s_buf ) );
    MBEDTLS_MPI_CHK( hash_to_p256( buf, blen, hash ) );
    MBEDTLS_MPI_CHK( ec_group_to_hw_p256(grp, p, b, gx, gy, n) );

    PKA_ECDSAVerifInTypeDef in = {0};
    in.primeOrderSize = EC_P256_BYTES;
    in.modulusSize    = EC_P256_BYTES;
    in.coefSign       = PKA_ECC_COEF_SIGN_NEGATIVE; /* a = -3 */

    in.coef            = P256_A_ABS; /* |a| = 3 */
    in.coefSign        = PKA_ECC_COEF_SIGN_NEGATIVE;
    in.modulus         = p;
    in.basePointX      = gx;
    in.basePointY      = gy;
    in.pPubKeyCurvePtX = q_buf + 1;
    in.pPubKeyCurvePtY = q_buf + 1 + EC_P256_BYTES;
    in.RSign           = r_buf;
    in.SSign           = s_buf;
    in.hash            = hash;
    in.primeOrder      = n;


    if (HAL_PKA_ECDSAVerif(&hpka, &in, HAL_MAX_DELAY) != HAL_OK)
    {
        uint32_t err = HAL_PKA_GetError(&hpka);

        LogError("ECDSA_VERIFY: HAL_PKA_ECDSAVerif failed, err=0x%08lx\n", (unsigned long)err);

        ret = MBEDTLS_ERR_ECP_HW_ACCEL_FAILED;
        goto cleanup;
    }

    if (HAL_PKA_ECDSAVerif_IsValidSignature(&hpka) != 1)
    {
        uint32_t err = HAL_PKA_GetError(&hpka);

        LogError("ECDSA_VERIFY: HAL_PKA_ECDSAVerif failed, err=0x%08lx\n", (unsigned long)err);

        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

cleanup:
    return ret;
}
#endif /* MBEDTLS_ECDSA_VERIFY_ALT */
#endif /* defined(MBEDTLS_ECDSA_SIGN_ALT) || defined(MBEDTLS_ECDSA_VERIFY_ALT) */

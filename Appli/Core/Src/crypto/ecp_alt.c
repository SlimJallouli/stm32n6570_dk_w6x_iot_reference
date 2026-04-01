/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    ecp_alt.c
 * @author  GPM Application Team
 * @version V1.0
 * @date    31-March-2026
 * @brief   mbedTLS ECP hardware acceleration implementation.
 *          Hardware-accelerated elliptic curve point arithmetic using
 *          STM32N6570 PKA peripheral.
 *
 *          Implements:
 *            - mbedtls_internal_ecp_normalize_jac()  via HAL_PKA_ECCProjective2Affine
 *            - mbedtls_internal_ecp_double_jac()     via HAL_PKA_ECCCompleteAddition (P+P)
 *            - mbedtls_internal_ecp_add_mixed()      via HAL_PKA_ECCCompleteAddition
 *
 *          Active only for SECP256R1 (P-256); other curves fall back to
 *          the mbedTLS software implementation automatically.
 *
 *          Requires in mbedtls_config_hw.h:
 *            #define MBEDTLS_ECP_INTERNAL_ALT
 *            #define MBEDTLS_ECP_NORMALIZE_JAC_ALT
 *            #define MBEDTLS_ECP_DOUBLE_JAC_ALT
 *            #define MBEDTLS_ECP_ADD_MIXED_ALT
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
/* USER CODE END Header */

#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "ecp_alt.h"
#include "mbedtls/bignum.h"
#include "mbedtls/error.h"
#include "stm32n6xx_hal.h"
#include "logging_levels.h"

#if defined LOG_LEVEL
#undef LOG_LEVEL
#endif

#define LOG_LEVEL LOG_ERROR

#include "logging.h"
#include <string.h>

#if defined(MBEDTLS_ECP_INTERNAL_ALT)

/* =========================================================================
 * Constants
 * =========================================================================*/

#define EC_P256_BYTES   32u   /* byte length of a P-256 field element */

/*
 * P-256 curve coefficient a = -3.
 * The PKA takes |a| as a big-endian byte array plus a sign flag.
 */
static const uint8_t P256_A_ABS[EC_P256_BYTES] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03 };

#ifndef PKA_ECC_COEF_SIGN_POSITIVE
#define PKA_ECC_COEF_SIGN_POSITIVE  0U
#endif
#ifndef PKA_ECC_COEF_SIGN_NEGATIVE
#define PKA_ECC_COEF_SIGN_NEGATIVE  1U
#endif

/* =========================================================================
 * External handle — defined and initialised by MX_PKA_Init() in main.c
 * =========================================================================*/
extern PKA_HandleTypeDef hpka;

/* =========================================================================
 * Internal helpers
 * =========================================================================*/

/**
 * @brief  Write an MPI into a fixed-size big-endian byte buffer (zero-padded).
 *         Returns MBEDTLS_ERR_ECP_BAD_INPUT_DATA if MPI is wider than
 *         EC_P256_BYTES.
 */
static int mpi_to_be32(const mbedtls_mpi *X, uint8_t out[EC_P256_BYTES])
{
  size_t len = mbedtls_mpi_size(X);

  if (len > EC_P256_BYTES)
  {
    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
  }

  memset(out, 0, EC_P256_BYTES);

  return mbedtls_mpi_write_binary(X, out + ( EC_P256_BYTES - len), len);
}

/**
 * @brief  Read a fixed-size big-endian byte buffer into an MPI.
 */
static int be32_to_mpi(mbedtls_mpi *X, const uint8_t in[EC_P256_BYTES])
{
  return mbedtls_mpi_read_binary(X, in, EC_P256_BYTES);
}

/**
 * @brief  Serialise the P-256 group parameters into byte arrays for the PKA.
 */
static int ec_group_to_hw_p256(const mbedtls_ecp_group *grp,
                                     uint8_t p[EC_P256_BYTES],
                                     uint8_t gx[EC_P256_BYTES],
                                     uint8_t gy[EC_P256_BYTES],
                                     uint8_t n[EC_P256_BYTES],
                                     uint8_t b[EC_P256_BYTES])
{
  int ret;
  MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary( &grp->P, p, EC_P256_BYTES ));
  MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary( &grp->G.X, gx, EC_P256_BYTES ));
  MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary( &grp->G.Y, gy, EC_P256_BYTES ));
  MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary( &grp->N, n, EC_P256_BYTES ));
  MBEDTLS_MPI_CHK(mbedtls_mpi_write_binary( &grp->B, b, EC_P256_BYTES ));
  cleanup: return ret;
}

/* =========================================================================
 * Capability / init / free  (mandatory for MBEDTLS_ECP_INTERNAL_ALT)
 * =========================================================================*/

unsigned char mbedtls_internal_ecp_grp_capable(const mbedtls_ecp_group *grp)
{
  if (grp == NULL)
  {
    return 0U;
  }

  /* Only P-256 is offloaded; other curves fall back to SW. */
  return (grp->id == MBEDTLS_ECP_DP_SECP256R1) ? 1U : 0U;
}

int mbedtls_internal_ecp_init(const mbedtls_ecp_group *grp)
{
  /* PKA peripheral is initialised once at system start (MX_PKA_Init).
   * Nothing extra required per operation.                             */
  (void) grp;
  return 0;
}

void mbedtls_internal_ecp_free(const mbedtls_ecp_group *grp)
{
  (void) grp;
}

/* =========================================================================
 * mbedtls_internal_ecp_normalize_jac
 *
 * Convert Jacobian (X:Y:Z) to affine (X/Z^2 : Y/Z^3 : 1) using the PKA's
 * HAL_PKA_ECCProjective2Affine operation.
 *
 * PKA API (stm32n6xx_hal_pka.h):
 *   HAL_StatusTypeDef HAL_PKA_ECCProjective2Affine(
 *       PKA_HandleTypeDef *hpka,
 *       PKA_ECCProjective2AffineInTypeDef *in,
 *       uint32_t Timeout);
 *   void HAL_PKA_ECCProjective2Affine_GetResult(
 *       PKA_HandleTypeDef *hpka,
 *       PKA_ECCProjective2AffineOutTypeDef *out);
 *
 * Error handling strategy:
 *  1. NULL guard at entry — no heap allocated yet; plain return.
 *  2. Z==0 (point at infinity) — already normalised; plain return.
 *  3. Every MPI serialisation uses MBEDTLS_MPI_CHK; first failure jumps
 *     to cleanup.
 *  4. Allocation failure → MBEDTLS_ERR_ECP_ALLOC_FAILED, goto cleanup.
 *  5. HAL failure → log PKA error code, set MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED,
 *     goto cleanup.
 *  6. Single cleanup block: vPortFree(NULL) is a FreeRTOS no-op, so calling
 *     it on un-allocated pointers is safe.
 * =========================================================================*/
#if defined(MBEDTLS_ECP_NORMALIZE_JAC_ALT)
int mbedtls_internal_ecp_normalize_jac(const mbedtls_ecp_group *grp, mbedtls_ecp_point *pt)
{
  int ret = 0;

  /* Stack-allocated serialisation buffers (no heap risk here). */
  uint8_t p_buf[EC_P256_BYTES];
  uint8_t gx_buf[EC_P256_BYTES]; /* needed by ec_group_to_hw_p256, unused by PKA op */
  uint8_t gy_buf[EC_P256_BYTES];
  uint8_t n_buf[EC_P256_BYTES];
  uint8_t b_buf[EC_P256_BYTES];
  uint8_t px_buf[EC_P256_BYTES];
  uint8_t py_buf[EC_P256_BYTES];
  uint8_t pz_buf[EC_P256_BYTES];

  /* Heap-allocated output buffers; initialised to NULL for safe cleanup. */
  uint8_t *out_x = NULL;
  uint8_t *out_y = NULL;
  uint32_t montgomery_param[EC_P256_BYTES / 4U];

  PKA_MontgomeryParamInTypeDef mont_in = { 0 };
  PKA_ECCProjective2AffineInTypeDef in = { 0 };
  PKA_ECCProjective2AffineOutTypeDef out = { 0 };

  /* --- Guard --------------------------------------------------------*/
  if (grp == NULL || pt == NULL)
  {
    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
  }

  /* Z == 0 is the point at infinity — already in canonical form. */
  if (mbedtls_mpi_cmp_int(&pt->MBEDTLS_PRIVATE(Z), 0) == 0)
  {
    return 0;
  }

  /* --- Serialise ----------------------------------------------------*/
  MBEDTLS_MPI_CHK(ec_group_to_hw_p256(grp, p_buf, gx_buf, gy_buf, n_buf, b_buf));
  MBEDTLS_MPI_CHK(mpi_to_be32( &pt->MBEDTLS_PRIVATE(X), px_buf ));
  MBEDTLS_MPI_CHK(mpi_to_be32( &pt->MBEDTLS_PRIVATE(Y), py_buf ));
  MBEDTLS_MPI_CHK(mpi_to_be32( &pt->MBEDTLS_PRIVATE(Z), pz_buf ));

  /* --- Allocate output buffers (FreeRTOS heap) ----------------------*/
  out_x = pvPortMalloc( EC_P256_BYTES);
  out_y = pvPortMalloc( EC_P256_BYTES);

  if (out_x == NULL || out_y == NULL)
  {
    LogError("normalize_jac: pvPortMalloc failed");
    ret = MBEDTLS_ERR_ECP_ALLOC_FAILED;

    goto cleanup;
  }

  /* --- Build PKA input ----------------------------------------------*/
  mont_in.size = EC_P256_BYTES;
  mont_in.pOp1 = p_buf;

  if (HAL_PKA_MontgomeryParam(&hpka, &mont_in, HAL_MAX_DELAY) != HAL_OK)
  {
    uint32_t pka_err = HAL_PKA_GetError(&hpka);
    LogError("HAL_PKA_MontgomeryParam failed, err=0x%08lx", (unsigned long )pka_err);
    ret = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
    goto cleanup;
  }

  HAL_PKA_MontgomeryParam_GetResult(&hpka, montgomery_param);

  in.modulusSize = EC_P256_BYTES;
  in.modulus = p_buf;
  in.basePointX = px_buf;
  in.basePointY = py_buf;
  in.basePointZ = pz_buf;
  in.pMontgomeryParam = montgomery_param;

  /* --- Launch (polling, blocking) -----------------------------------*/
  if (HAL_PKA_ECCProjective2Affine(&hpka, &in, HAL_MAX_DELAY) != HAL_OK)
  {
    uint32_t pka_err = HAL_PKA_GetError(&hpka);
    LogError("HAL_PKA_ECCProjective2Affine failed, err=0x%08lx", (unsigned long )pka_err);
    ret = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
    goto cleanup;
  }

  /* --- Retrieve result ----------------------------------------------*/
  out.ptX = out_x;
  out.ptY = out_y;
  HAL_PKA_ECCProjective2Affine_GetResult(&hpka, &out);

  /* --- Write back ---------------------------------------------------*/
  MBEDTLS_MPI_CHK(be32_to_mpi( &pt->MBEDTLS_PRIVATE(X), out_x ));
  MBEDTLS_MPI_CHK(be32_to_mpi( &pt->MBEDTLS_PRIVATE(Y), out_y ));
  MBEDTLS_MPI_CHK(mbedtls_mpi_lset( &pt->MBEDTLS_PRIVATE(Z), 1 ));

  cleanup: vPortFree(out_x); /* FreeRTOS: vPortFree(NULL) is a documented no-op */
  vPortFree(out_y);
  return ret;
}
#endif /* MBEDTLS_ECP_NORMALIZE_JAC_ALT */

/* =========================================================================
 * mbedtls_internal_ecp_double_jac
 *
 * Point doubling in Jacobian coordinates: R = 2*P.
 *
 * The STM32N6 PKA has no dedicated "point double" opcode.  We implement
 * doubling as P + P using HAL_PKA_ECCCompleteAddition, which handles
 * the equal-input case (doubling) correctly inside hardware.
 *
 * The result is returned in projective Jacobian coordinates, consistent
 * with what mbedTLS expects from this callback.
 *
 * Error handling:
 *  1. NULL guard — plain return, no heap.
 *  2. P at infinity (Z==0) → R = P, early return, no heap.
 *  3. MBEDTLS_MPI_CHK for all MPI ops; HAL failure → HW_ACCEL_FAILED.
 *  4. Single cleanup: vPortFree(NULL) safe.
 * =========================================================================*/
#if defined(MBEDTLS_ECP_DOUBLE_JAC_ALT)
int mbedtls_internal_ecp_double_jac(const mbedtls_ecp_group *grp, mbedtls_ecp_point *R, const mbedtls_ecp_point *P)
{
  int ret = 0;

  uint8_t p_buf[EC_P256_BYTES];
  uint8_t gx_buf[EC_P256_BYTES];
  uint8_t gy_buf[EC_P256_BYTES];
  uint8_t n_buf[EC_P256_BYTES];
  uint8_t b_buf[EC_P256_BYTES];
  uint8_t px_buf[EC_P256_BYTES];
  uint8_t py_buf[EC_P256_BYTES];
  uint8_t pz_buf[EC_P256_BYTES];

  uint8_t *out_x = NULL;
  uint8_t *out_y = NULL;
  uint8_t *out_z = NULL;

  PKA_ECCCompleteAdditionInTypeDef in = { 0 };
  PKA_ECCCompleteAdditionOutTypeDef out = { 0 };

  /* --- Guard --------------------------------------------------------*/
  if (grp == NULL || R == NULL || P == NULL)
  {
    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
  }

  /* Doubling the point at infinity yields itself. */
  if (mbedtls_mpi_cmp_int(&P->MBEDTLS_PRIVATE(Z), 0) == 0)
  {
    return mbedtls_ecp_copy(R, P);
  }

  /* --- Serialise ----------------------------------------------------*/
  MBEDTLS_MPI_CHK(ec_group_to_hw_p256(grp, p_buf, gx_buf, gy_buf, n_buf, b_buf));
  MBEDTLS_MPI_CHK(mpi_to_be32( &P->MBEDTLS_PRIVATE(X), px_buf ));
  MBEDTLS_MPI_CHK(mpi_to_be32( &P->MBEDTLS_PRIVATE(Y), py_buf ));
  MBEDTLS_MPI_CHK(mpi_to_be32( &P->MBEDTLS_PRIVATE(Z), pz_buf ));

  /* --- Allocate output buffers --------------------------------------*/
  out_x = pvPortMalloc( EC_P256_BYTES);
  out_y = pvPortMalloc( EC_P256_BYTES);
  out_z = pvPortMalloc( EC_P256_BYTES);

  if (out_x == NULL || out_y == NULL || out_z == NULL)
  {
    LogError("double_jac: pvPortMalloc failed");
    ret = MBEDTLS_ERR_ECP_ALLOC_FAILED;

    goto cleanup;
  }

  /* --- Build PKA input: both addends = P ----------------------------*/
  in.modulusSize = EC_P256_BYTES;
  in.coefSign = PKA_ECC_COEF_SIGN_NEGATIVE; /* a = -3 for P-256 */
  in.coefA = P256_A_ABS;
  in.modulus = p_buf;
  in.basePointX1 = px_buf; /* first addend: P  */
  in.basePointY1 = py_buf;
  in.basePointZ1 = pz_buf;
  in.basePointX2 = px_buf; /* second addend: P (→ P+P = 2P) */
  in.basePointY2 = py_buf;
  in.basePointZ2 = pz_buf;

  /* --- Launch (polling, blocking) -----------------------------------*/
  if (HAL_PKA_ECCCompleteAddition(&hpka, &in, HAL_MAX_DELAY) != HAL_OK)
  {
    uint32_t pka_err = HAL_PKA_GetError(&hpka);
    LogError("HAL_PKA_ECCCompleteAddition (double) failed, err=0x%08lx", (unsigned long )pka_err);
    ret = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;

    goto cleanup;
  }

  /* --- Retrieve result (Jacobian) -----------------------------------*/
  out.ptX = out_x;
  out.ptY = out_y;
  out.ptZ = out_z;
  HAL_PKA_ECCCompleteAddition_GetResult(&hpka, &out);

  /* --- Write back (copies safe if R aliases P) ----------------------*/
  MBEDTLS_MPI_CHK(be32_to_mpi( &R->MBEDTLS_PRIVATE(X), out_x ));
  MBEDTLS_MPI_CHK(be32_to_mpi( &R->MBEDTLS_PRIVATE(Y), out_y ));
  MBEDTLS_MPI_CHK(be32_to_mpi( &R->MBEDTLS_PRIVATE(Z), out_z ));

  cleanup: vPortFree(out_x);
  vPortFree(out_y);
  vPortFree(out_z);
  return ret;
}
#endif /* MBEDTLS_ECP_DOUBLE_JAC_ALT */

/* =========================================================================
 * mbedtls_internal_ecp_add_mixed
 *
 * Mixed-coordinate point addition: R = P + Q,
 * where P is in Jacobian (X:Y:Z) and Q is in affine (X:Y:1).
 *
 * HAL_PKA_ECCCompleteAddition accepts two projective-coordinate points,
 * so we supply Q with an explicit Z = 1 (constant byte array, no alloc).
 *
 * The "complete" addition handles degenerate cases (P==Q, P==-Q, P or Q
 * at infinity) correctly inside the PKA hardware.
 *
 * Error handling:
 *  1. NULL guard — plain return, no heap.
 *  2. P at infinity (Zp == 0) → R = Q, early return, no heap.
 *  3. Q at infinity (Zq == 0, if limb present) → R = P, early return, no heap.
 *  4. Q must be affine (Zq == 1 or no limb); otherwise BAD_INPUT_DATA.
 *  5. MBEDTLS_MPI_CHK for all MPI ops.
 *  6. Allocation failure → ALLOC_FAILED, goto cleanup.
 *  7. HAL failure → log PKA error, HW_ACCEL_FAILED, goto cleanup.
 *  8. Single cleanup block; vPortFree(NULL) safe.
 * =========================================================================*/
#if defined(MBEDTLS_ECP_ADD_MIXED_ALT)
int mbedtls_internal_ecp_add_mixed(const mbedtls_ecp_group *grp, mbedtls_ecp_point *R, const mbedtls_ecp_point *P, const mbedtls_ecp_point *Q)
{
  int ret = 0;

  uint8_t p_buf[EC_P256_BYTES];
  uint8_t gx_buf[EC_P256_BYTES];
  uint8_t gy_buf[EC_P256_BYTES];
  uint8_t n_buf[EC_P256_BYTES];
  uint8_t b_buf[EC_P256_BYTES];
  uint8_t px_buf[EC_P256_BYTES];
  uint8_t py_buf[EC_P256_BYTES];
  uint8_t pz_buf[EC_P256_BYTES];
  uint8_t qx_buf[EC_P256_BYTES];
  uint8_t qy_buf[EC_P256_BYTES];

  /* Z = 1 for the affine point Q — constant, no heap needed. */
  static const uint8_t z_one[EC_P256_BYTES] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

  uint8_t *out_x = NULL;
  uint8_t *out_y = NULL;
  uint8_t *out_z = NULL;

  PKA_ECCCompleteAdditionInTypeDef in = { 0 };
  PKA_ECCCompleteAdditionOutTypeDef out = { 0 };

  /* --- Guard --------------------------------------------------------*/
  if (grp == NULL || R == NULL || P == NULL || Q == NULL)
  {
    return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
  }

  /* P is the point at infinity → R = Q. */
  if (mbedtls_mpi_cmp_int(&P->MBEDTLS_PRIVATE(Z), 0) == 0)
  {
    return mbedtls_ecp_copy(R, Q);
  }

  /* Q carries an explicit Z field — validate it. */
  if (Q->MBEDTLS_PRIVATE(Z).MBEDTLS_PRIVATE(p) != NULL)
  {
    if (mbedtls_mpi_cmp_int(&Q->MBEDTLS_PRIVATE(Z), 0) == 0)
    {
      return mbedtls_ecp_copy(R, P); /* Q at infinity → R = P */
    }

    if (mbedtls_mpi_cmp_int(&Q->MBEDTLS_PRIVATE(Z), 1) != 0)
    {
      return MBEDTLS_ERR_ECP_BAD_INPUT_DATA; /* Q must be affine */
    }
  }

  /* --- Serialise curve parameters -----------------------------------*/
  MBEDTLS_MPI_CHK(ec_group_to_hw_p256(grp, p_buf, gx_buf, gy_buf, n_buf, b_buf));

  /* --- Serialise P and Q coordinates --------------------------------*/
  MBEDTLS_MPI_CHK(mpi_to_be32( &P->MBEDTLS_PRIVATE(X), px_buf ));
  MBEDTLS_MPI_CHK(mpi_to_be32( &P->MBEDTLS_PRIVATE(Y), py_buf ));
  MBEDTLS_MPI_CHK(mpi_to_be32( &P->MBEDTLS_PRIVATE(Z), pz_buf ));
  MBEDTLS_MPI_CHK(mpi_to_be32( &Q->MBEDTLS_PRIVATE(X), qx_buf ));
  MBEDTLS_MPI_CHK(mpi_to_be32( &Q->MBEDTLS_PRIVATE(Y), qy_buf ));

  /* --- Allocate output buffers --------------------------------------*/
  out_x = pvPortMalloc( EC_P256_BYTES);
  out_y = pvPortMalloc( EC_P256_BYTES);
  out_z = pvPortMalloc( EC_P256_BYTES);

  if (out_x == NULL || out_y == NULL || out_z == NULL)
  {
    LogError("add_mixed: pvPortMalloc failed");
    ret = MBEDTLS_ERR_ECP_ALLOC_FAILED;
    goto cleanup;
  }

  /* --- Build PKA input ----------------------------------------------*/
  in.modulusSize = EC_P256_BYTES;
  in.coefSign = PKA_ECC_COEF_SIGN_NEGATIVE; /* a = -3 for P-256 */
  in.coefA = P256_A_ABS;
  in.modulus = p_buf;
  /* First addend: P (Jacobian) */
  in.basePointX1 = px_buf;
  in.basePointY1 = py_buf;
  in.basePointZ1 = pz_buf;
  /* Second addend: Q (affine → Z = 1) */
  in.basePointX2 = qx_buf;
  in.basePointY2 = qy_buf;
  in.basePointZ2 = z_one;

  /* --- Launch (polling, blocking) -----------------------------------*/
  if (HAL_PKA_ECCCompleteAddition(&hpka, &in, HAL_MAX_DELAY) != HAL_OK)
  {
    uint32_t pka_err = HAL_PKA_GetError(&hpka);
    LogError("HAL_PKA_ECCCompleteAddition failed, err=0x%08lx", (unsigned long )pka_err);
    ret = MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED;
    goto cleanup;
  }

  /* --- Retrieve result (Jacobian) -----------------------------------*/
  out.ptX = out_x;
  out.ptY = out_y;
  out.ptZ = out_z;
  HAL_PKA_ECCCompleteAddition_GetResult(&hpka, &out);

  /* --- Write back (copies safe if R aliases P or Q) -----------------*/
  MBEDTLS_MPI_CHK(be32_to_mpi( &R->MBEDTLS_PRIVATE(X), out_x ));
  MBEDTLS_MPI_CHK(be32_to_mpi( &R->MBEDTLS_PRIVATE(Y), out_y ));
  MBEDTLS_MPI_CHK(be32_to_mpi( &R->MBEDTLS_PRIVATE(Z), out_z ));

  cleanup:
  /* vPortFree(NULL) is a documented FreeRTOS no-op — safe to always call. */
  vPortFree(out_x);
  vPortFree(out_y);
  vPortFree(out_z);
  return ret;
}
#endif /* MBEDTLS_ECP_ADD_MIXED_ALT */

#endif /* MBEDTLS_ECP_INTERNAL_ALT */

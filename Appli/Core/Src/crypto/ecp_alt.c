#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "ecp_alt.h"
#include "mbedtls/bignum.h"
#include "mbedtls/error.h"

#if defined(MBEDTLS_ECP_INTERNAL_ALT)

static int prv_mod( const mbedtls_ecp_group *grp, mbedtls_mpi *X )
{
    return mbedtls_mpi_mod_mpi( X, X, &grp->P );
}

static int prv_mul_mod( const mbedtls_ecp_group *grp,
                        mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        const mbedtls_mpi *B )
{
    int ret = mbedtls_mpi_mul_mpi( X, A, B );

    if( ret == 0 )
    {
        ret = prv_mod( grp, X );
    }

    return ret;
}

static int prv_add_mod( const mbedtls_ecp_group *grp,
                        mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        const mbedtls_mpi *B )
{
    int ret = mbedtls_mpi_add_mpi( X, A, B );

    if( ret == 0 )
    {
        ret = prv_mod( grp, X );
    }

    return ret;
}

static int prv_sub_mod( const mbedtls_ecp_group *grp,
                        mbedtls_mpi *X,
                        const mbedtls_mpi *A,
                        const mbedtls_mpi *B )
{
    int ret;
    mbedtls_mpi T;

    mbedtls_mpi_init( &T );

    if( mbedtls_mpi_cmp_mpi( A, B ) >= 0 )
    {
        ret = mbedtls_mpi_sub_mpi( X, A, B );
    }
    else
    {
        ret = mbedtls_mpi_add_mpi( &T, A, &grp->P );

        if( ret == 0 )
        {
            ret = mbedtls_mpi_sub_mpi( X, &T, B );
        }
    }

    if( ret == 0 )
    {
        ret = prv_mod( grp, X );
    }

    mbedtls_mpi_free( &T );
    return ret;
}

static int prv_shift_l_mod( const mbedtls_ecp_group *grp,
                            mbedtls_mpi *X,
                            size_t count )
{
    int ret = mbedtls_mpi_shift_l( X, count );

    if( ret == 0 )
    {
        ret = prv_mod( grp, X );
    }

    return ret;
}

unsigned char mbedtls_internal_ecp_grp_capable( const mbedtls_ecp_group *grp )
{
    if( grp == NULL )
    {
        return 0;
    }

    return ( grp->id == MBEDTLS_ECP_DP_SECP256R1 ) ? 1U : 0U;
}

int mbedtls_internal_ecp_init( const mbedtls_ecp_group *grp )
{
    (void) grp;
    return 0;
}

void mbedtls_internal_ecp_free( const mbedtls_ecp_group *grp )
{
    (void) grp;
}

#if defined(MBEDTLS_ECP_NORMALIZE_JAC_ALT)
int mbedtls_internal_ecp_normalize_jac( const mbedtls_ecp_group *grp,
                                        mbedtls_ecp_point *pt )
{
    int ret = 0;
    mbedtls_mpi Zi, ZZi;

    if( grp == NULL || pt == NULL )
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    if( mbedtls_mpi_cmp_int( &pt->MBEDTLS_PRIVATE(Z), 0 ) == 0 )
    {
        return 0;
    }

    mbedtls_mpi_init( &Zi );
    mbedtls_mpi_init( &ZZi );

    ret = mbedtls_mpi_inv_mod( &Zi, &pt->MBEDTLS_PRIVATE(Z), &grp->P );

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &ZZi, &Zi, &Zi );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &pt->MBEDTLS_PRIVATE(X), &pt->MBEDTLS_PRIVATE(X), &ZZi );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &pt->MBEDTLS_PRIVATE(Y), &pt->MBEDTLS_PRIVATE(Y), &ZZi );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &pt->MBEDTLS_PRIVATE(Y), &pt->MBEDTLS_PRIVATE(Y), &Zi );
    }

    if( ret == 0 )
    {
        ret = mbedtls_mpi_lset( &pt->MBEDTLS_PRIVATE(Z), 1 );
    }

    mbedtls_mpi_free( &Zi );
    mbedtls_mpi_free( &ZZi );
    return ret;
}
#endif

#if defined(MBEDTLS_ECP_DOUBLE_JAC_ALT)
int mbedtls_internal_ecp_double_jac( const mbedtls_ecp_group *grp,
                                     mbedtls_ecp_point *R,
                                     const mbedtls_ecp_point *P )
{
    int ret = 0;
    mbedtls_mpi M, S, T, U;

    if( grp == NULL || R == NULL || P == NULL )
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    if( mbedtls_mpi_cmp_int( &P->MBEDTLS_PRIVATE(Z), 0 ) == 0 )
    {
        return mbedtls_ecp_copy( R, P );
    }

    mbedtls_mpi_init( &M );
    mbedtls_mpi_init( &S );
    mbedtls_mpi_init( &T );
    mbedtls_mpi_init( &U );

    if( grp->A.MBEDTLS_PRIVATE(p) == NULL )
    {
        ret = prv_mul_mod( grp, &S, &P->MBEDTLS_PRIVATE(Z), &P->MBEDTLS_PRIVATE(Z) );

        if( ret == 0 )
        {
            ret = prv_add_mod( grp, &T, &P->MBEDTLS_PRIVATE(X), &S );
        }

        if( ret == 0 )
        {
            ret = prv_sub_mod( grp, &U, &P->MBEDTLS_PRIVATE(X), &S );
        }

        if( ret == 0 )
        {
            ret = prv_mul_mod( grp, &S, &T, &U );
        }

        if( ret == 0 )
        {
            ret = mbedtls_mpi_mul_int( &M, &S, 3 );
        }

        if( ret == 0 )
        {
            ret = prv_mod( grp, &M );
        }
    }
    else
    {
        ret = prv_mul_mod( grp, &S, &P->MBEDTLS_PRIVATE(X), &P->MBEDTLS_PRIVATE(X) );

        if( ret == 0 )
        {
            ret = mbedtls_mpi_mul_int( &M, &S, 3 );
        }

        if( ret == 0 )
        {
            ret = prv_mod( grp, &M );
        }

        if( ret == 0 && mbedtls_mpi_cmp_int( &grp->A, 0 ) != 0 )
        {
            ret = prv_mul_mod( grp, &S, &P->MBEDTLS_PRIVATE(Z), &P->MBEDTLS_PRIVATE(Z) );

            if( ret == 0 )
            {
                ret = prv_mul_mod( grp, &T, &S, &S );
            }

            if( ret == 0 )
            {
                ret = prv_mul_mod( grp, &S, &T, &grp->A );
            }

            if( ret == 0 )
            {
                ret = prv_add_mod( grp, &M, &M, &S );
            }
        }
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &T, &P->MBEDTLS_PRIVATE(Y), &P->MBEDTLS_PRIVATE(Y) );
    }

    if( ret == 0 )
    {
        ret = prv_shift_l_mod( grp, &T, 1 );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &S, &P->MBEDTLS_PRIVATE(X), &T );
    }

    if( ret == 0 )
    {
        ret = prv_shift_l_mod( grp, &S, 1 );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &U, &T, &T );
    }

    if( ret == 0 )
    {
        ret = prv_shift_l_mod( grp, &U, 1 );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &T, &M, &M );
    }

    if( ret == 0 )
    {
        ret = prv_sub_mod( grp, &T, &T, &S );
    }

    if( ret == 0 )
    {
        ret = prv_sub_mod( grp, &T, &T, &S );
    }

    if( ret == 0 )
    {
        ret = prv_sub_mod( grp, &S, &S, &T );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &S, &S, &M );
    }

    if( ret == 0 )
    {
        ret = prv_sub_mod( grp, &S, &S, &U );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &U, &P->MBEDTLS_PRIVATE(Y), &P->MBEDTLS_PRIVATE(Z) );
    }

    if( ret == 0 )
    {
        ret = prv_shift_l_mod( grp, &U, 1 );
    }

    if( ret == 0 )
    {
        ret = mbedtls_mpi_copy( &R->MBEDTLS_PRIVATE(X), &T );
    }

    if( ret == 0 )
    {
        ret = mbedtls_mpi_copy( &R->MBEDTLS_PRIVATE(Y), &S );
    }

    if( ret == 0 )
    {
        ret = mbedtls_mpi_copy( &R->MBEDTLS_PRIVATE(Z), &U );
    }

    mbedtls_mpi_free( &M );
    mbedtls_mpi_free( &S );
    mbedtls_mpi_free( &T );
    mbedtls_mpi_free( &U );
    return ret;
}
#endif

#if defined(MBEDTLS_ECP_ADD_MIXED_ALT)
int mbedtls_internal_ecp_add_mixed( const mbedtls_ecp_group *grp,
                                    mbedtls_ecp_point *R,
                                    const mbedtls_ecp_point *P,
                                    const mbedtls_ecp_point *Q )
{
    int ret = 0;
    mbedtls_mpi T1, T2, T3, T4, X, Y, Z;

    if( grp == NULL || R == NULL || P == NULL || Q == NULL )
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    if( mbedtls_mpi_cmp_int( &P->MBEDTLS_PRIVATE(Z), 0 ) == 0 )
    {
        return mbedtls_ecp_copy( R, Q );
    }

    if( Q->MBEDTLS_PRIVATE(Z).MBEDTLS_PRIVATE(p) != NULL &&
        mbedtls_mpi_cmp_int( &Q->MBEDTLS_PRIVATE(Z), 0 ) == 0 )
    {
        return mbedtls_ecp_copy( R, P );
    }

    if( Q->MBEDTLS_PRIVATE(Z).MBEDTLS_PRIVATE(p) != NULL &&
        mbedtls_mpi_cmp_int( &Q->MBEDTLS_PRIVATE(Z), 1 ) != 0 )
    {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    mbedtls_mpi_init( &T1 );
    mbedtls_mpi_init( &T2 );
    mbedtls_mpi_init( &T3 );
    mbedtls_mpi_init( &T4 );
    mbedtls_mpi_init( &X );
    mbedtls_mpi_init( &Y );
    mbedtls_mpi_init( &Z );

    ret = prv_mul_mod( grp, &T1, &P->MBEDTLS_PRIVATE(Z), &P->MBEDTLS_PRIVATE(Z) );

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &T2, &T1, &P->MBEDTLS_PRIVATE(Z) );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &T1, &T1, &Q->MBEDTLS_PRIVATE(X) );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &T2, &T2, &Q->MBEDTLS_PRIVATE(Y) );
    }

    if( ret == 0 )
    {
        ret = prv_sub_mod( grp, &T1, &T1, &P->MBEDTLS_PRIVATE(X) );
    }

    if( ret == 0 )
    {
        ret = prv_sub_mod( grp, &T2, &T2, &P->MBEDTLS_PRIVATE(Y) );
    }

    if( ret == 0 && mbedtls_mpi_cmp_int( &T1, 0 ) == 0 )
    {
        if( mbedtls_mpi_cmp_int( &T2, 0 ) == 0 )
        {
            ret = mbedtls_internal_ecp_double_jac( grp, R, P );
        }
        else
        {
            ret = mbedtls_ecp_set_zero( R );
        }

        goto cleanup;
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &Z, &P->MBEDTLS_PRIVATE(Z), &T1 );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &T3, &T1, &T1 );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &T4, &T3, &T1 );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &T3, &T3, &P->MBEDTLS_PRIVATE(X) );
    }

    if( ret == 0 )
    {
        ret = mbedtls_mpi_copy( &T1, &T3 );
    }

    if( ret == 0 )
    {
        ret = prv_shift_l_mod( grp, &T1, 1 );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &X, &T2, &T2 );
    }

    if( ret == 0 )
    {
        ret = prv_sub_mod( grp, &X, &X, &T1 );
    }

    if( ret == 0 )
    {
        ret = prv_sub_mod( grp, &X, &X, &T4 );
    }

    if( ret == 0 )
    {
        ret = prv_sub_mod( grp, &T3, &T3, &X );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &T3, &T3, &T2 );
    }

    if( ret == 0 )
    {
        ret = prv_mul_mod( grp, &T4, &T4, &P->MBEDTLS_PRIVATE(Y) );
    }

    if( ret == 0 )
    {
        ret = prv_sub_mod( grp, &Y, &T3, &T4 );
    }

    if( ret == 0 )
    {
        ret = mbedtls_mpi_copy( &R->MBEDTLS_PRIVATE(X), &X );
    }

    if( ret == 0 )
    {
        ret = mbedtls_mpi_copy( &R->MBEDTLS_PRIVATE(Y), &Y );
    }

    if( ret == 0 )
    {
        ret = mbedtls_mpi_copy( &R->MBEDTLS_PRIVATE(Z), &Z );
    }

cleanup:
    mbedtls_mpi_free( &T1 );
    mbedtls_mpi_free( &T2 );
    mbedtls_mpi_free( &T3 );
    mbedtls_mpi_free( &T4 );
    mbedtls_mpi_free( &X );
    mbedtls_mpi_free( &Y );
    mbedtls_mpi_free( &Z );
    return ret;
}
#endif

#endif /* MBEDTLS_ECP_INTERNAL_ALT */

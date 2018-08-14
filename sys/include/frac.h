/*
 * Copyright (C) 2018 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup  sys_frac   Fractional integer operations
 * @ingroup   sys
 *
 * This header provides some functions for scaling integers by fractions, while
 * preserving as many bits as possible.
 *
 * The implementation requires that @ref frac_t is initialized properly, either
 * by calling @ref frac_init, which will compute the algorithm parameters at
 * runtime, or via a precomputed initializer.
 *
 * Precomputing the frac_t values can be done via the application found in
 * `tests/frac-config` in the RIOT tree.
 *
 * @see       Libdivide homepage: http://libdivide.com/
 *
 * @file
 * @ingroup   sys
 * @author    Joakim Nohlgård <joakim.nohlgard@eistec.se>
 * @{
 */

#ifndef FRAC_H
#define FRAC_H

#include <assert.h>
#include <stdint.h>
#include "libdivide.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   frac descriptor for fraction consisting of two 32 bit integers
 */
typedef struct {
    uint32_t num; /**< numerator */
    uint32_t den; /**< denominator, needed for modulo operation */
    uint32_t frac; /**< fraction */
    uint8_t shift; /**< exponent */
} frac_t;

/**
 * @brief   Initialize frac_t struct
 *
 * This function computes the mathematical parameters used by the frac algorithm.
 *
 * @note Be extra careful if @p num > @p den, the result from @ref frac_scale
 * may not fit in a 64 bit integer if @c x is very big.
 *
 * @pre @p den must not be 0
 *
 * @param[out]  frac    pointer to frac descriptor to initialize
 * @param[in]   num     numerator
 * @param[in]   den     denominator
 */
void frac_init(frac_t *frac, uint32_t num, uint32_t den);

/**
 * @brief Scale a 64 bit integer by a 32/32 integer fraction
 *
 * @pre x * frac < 2**64, i.e. the result fits in a 64 bit integer
 *
 * @param[in]   frac  scaling fraction
 * @param[in]   x     unscaled integer
 *
 * @return      x * frac, avoiding truncation
 * @return      a wrong result if x * frac > 2**64
 */
uint32_t frac_scale(const frac_t *frac, uint32_t x);

#ifdef __cplusplus
}
#endif
/** @} */
#endif /* FRAC_H */

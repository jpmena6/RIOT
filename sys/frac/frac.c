/**
 * Copyright (C) 2018 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * @ingroup sys_util
 * @{
 * @file
 * @brief    Integer fraction function implementations
 *
 * @author   Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include <stdint.h>
#include <stdio.h>
#include "assert.h"
#include "frac.h"
#include "libdivide.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

/**
 * @brief   compute greatest common divisor of @p u and @p v
 *
 * @param[in]   u    first operand
 * @param[in]   v    second operand
 *
 * @return  Greatest common divisor of @p u and @p v
 */
static uint32_t gcd32(uint32_t u, uint32_t v)
{
    /* Source: https://en.wikipedia.org/wiki/Binary_GCD_algorithm#Iterative_version_in_C */
    unsigned shift;

    /* GCD(0,v) == v; GCD(u,0) == u, GCD(0,0) == 0 */
    if (u == 0) {
        return v;
    }
    if (v == 0) {
        return u;
    }

    /* Let shift := lg K, where K is the greatest power of 2
     * dividing both u and v. */
    for (shift = 0; ((u | v) & 1) == 0; ++shift) {
        u >>= 1;
        v >>= 1;
    }

    /* remove all factors of 2 in u */
    while ((u & 1) == 0) {
        u >>= 1;
    }

    /* From here on, u is always odd. */
    do {
        /* remove all factors of 2 in v -- they are not common */
        /*   note: v is not zero, so while will terminate */
        while ((v & 1) == 0) {
            v >>= 1;
        }

        /* Now u and v are both odd. Swap if necessary so u <= v,
         * then set v = v - u (which is even). */
        if (u > v) {
            /* Swap u and v */
            unsigned int t = v;
            v = u;
            u = t;
        }

        v = v - u; /* Here v >= u */
    } while (v != 0);

    /* restore common factors of 2 */
    return u << shift;
}

void frac_init(frac_t *frac, uint32_t num, uint32_t den)
{
    DEBUG("frac_init32(%p, %" PRIu32 ", %" PRIu32 ")\n", (const void *)frac, num, den);
    assert(den);
    /* Reduce the fraction to shortest possible form by dividing by the greatest
     * common divisor */
    uint32_t gcd = gcd32(num, den);
    DEBUG("frac_init32: gcd = %" PRIu32 "\n", gcd);
    /* Use libdivide even though we only use this divisor twice, to avoid
     * unnecessarily pulling in libgcc division helpers if hardware is missing
     * division instructions */
    struct libdivide_u64_t div = libdivide_u64_gen(gcd);
    /* equivalent to: den /= gcd; */
    den = libdivide_u64_do(den, &div);
    num = libdivide_u64_do(num, &div);
    DEBUG("frac_init32: gcd = %" PRIu32 " num = %" PRIu32 " den = %" PRIu32 "\n",
          gcd, num, den);
    frac->div = libdivide_u64_gen(den);
    frac->den = den;
    frac->num = num;
}

uint64_t frac_scale(const frac_t *frac, uint64_t x)
{
    assert(frac);
    /* integer quotient */
    uint64_t quot = libdivide_u64_do(x, &frac->div);
    /* remainder */
    uint64_t rem = x - (quot * frac->den);
    /* quot * frac->num may wrap around when num > den and x is "big" */
    /* rem * frac->num will never wrap around as long as both num and den are at
     * most 32 bits wide, u32 x u32 -> u64 */
    uint64_t scaled = quot * frac->num + libdivide_u64_do(rem * frac->num, &frac->div);
    DEBUG("frac_scale32: x = %" PRIu64 " quot = %" PRIu64 " rem = %" PRIu64
          " scaled = %" PRIu64 "\n", x, quot, rem, scaled);
    return scaled;
}

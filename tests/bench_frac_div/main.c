/*
 * Copyright (C) 2018 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Division code benchmark
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "frac.h"
#include "div.h"
#include "periph/timer.h"

#ifndef TEST_NUMOF
#define TEST_NUMOF       2048
#endif

#ifndef TIM_REF_DEV
#define TIM_REF_DEV     TIMER_DEV(0)
#endif

#ifndef TIM_REF_FREQ
#define TIM_REF_FREQ    1000000ul
#endif

/* working area */
static uint64_t buf[TEST_NUMOF];

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Apply div_u32_by_15625div512 on all elements of buf */
uint32_t bench_div_u32_by_15625div512(uint64_t *buf, size_t nelem)
{
    unsigned time_start = timer_read(TIM_REF_DEV);
    for (unsigned k = 0; k < nelem; ++k) {
        buf[k] = div_u32_by_15625div512(buf[k]);
    }
    unsigned time_end = timer_read(TIM_REF_DEV);
    return (time_end - time_start);
}

/* Use frac_scale on all elements of buf */
uint32_t bench_frac(uint64_t *buf, size_t nelem, uint32_t num, uint32_t den)
{
    frac_t frac;
    frac_init(&frac, num, den);
    unsigned time_start = timer_read(TIM_REF_DEV);
    for (unsigned k = 0; k < nelem; ++k) {
        buf[k] = frac_scale(&frac, buf[k]);
    }
    unsigned time_end = timer_read(TIM_REF_DEV);
    return (time_end - time_start);
}

/* Use div_u64_by_1000000 to compute a fractional scale on all elements of buf */
uint32_t bench_div_u64_by_1000000(uint64_t *buf, size_t nelem, uint32_t num)
{
    unsigned time_start = timer_read(TIM_REF_DEV);
    for (unsigned k = 0; k < nelem; ++k) {
        uint64_t x = buf[k];
        uint64_t q = div_u64_by_1000000(x);
        uint32_t r = x - q * num;
        buf[k] = q + div_u64_by_1000000((uint64_t)r * num);
    }
    unsigned time_end = timer_read(TIM_REF_DEV);
    return (time_end - time_start);
}

/* Use 64 bit division operator on all elements of buf */
uint32_t bench_divide(uint64_t *buf, size_t nelem, uint32_t num, uint32_t den)
{
    unsigned time_start = timer_read(TIM_REF_DEV);
    for (unsigned k = 0; k < nelem; ++k) {
        uint64_t x = buf[k];
        uint64_t q = x / den;
        uint32_t r = x - q * num;
        buf[k] = q + ((uint64_t)r * num) / den;
    }
    unsigned time_end = timer_read(TIM_REF_DEV);
    return (time_end - time_start);
}

/* Floating point multiplication on all elements of buf */
uint32_t bench_double(uint64_t *buf, size_t nelem, uint32_t num, uint32_t den)
{
    double scale = ((double)num) / ((double)den);
    unsigned time_start = timer_read(TIM_REF_DEV);
    for (unsigned k = 0; k < nelem; ++k) {
        buf[k] = buf[k] * scale;
    }
    unsigned time_end = timer_read(TIM_REF_DEV);
    return (time_end - time_start);
}

void timer_cb(void *arg, int chan)
{
    (void) arg;
    (void) chan;
    puts("Warning! spurious timer interrupt");
}

void fill_buf(uint64_t *buf, size_t nelem, uint64_t seed)
{
    for (unsigned k = 0; k < nelem; ++k) {
        seed = 6364136223846793005ull * seed + 1;
        buf[k] = seed;
    }
}

uint64_t frac_long_divide(uint32_t num, uint32_t den, uint32_t *rem, int *prec);

int main(void)
{
    puts("Division benchmark");

    puts("Init timer");
    printf("TIM_REF_DEV: %u\n", (unsigned)TIM_REF_DEV);
    printf("TIM_REF_FREQ: %lu\n", (unsigned long)TIM_REF_FREQ);
    int res = timer_init(TIM_REF_DEV, TIM_REF_FREQ, timer_cb, NULL);
    if (res < 0) {
        puts("Error initializing timer!");
        while(1) {}
    }
    uint64_t seed = 12345;
    uint32_t variation = 4321;
    while (1) {
        ++seed;
        fill_buf(buf, ARRAY_LEN(buf), seed);
        uint32_t time_div = bench_div_u32_by_15625div512(buf, ARRAY_LEN(buf));
        fill_buf(buf, ARRAY_LEN(buf), seed);
        uint32_t time_frac = bench_frac(buf, ARRAY_LEN(buf), 512, 15625);
        fill_buf(buf, ARRAY_LEN(buf), seed);
        uint32_t time_divide = bench_divide(buf, ARRAY_LEN(buf), 512, 15625);
        fill_buf(buf, ARRAY_LEN(buf), seed);
        uint32_t time_double = bench_double(buf, ARRAY_LEN(buf), 512, 15625);
        printf("const (  512 /   15625) /,%%: %8" PRIu32 " frac: %8" PRIu32 " div: %8" PRIu32 " double: %8" PRIu32 "\n", time_divide, time_frac, time_div, time_double);
        uint32_t var = variation % 10000ul + 995000ul;
        fill_buf(buf, ARRAY_LEN(buf), seed);
        time_div = bench_div_u64_by_1000000(buf, ARRAY_LEN(buf), var);
        fill_buf(buf, ARRAY_LEN(buf), seed);
        time_frac = bench_frac(buf, ARRAY_LEN(buf), var, 1000000ul);
        fill_buf(buf, ARRAY_LEN(buf), seed);
        time_divide = bench_divide(buf, ARRAY_LEN(buf), var, 1000000ul);
        fill_buf(buf, ARRAY_LEN(buf), seed);
        time_double = bench_double(buf, ARRAY_LEN(buf), var, 1000000ul);
        printf("var (%7" PRIu32 " / 1000000) /,%%: %8" PRIu32 " frac: %8" PRIu32 " div: %8" PRIu32 " double: %8" PRIu32 "\n", var, time_divide, time_frac, time_div, time_double);
        fill_buf(buf, ARRAY_LEN(buf), seed);
        time_frac = bench_frac(buf, ARRAY_LEN(buf), 1000000ul, var);
        fill_buf(buf, ARRAY_LEN(buf), seed);
        time_divide = bench_divide(buf, ARRAY_LEN(buf), 1000000ul, var);
        fill_buf(buf, ARRAY_LEN(buf), seed);
        time_double = bench_double(buf, ARRAY_LEN(buf), 1000000ul, var);
        printf("var (1000000 / %7" PRIu32 ") /,%%: %8" PRIu32 " frac: %8" PRIu32 " div:   N/A    double: %8" PRIu32 "\n", var, time_divide, time_frac, time_double);
        ++variation;
    }
    return 0;
}

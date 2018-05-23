/*
 * Copyright (C) 2018 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <string.h>
#include "embUnit.h"
#include "tests-frac.h"

#include "frac.h"
#include "div.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

static const uint32_t u32_fraction_operands[] = {
    1l,
    2l,
    5l,
    10l,
    100l,
    1000l,
    1000000l,
    2000000l,
    4000000l,
    8000000l,
    16000000l,
    32000000l,
    32768l,
    9600l,
    38400l,
    115200l,
    230400l,
    460800l,
    921600l,
    4096l,
    15625l,
    1048576l,
    0x10000000l,
    0x1000000l,
    1000000000l,
    999999733l,  /* <- prime */
    512000000l,
    1024000000l,
    0x40000000l,
};

static const uint64_t u64_test_values[] = {
    0ul,
    1ul,
    10ul,
    32ul,
    15625ul,
    15625ul*5,
    (15625ul*5)+1,
    0xfffful,
    0xfffful<<10,
    1234567890ul,
    99999999ul,
    1000000ul,
    115200ul,
    38400ul,
    57600ul,
    921600ul,
    32768ul,
    16000000ul,
    15999999ul,
    32767ul,
    327679999ul,
    100000000ul,
    2100012683ul,            /* <- prime */
    0x7ffffffful,
    11111111111ull,
    0x100000000ull,
    16383999997ull,
    16383999998ull,
    16383999999ull,
    16384000000ull,
    1048575999807ull,
    1048575999808ull,
    1048575999809ull,
    0xabcdef01ull,
    0xabcdef012ull,
    0xabcdef0123ull,
    0xabcdef01234ull,
    0xabcdef012345ull,
    0xabcdef0123456ull,
    0xabcdef01234567ull,
    0x111111111111111ull,
    0xffffffffffffeeull,
    0x777777777777777ull,
    0x1111111111111111ull,
    0x7fffffffffffffffull,
    0x8000000000000000ull,
};

#define N_U32_OPERANDS (sizeof(u32_fraction_operands) / sizeof(u32_fraction_operands[0]))
#define N_U64_VALS (sizeof(u64_test_values) / sizeof(u64_test_values[0]))

static void test_frac_scale32(void)
{
    for (unsigned k = 0; k < N_U32_OPERANDS; ++k) {
        for (unsigned j = 0; j < N_U32_OPERANDS; ++j) {
            int32_t num = u32_fraction_operands[j];
            int32_t den = u32_fraction_operands[k];
            frac_t frac;
            frac_init(&frac, num, den);
            for (unsigned i = 0; i < N_U64_VALS; i++) {
                DEBUG("Scaling %" PRIu64 " by (%" PRIu32 " / %" PRIu32 "), ",
                    u64_test_values[i], num, den);
                /* Using 128 bit intermediate result by reusing libdivide functions.
                 * For a more decoupled verification, this should use
                 * 128 bit integers directly, but that is only supported by GCC
                 * on 64 bit platforms, which would prevent us from running this
                 * test on actual MCUs. We assume that
                 * libdivide__mullhi_u64,
                 * libdivide_128_div_64_to_64
                 * have been verified by other tests in the upstream test suite.
                 */
                /* high 64 bits of intermediate result */
                uint64_t hi = libdivide__mullhi_u64(num, u64_test_values[i]);
                /* low 64 bits of intermediate result */
                uint64_t lo = u64_test_values[i] * num;
                /* remainder from division, will be set to (uint64_t) -1 if the
                 * result does not fit */
                uint64_t rem = 0;
                /* compute 128/64 -> 64 division */
                uint64_t expected = libdivide_128_div_64_to_64(hi, lo, den, &rem);
                if (rem == (uint64_t) -1) {
                    /* Expected value does not fit in a 64 bit unsigned integer */
                    DEBUG("overflow, skipping\n");
                    continue;
                }
                uint64_t actual = frac_scale(&frac, u64_test_values[i]);
                DEBUG("expect %" PRIu64 ", actual %" PRIu64 "\n",  expected, actual);
                TEST_ASSERT_EQUAL_INT(
                    expected,
                    actual);
            }
        }
    }
}

Test *tests_frac_tests(void)
{
    EMB_UNIT_TESTFIXTURES(fixtures) {
        new_TestFixture(test_frac_scale32),
    };

    EMB_UNIT_TESTCALLER(frac_tests, NULL, NULL, fixtures);

    return (Test *)&frac_tests;
}

void tests_frac(void)
{
    TESTS_RUN(tests_frac_tests());
}

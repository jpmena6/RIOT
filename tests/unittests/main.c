/*
 * Copyright (C) 2014 Martine Lenders <mlenders@inf.fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include "map.h"

#include "embUnit.h"
#include "xtimer.h"

#define UNCURRY(FUN, ARGS) FUN(ARGS)
#define RUN_TEST_SUITES(...) MAP(RUN_TEST_SUITE, __VA_ARGS__)
#define RUN_TEST_SUITE(TEST_SUITE) \
    do { \
        extern void tests_##TEST_SUITE(void); \
        tests_##TEST_SUITE(); \
    } while (0);

int main(void)
{
#ifdef MODULE_XTIMER
    /* auto_init is disabled, but some modules depends on this module being initialized */
    xtimer_init();
#endif

#ifdef OUTPUT
    TextUIRunner_setOutputter(OUTPUTTER);
#endif

    TESTS_START();
#ifndef NO_TEST_SUITES
    UNCURRY(RUN_TEST_SUITES, TEST_SUITES)
#endif
    TESTS_END();

    uintptr_t *stackp = (uintptr_t *)sched_active_thread->stack_start;
    ++stackp;

    /* assume that the comparison fails before or after end of stack */
    /* assume that the stack grows "downwards" */
    while (*stackp == (uintptr_t) stackp) {
        stackp++;
    }

    uintptr_t space_free = (uintptr_t) stackp - (uintptr_t) sched_active_thread->stack_start;

    printf("Stack: %p free: %u\n", (void *)sched_active_thread->stack_start, (unsigned)space_free);

    return 0;
}

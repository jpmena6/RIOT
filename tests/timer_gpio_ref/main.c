/*
 * Copyright (C) 2018 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Peripheral timer test application
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#include <stdint.h>

#include "periph/timer.h"
#include "periph/gpio.h"
#ifdef MODULE_PERIPH_ADC
#include "periph/adc.h"
#endif
#include "mutex.h"
#include "fmt.h"
#include "matstat.h"
#ifdef MODULE_DS3234
#include "ds3234.h"
#include "ds3234_params.h"
#endif

#ifndef TEST_TIMER
#define TEST_TIMER          TIMER_LPTMR_DEV(0)
#endif
#ifndef TEST_PIN
#define TEST_PIN            GPIO_PIN(PORT_C, 5)
#endif
#ifndef TEST_TIMER_MASK
#define TEST_TIMER_MASK     0xffffu
#endif
#ifndef TEST_FREQ
#define TEST_FREQ           32768u
#endif
#ifdef MODULE_PERIPH_ADC
#ifndef TEST_ADC_LINE
#define TEST_ADC_LINE       ADC_LINE(0)
#endif
#endif

typedef struct {
    matstat_state_t stats;
    int32_t last_value;
    unsigned int last_time;
    mutex_t mtx;
    int adc;
} test_state_t;

test_state_t test_state = {
    .stats = MATSTAT_STATE_INIT,
    .last_value = 0,
    .last_time = 0,
    .mtx = MUTEX_INIT_LOCKED,
    .adc = 0,
};

static void pin_cb(void *arg)
{
    test_state_t *state = arg;
    unsigned int now = timer_read(TEST_TIMER);
#ifdef MODULE_PERIPH_ADC
    state->adc = adc_sample(TEST_ADC_LINE, ADC_RES_16BIT);
#endif
    if (state->last_time == 0) {
        /* Only set last, don't count for the first tick */
        state->last_time = now;
        return;
    }
    int32_t diff = TEST_TIMER_MASK & (now - state->last_time);
    matstat_add(&state->stats, diff * 1000);
    state->last_time = now;
    state->last_value = diff;
    mutex_unlock(&state->mtx);
}

static void timer_cb(void *arg, int chan)
{
    (void) arg;
    (void) chan;
    print_str("Warning: Timer CB!\n");
    return;
}

static void enable_pps_devs(void)
{
#ifdef MODULE_DS3234
    for (unsigned k = 0; k < (sizeof(ds3234_params) / sizeof(ds3234_params[0])); ++k) {
        print_str("Init #");
        print_u32_dec(k);
        print_str("... ");
        int res = ds3234_pps_init(&ds3234_params[k]);
        if (res == 0) {
            print_str("[OK]\n");
        }
        else {
            print_str("[Failed]\n");
        }
    }

    print_str("DS3234 init done.\n");
#endif /* MODULE_DS3234 */
}

int main(void)
{
    print_str("\nPPS pin input test for timer\n");

    enable_pps_devs();
    gpio_init_int(TEST_PIN, GPIO_IN_PU, GPIO_RISING, pin_cb, &test_state);
    timer_init(TEST_TIMER, TEST_FREQ, timer_cb, NULL);
#ifdef MODULE_PERIPH_ADC
    adc_init(TEST_ADC_LINE);
#endif

    while (1) {
        mutex_lock(&test_state.mtx);
        int32_t mean = matstat_mean(&test_state.stats);
        uint64_t var = matstat_variance(&test_state.stats);
        char buf[20];
        print_str("Tick: ");
        print(buf, fmt_lpad(buf, fmt_s32_dec(buf, (int32_t)test_state.last_value), 7, ' '));
        print_str(" adc = ");
        print(buf, fmt_lpad(buf, fmt_s32_dec(buf, test_state.adc), 7, ' '));
        print_str(" mean = ");
        print(buf, fmt_lpad(buf, fmt_s32_dfp(buf, mean, -3), 7, ' '));
        print_str(" var = ");
        print(buf, fmt_lpad(buf, fmt_s64_dec(buf, var), 7, ' '));
        print("\n", 1);
    }

    return 0;
}

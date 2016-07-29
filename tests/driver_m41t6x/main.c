/*
 * Copyright (C) 2016 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup tests
 * @{
 *
 * @file
 * @brief       Test application for the M41T6x real time clock driver
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 *
 * @}
 */

#ifndef TEST_M41T6X_I2C
#error "TEST_M41T6X_I2C not defined"
#endif
#ifndef TEST_M41T6X_ADDR
#error "TEST_M41T6X_ADDR not defined"
#endif

#include <stdio.h>
#include <time.h>

#include "xtimer.h"
#include "timex.h"
#include "m41t6x.h"
#include "periph/i2c.h"

#define SLEEP    (1 * SEC_IN_USEC)

static int _print_time(const struct tm *time)
{
    printf("%04i-%02i-%02i %02i:%02i:%02i\n",
            time->tm_year + 1900, time->tm_mon + 1, time->tm_mday,
            time->tm_hour, time->tm_min, time->tm_sec
          );
    return 0;
}

int main(void)
{
    int res;

    static const struct tm test_time_initial = {
        .tm_sec  =  56,
        .tm_min  =  58,
        .tm_hour =  12,
        .tm_mday =  31,
        .tm_mon  =   6, /* Jan = 0 */
        .tm_year = 116, /* 1900 + 116 = 2016 */
        .tm_wday =   0, /* Sunday = 0 */
        .tm_yday = 212, /* 1 Jan = 0 */
        .tm_isdst = -1, /* unknown DST status */
    };

    static const m41t6x_t dev = {
        .i2c = TEST_M41T6X_I2C,
        .addr = TEST_M41T6X_ADDR,
        .irq_pin = TEST_M41T6X_IRQ,
    };

    puts("M41T6x real time clock test application\n");
    printf("Initializing I2C_%i... ", TEST_M41T6X_I2C);
    res = i2c_init_master(TEST_M41T6X_I2C, I2C_SPEED_FAST);
    if (res < 0) {
        puts("[Failed]");
        return -1;
    }
    puts("[OK]");

    printf("Initializing M41T6x RTC at I2C_%i, address 0x%02x... ",
        TEST_M41T6X_I2C, TEST_M41T6X_ADDR);
    res = m41t6x_init(&dev);
    if (res != 0) {
        puts("[Failed]");
        return -1;
    }
    puts("[OK]");

    printf("Setting time to ");
    _print_time(&test_time_initial);
    puts("");

    res = m41t6x_set_time(&dev, &test_time_initial);
    if (res != 0) {
        puts("[Failed]");
        return -1;
    }
    puts("[OK]");

    while (1) {
        struct tm now;
        xtimer_usleep(SLEEP);

        res = m41t6x_get_time(&dev, &now);
        if (res < 0) {
            printf("Communication error: %d\n", res);
            continue;
        }
        printf("m41t6x: ");
        _print_time(&now);
        puts("");
    }

    return 0;
}

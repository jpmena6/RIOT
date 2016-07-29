/*
 * Copyright (C) 2016 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @{
 * @file
 * @brief       ST M41T6x real time clock device driver implementation
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#include <stdint.h>
#include <time.h>
#include "m41t6x.h"
#include "timex.h"
#include "xtimer.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

/**
 * @internal
 * @brief M41T6x hardware register addresses
 */
enum {
    M41T6X_REG_SUBSECOND     = 0x00,
    M41T6X_REG_SECONDS       = 0x01,
    M41T6X_REG_MINUTES       = 0x02,
    M41T6X_REG_HOURS         = 0x03,
    M41T6X_REG_DAY           = 0x04,
    M41T6X_REG_DATE          = 0x05,
    M41T6X_REG_CENTURY_MONTH = 0x06,
    M41T6X_REG_YEAR          = 0x07,
    M41T6X_REG_CALIBRATION   = 0x08,
    M41T6X_REG_WATCHDOG      = 0x09,
    M41T6X_REG_ALARM_MONTH   = 0x0A,
    M41T6X_REG_ALARM_DATE    = 0x0B,
    M41T6X_REG_ALARM_HOURS   = 0x0C,
    M41T6X_REG_ALARM_MINUTES = 0x0D,
    M41T6X_REG_ALARM_SECONDS = 0x0E,
    M41T6X_REG_FLAGS         = 0x0F,
};

#define M41T6X_MONTH_MASK     (0x1F)
#define M41T6X_DATE_MASK      (0x3F)
#define M41T6X_DAY_MASK       (0x07)
#define M41T6X_HOURS_MASK     (0x3F)
#define M41T6X_SECONDS_MASK   (0x7F)
#define M41T6X_MINUTES_MASK   (0x7F)
#define M41T6X_CENTURY_SHIFT  (6)
#define M41T6X_AFE_MASK       (0x80)
#define M41T6X_FLAG_OF        (1 << 2)

#define M41T6X_CLOCK_SIZE (M41T6X_REG_YEAR - M41T6X_REG_SUBSECOND + 1)
#define M41T6X_ALARM_SIZE (M41T6X_REG_ALARM_SECONDS - M41T6X_REG_ALARM_MONTH + 1)

#define M41T6X_INIT_BACKOFF  (SEC_IN_USEC / 8)
#define M41T6X_INIT_RETRIES  (4 * SEC_IN_USEC / M41T6X_INIT_BACKOFF)

/**
 * @internal
 * @brief M41T6x read register
 */
static inline int _m41t6x_read(const m41t6x_t *dev, uint8_t start_addr, void *dest, size_t len)
{
    i2c_acquire(dev->i2c);
    int res = i2c_read_regs(dev->i2c, dev->addr, start_addr, dest, len);
    i2c_release(dev->i2c);
    return res;
}

/**
 * @internal
 * @brief M41T6x write register
 */
static inline int _m41t6x_write(const m41t6x_t *dev, uint8_t start_addr, const void *src, size_t len)
{
    i2c_acquire(dev->i2c);
    int res = i2c_write_regs(dev->i2c, dev->addr, start_addr, src, len);
    i2c_release(dev->i2c);
    return res;
}

/**
 * @internal
 * @brief Convert binary to BCD
 */
static inline uint8_t _bcd(uint8_t bin)
{
    return (((bin / 10) << 4) | (bin % 10));
}

/**
 * @internal
 * @brief Convert BCD to binary
 */
static inline uint8_t _bin(uint8_t bcd)
{
    return (bcd & 0x0f) + ((bcd >> 4) * 10);
}

int m41t6x_init(const m41t6x_t *rtc)
{
    DEBUG("m41t6x_init\n");
    uint8_t flags = 0;
    unsigned int i;

    for (i = 0; i < M41T6X_INIT_RETRIES; ++i) {
        int res = _m41t6x_read(rtc, M41T6X_REG_FLAGS, &flags, 1);
        if (res < 0) {
            return res;
        }
        DEBUG("m41t6x_init: flags=%x\n", (unsigned int) flags);
        if ((flags & M41T6X_FLAG_OF) == 0) {
            break;
        }
        /* Oscillator failure flag is set at power on */
        DEBUG("m41t6x_init: OF\n");
        flags &= ~(M41T6X_FLAG_OF);
        res = _m41t6x_write(rtc, M41T6X_REG_FLAGS, &flags, 1);
        if (res < 0) {
            return res;
        }
        DEBUG("m41t6x_init: retry\n");
        xtimer_usleep(M41T6X_INIT_BACKOFF);
    }
    if (i >= M41T6X_INIT_RETRIES) {
        return -1;
    }

    if (rtc->irq_pin != GPIO_UNDEF) {
        int res = gpio_init(rtc->irq_pin, GPIO_IN);
        if (res < 0) {
            return res;
        }
    }
    return 0;
}

int m41t6x_set_time(const m41t6x_t *rtc, const struct tm *time)
{
    DEBUG("m41t6x_set_time\n");
    uint8_t buf[M41T6X_CLOCK_SIZE];
    int res = _m41t6x_read(rtc, M41T6X_REG_SUBSECOND, &buf[0], M41T6X_CLOCK_SIZE);
    if (res < 0) {
        return res;
    }

    uint8_t century = 0;
    int year = time->tm_year;
    while (year > 100) {
        ++century;
        year -= 100;
    }

    /* Assuming *time was valid to begin with, i.e. no tm_sec = 65, tm_min = 100 etc */

    buf[0] = 0x00; /* subsecond is only valid to set to 0 */
    buf[1] = (buf[1] & ~(M41T6X_SECONDS_MASK)) | _bcd(time->tm_sec);
    buf[2] = (buf[2] & ~(M41T6X_MINUTES_MASK)) | _bcd(time->tm_min);
    buf[3] = (buf[3] & ~(M41T6X_HOURS_MASK))   | _bcd(time->tm_hour);
    buf[4] = (buf[4] & ~(M41T6X_DAY_MASK))     | _bcd(time->tm_wday + 1);
    buf[5] = (buf[5] & ~(M41T6X_DATE_MASK))    | _bcd(time->tm_mday);
    buf[6] = (century << M41T6X_CENTURY_SHIFT) | _bcd(time->tm_mon + 1);
    buf[7] = _bcd(year);

    DEBUG("m41t6x_set_time: %02x %02x %02x %02x %02x %02x %02x %02x\n",
        (unsigned int)buf[0], (unsigned int)buf[1],
        (unsigned int)buf[2], (unsigned int)buf[3],
        (unsigned int)buf[4], (unsigned int)buf[5],
        (unsigned int)buf[6], (unsigned int)buf[7]);

    return _m41t6x_write(rtc, M41T6X_REG_SUBSECOND, &buf[0], M41T6X_CLOCK_SIZE);
}

int m41t6x_get_time(const m41t6x_t *rtc, struct tm *time)
{
    DEBUG("m41t6x_get_time\n");
    uint8_t buf[M41T6X_CLOCK_SIZE];
    int res = _m41t6x_read(rtc, M41T6X_REG_SUBSECOND, &buf[0], M41T6X_CLOCK_SIZE);
    if (res < 0) {
        return res;
    }

    DEBUG("m41t6x_get_time: %02x %02x %02x %02x %02x %02x %02x %02x\n",
        (unsigned int)buf[0], (unsigned int)buf[1],
        (unsigned int)buf[2], (unsigned int)buf[3],
        (unsigned int)buf[4], (unsigned int)buf[5],
        (unsigned int)buf[6], (unsigned int)buf[7]);

    /* buf[0]: struct tm does not have a subsecond field */
    time->tm_sec  = _bin(buf[1] & M41T6X_SECONDS_MASK);
    time->tm_min  = _bin(buf[2] & M41T6X_MINUTES_MASK);
    time->tm_hour = _bin(buf[3] & M41T6X_HOURS_MASK);
    time->tm_wday = _bin(buf[4] & M41T6X_DAY_MASK) - 1;
    time->tm_mday = _bin(buf[5] & M41T6X_DATE_MASK);
    time->tm_mon  = _bin(buf[6] & M41T6X_MONTH_MASK) - 1;
    time->tm_year = _bin(buf[7]) + (buf[6] >> M41T6X_CENTURY_SHIFT) * 100;

    return 0;
}

int m41t6x_set_alarm(const m41t6x_t *rtc, const struct tm *time, rtc_alarm_cb_t cb, void *arg)
{
    DEBUG("m41t6x_set_alarm\n");
    //~ uint8_t buf[M41T6X_ALARM_SIZE];

    return 0;
}

int m41t6x_get_alarm(const m41t6x_t *rtc, struct tm *time)
{
    DEBUG("m41t6x_get_alarm\n");
    uint8_t buf[M41T6X_ALARM_SIZE];

    int res = _m41t6x_read(rtc, M41T6X_REG_ALARM_MONTH, &buf[0], M41T6X_ALARM_SIZE);
    if (res < 0) {
        return res;
    }

    if ((buf[1] & M41T6X_DATE_MASK) == 0) {
        /* no alarm set */
        return -1;
    }

    time->tm_sec  = _bin(buf[4] & M41T6X_SECONDS_MASK);
    time->tm_min  = _bin(buf[3] & M41T6X_MINUTES_MASK);
    time->tm_hour = _bin(buf[2] & M41T6X_HOURS_MASK);
    time->tm_mday = _bin(buf[1] & M41T6X_DATE_MASK) - 1;
    time->tm_mon  = _bin(buf[0] & M41T6X_MONTH_MASK) - 1;
    time->tm_year = 0;

    return 0;
}

int m41t6x_clear_alarm(const m41t6x_t *rtc)
{
    DEBUG("m41t6x_clear_alarm\n");
    uint8_t buf[M41T6X_ALARM_SIZE];

    int res = _m41t6x_read(rtc, M41T6X_REG_ALARM_MONTH, &buf[0], M41T6X_ALARM_SIZE);
    if (res < 0) {
        return res;
    }

    /* To disable the alarm: Clear the RPT bits and set alarm day of month to 0 */
    buf[0] &= ~(M41T6X_AFE_MASK); /* Clear the AFE bit */
    buf[1] = 0; /* Clear RPT4,5 bits and set day of month to 0*/
    /* Clear RPT3-RPT1 bits */
    buf[2] &= M41T6X_HOURS_MASK;
    buf[3] &= M41T6X_MINUTES_MASK;
    buf[4] &= M41T6X_SECONDS_MASK;
    res = _m41t6x_write(rtc, M41T6X_REG_ALARM_MONTH, &buf[0], M41T6X_ALARM_SIZE);
    if (res < 0) {
        return res;
    }
    return 0;
}

/** @} */

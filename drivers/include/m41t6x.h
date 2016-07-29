/*
 * Copyright (C) 2016 Eistec AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @defgroup    drivers_time_m41t6x M41T6x Real Time Clocks
 * @ingroup     drivers
 * @brief       Device driver for ST M41T6x real time clocks
 *
 * @note
 * The values used for setting and getting the time/alarm should
 * conform to the `struct tm` specification.
 * @see http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/time.h.html
 *
 * @{
 * @file
 * @brief       ST M41T6x real time clock device driver interface definitions
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#ifndef M41T6X_H
#define M41T6X_H

#include <stdint.h>
#include <time.h>
#include "periph/rtc.h"
#include "periph/i2c.h"
#include "periph/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Device descriptor for M41T6x devices
 *
 * @note set @p irq_pin to GPIO_UNDEF on M41T64 (no IRQ signal)
 */
typedef struct {
    i2c_t i2c;      /**< I2C bus the device is connected to */
    uint8_t addr;   /**< the slave address of the device on the I2C bus */
    gpio_t irq_pin; /**< GPIO pin on the MCU that the IRQ signal is connected to */
} m41t6x_t;

/**
 * @brief Initialize RTC module
 *
 * @param[in] rtc           Pointer to device descriptor
 *
 * @return  0 on success
 * @return <0 on error
 */
int m41t6x_init(const m41t6x_t *rtc);

/**
 * @brief Set RTC to given time.
 *
 * @param[in] rtc           Pointer to device descriptor
 * @param[in] time          Pointer to the struct holding the time to set.
 *
 * @return  0 on success
 * @return <0 on error
 */
int m41t6x_set_time(const m41t6x_t *rtc, const struct tm *time);

/**
 * @brief Get current RTC time.
 *
 * @param[out] time         Pointer to the struct to write the time to.
 *
 * @return  0 on success
 * @return <0 on error
 */
int m41t6x_get_time(const m41t6x_t *rtc, struct tm *time);

/**
 * @brief Set an alarm for RTC to the specified value.
 *
 * @note Any already set alarm will be overwritten.
 *
 * @note The alarm function is fairly useless on M41T64 where there is no hardware signal for IRQ.
 *
 * @param[in] rtc           Pointer to device descriptor
 * @param[in] time          The time to trigger an alarm when hit
 * @param[in] cb            Callback executed when alarm is triggered
 * @param[in] arg           Argument passed to callback
 *
 * @return  0 on success
 * @return <0 on error
 */
int m41t6x_set_alarm(const m41t6x_t *rtc, const struct tm *time, rtc_alarm_cb_t cb, void *arg);

/**
 * @brief Gets the current alarm setting
 *
 * @param[in]   rtc         Pointer to device descriptor
 * @param[out]  time        Pointer to structure to receive alarm time
 *
 * @return  0 on success
 * @return <0 on error
 */
int m41t6x_get_alarm(const m41t6x_t *rtc, struct tm *time);

/**
 * @brief Clear any set alarm, do nothing if nothing set
 *
 * @param[in]   rtc         Pointer to device descriptor
 *
 * @return  0 on success
 * @return <0 on error
 */
int m41t6x_clear_alarm(const m41t6x_t *rtc);

#ifdef __cplusplus
}
#endif

#endif /* M41T6X_H */
/** @} */

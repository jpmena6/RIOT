/*
 * Copyright (C) 2018 SKF AB
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_tacho Tachometer
 * @ingroup     drivers_sensors
 *
 * @brief       GPIO based tachometer driver
 *
 * @{
 *
 * @file
 * @brief       Tachometer API
 *
 * @author      Joakim Nohlgård <joakim.nohlgard@eistec.se>
 */

#ifndef TACHO_H
#define TACHO_H

#include <stddef.h>
#include <stdint.h>
#include "periph/gpio.h"
#include "xtimer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Hardware interface parameters
 */
typedef struct {
    gpio_t       gpio;          /**< GPIO pin to use */
    gpio_flank_t gpio_flank;    /**< GPIO flank option */
    gpio_mode_t  gpio_mode;     /**< GPIO pull resistor configuration */
} tacho_params_t;

/**
 * @brief   State vector element
 */
typedef struct {
    xtimer_ticks32_t time_start;    /**< time of first pulse */
    xtimer_ticks32_t time_end;      /**< time of last pulse */
    unsigned count;                 /**< number of pulses in this interval */
} tacho_interval_t;

/**
 * @brief   Tachometer state
 */
typedef struct {
    tacho_interval_t *bufs; /**< Circular buffer of measurement intervals */
    xtimer_ticks32_t min_duration; /**< Duration to count before switching buffers */
    unsigned num_bufs; /**< number of elements in bufs array */
    unsigned idx; /**< current buffer being updated */
} tacho_t;

/**
 * @brief   Initialize a tachometer
 *
 * This function will initialize the hardware and reset the state of the driver.
 *
 * @pre The members @c bufs, @c num_bufs, @c min_duration in the device descriptor
 * @p dev must be initialized by the user before calling this function.
 *
 * The contents of @p dev->bufs will be cleared to get a clean state.
 *
 * @param[out]  dev         device descriptor
 * @param[in]   params      configuration parameters
 *
 * @return               0 on success
 * @return              <0 on error
 */
int tacho_init(tacho_t *dev, const tacho_params_t *params);

/**
 * @brief   Get sum of the last n tacho readings as a fraction
 *
 * This will return the sum of the last @p n filled tacho buffers, yielding a
 * form of moving average.
 *
 * Computing RPM:
 *
 *     rpm = ((uint64_t)count * 60000000ul) / duration;
 *
 * @param[in]   dev         device descriptor of sensor
 * @param[in]   n           number of buffers to use
 * @param[out]  count       pulse count
 * @param[out]  duration    duration for the count, in microseconds
 */
void tacho_read(const tacho_t *dev, unsigned n, unsigned *count, uint32_t *duration);

#ifdef __cplusplus
}
#endif

#endif /* PULSE_COUNTER_H */
/** @} */

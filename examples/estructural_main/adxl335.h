/**
 *	@author	Tomas Herrera C
 *
 *	@brief	in 1 block we will save 200 samples
 */

#ifndef ACC_H
#define ACC_H

#include <stdint.h>
#include <board.h>
#include "periph/gpio.h"

#define ADXL3558_ADDR_1 	0x1d /* ASEL=0 */
#define ADXL3558_ADDR_2 	0x53 /* ASEL=1 */
/* Set speed to .speed = I2C_SPEED_HIGH, in periph_conf.h of the board to 3.4MHz */

/* Registers */
#define ADXL3558_DEVID_AD_REG 0x00
#define ADXL3558_PARTID_REG 0x02
#define ADXL3558_STATUS_REG 0x04

#define ADXL3558_XDATA3_REG 0x08 /* MSB [7:0]*/
#define ADXL3558_XDATA2_REG 0x09
#define ADXL3558_XDATA1_REG 0x0a /* LSB [7:4], [3:0] RSV*/

#define ADXL3558_YDATA3_REG 0x0b /* MSB [7:0]*/
#define ADXL3558_YDATA2_REG 0x0c
#define ADXL3558_YDATA1_REG 0x0d /* LSB [7:4], [3:0] RSV*/

#define ADXL3558_ZDATA3_REG 0x0e /* MSB [7:0]*/
#define ADXL3558_ZDATA2_REG 0x0f
#define ADXL3558_ZDATA1_REG 0x10 /* LSB [7:4], [3:0] RSV*/

#define ADXL3558_POWER_CTL_REG 0x2d

/* Register values */
#define ADXL3558_DEVID_AD_VAL 0xad
#define ADXL3558_PARTID_VAL 0xed


#define ADXL335_SPI_DEV 	SPI_DEV(0)
#define ADXL335_SPI_MODE 	SPI_MODE_0
#define ADXL335_SPI_CS 		GPIO_PIN(PORT_A,18)
#define ADXL335_SPI_CLK		SPI_CLK_1MHZ

#define ADXL3558_POWER_CTL_VAL 0b10 /* TEMP_OFF = 1, STANDBY = 0 */


/* SPI */
#define SPI_READ 0x1
#define SPI_WRITE 0x0

/* ADXL335 */
#define LSB2g (const double) 1.0/256000

void adxl335_init(void);


void adxl335_get(uint32_t * x, uint32_t * y, uint32_t * z);

uint8_t adxl335_earthquake(void);

int32_t bit20_to_int32(uint32_t s);

/*1.0/256000*/
double bits_to_g(int32_t s);


#endif

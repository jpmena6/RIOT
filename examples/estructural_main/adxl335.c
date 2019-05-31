#include "adxl335.h"
#include "periph/spi.h"
#include "periph/gpio.h"
#include <string.h>
#include <unistd.h>


static uint8_t adxl335_probe(void)
{
	uint8_t partid;
	spi_acquire(ADXL335_SPI_DEV,ADXL335_SPI_CS,ADXL335_SPI_MODE,ADXL335_SPI_CLK);
	spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 1, (ADXL3558_PARTID_REG<<1)+SPI_READ);
	partid = spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 0,0);
	spi_release(ADXL335_SPI_DEV);
	char buff[10];
	sprintf(buff, "%d", partid);
	puts(buff);
	return (partid == ADXL3558_PARTID_VAL);
}

static void adxl355_start(void)
{
	spi_acquire(ADXL335_SPI_DEV,ADXL335_SPI_CS,ADXL335_SPI_MODE,ADXL335_SPI_CLK);

	spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 1, (ADXL3558_POWER_CTL_REG<<1)+SPI_WRITE);
	spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 0,  ADXL3558_POWER_CTL_VAL);

	spi_release(ADXL335_SPI_DEV);
}

void adxl335_init(void)
{
	spi_init(ADXL335_SPI_DEV);
	spi_init_cs(ADXL335_SPI_DEV,ADXL335_SPI_CS);
	if (!adxl335_probe())
		puts("Error");
	else
		puts("OK");
	adxl355_start();
}

int32_t bit20_to_int32(uint32_t s)
{

	if (s & 0x80000){
		return  (s | 0xfff80000); /* move the sign bit */
	}
	return s;

}

void adxl335_get(uint32_t * x, uint32_t * y, uint32_t * z)
{
	
	//uint32_t s;
	spi_acquire(ADXL335_SPI_DEV,ADXL335_SPI_CS,ADXL335_SPI_MODE,ADXL335_SPI_CLK);
	spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 1, (ADXL3558_XDATA3_REG<<1)+SPI_READ);
	*x = (spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 1,0) << 12);
	*x |= (spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 1,0) << 4);
	*x |= (spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 0,0) >> 4);
	//*x = bit20_to_int32(*x);

	spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 1, (ADXL3558_YDATA3_REG<<1)+SPI_READ);
	*y = (spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 1,0) << 12);
	*y |= (spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 1,0) << 4);
	*y |= (spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 0,0) >> 4);
	//*y = bit20_to_int32(*y);

	spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 1, (ADXL3558_ZDATA3_REG<<1)+SPI_READ);
	*z = (spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 1,0) << 12);
	*z |= (spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 1,0) << 4);
	*z |= (spi_transfer_byte(ADXL335_SPI_DEV, ADXL335_SPI_CS, 0,0) >> 4);
	//*z = bit20_to_int32(*z);
	spi_release(ADXL335_SPI_DEV);

}

double bits_to_g(int32_t s)
{
	return (LSB2g*s);

}




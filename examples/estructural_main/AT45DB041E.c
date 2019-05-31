#include "AT45DB041E.h"
#include "periph/spi.h"
#include "periph/gpio.h"
#include <string.h>
#include <unistd.h>
#ifdef DEBUG_AT45
#include <xtimer.h>
#endif


/* 1 of ready, 0 if busy */
static uint8_t AT45DB041E_is_ready(void){

	uint8_t cmd[3] = {0xd7,0x00,0x00};
	uint8_t stat[1+2];
	spi_acquire(FLASH_SPI_DEV,FLASH_SPI_CS,FLASH_SPI_MODE,FLASH_SPI_CLK);
	spi_transfer_bytes(FLASH_SPI_DEV, FLASH_SPI_CS, 0, cmd, stat, 1+2);
	spi_release(FLASH_SPI_DEV);
	//printf("0x%02x:%02x:%02x\r\n",stat[0],stat[1],stat[2]);
	return (stat[1]&0x80);

} 

/* blocks until device is ready */
static void AT45DB041E_release(void)
{	
	while (!AT45DB041E_is_ready());;
	
}

/* Add 41k resistor to PC18 pin to GND */
void AT45DB041E_init(void)
{
	//gpio_init(GPIO_PIN(PORT_C,1),GPIO_IN_PD);
	spi_init(FLASH_SPI_DEV);
	spi_init_cs(FLASH_SPI_DEV,FLASH_SPI_CS);
	AT45DB041E_release();
}

/* 17 seconds max */
void AT45DB041E_chip_erase(void)
{
	const uint8_t chip_erase[] = {0xc7, 0x94, 0x80, 0x9a};
	const uint8_t len = sizeof(chip_erase);
	spi_acquire(FLASH_SPI_DEV,FLASH_SPI_CS,FLASH_SPI_MODE,FLASH_SPI_CLK);
	spi_transfer_bytes(FLASH_SPI_DEV, FLASH_SPI_CS, 0, chip_erase, NULL, len);
	spi_release(FLASH_SPI_DEV);
	AT45DB041E_release();
}

void AT45DB041E_page_write(uint16_t page, void * wbuff, uint16_t buf_size)
{
	/* Main Memory Byte/Page Program through Buffer 1 without Built-In Erase */
	/* 4 dummy bits, 11 page address bits, 9 buffer address bits that select the first byte in the buffer to be written */
	page = page & 0x7ff; /* page is 11 bit */
	uint8_t addr_2 = page >> 7; 
	uint8_t addr_1 = (page & 0x7f) << 1;
	const uint8_t addr_0 = 0x00;
	uint8_t cmd[4] = {0x02, addr_2, addr_1, addr_0};
	spi_acquire(FLASH_SPI_DEV,FLASH_SPI_CS,FLASH_SPI_MODE,FLASH_SPI_CLK);
	spi_transfer_bytes(FLASH_SPI_DEV, FLASH_SPI_CS, 1, cmd, 	NULL, 	4);
	spi_transfer_bytes(FLASH_SPI_DEV, FLASH_SPI_CS, 0, wbuff, 	NULL, 	buf_size);
	spi_release(FLASH_SPI_DEV);
	AT45DB041E_release();
}

void AT45DB041E_page_read(uint16_t page, void * buff, uint16_t buf_size)
{
	/*Main Memory Page Read*/
	/* 4 dummy bits, 11 page address bits, 9 buffer address bits that select the first byte in the buffer to be written */
	page = page & 0x7ff; /* 11 bites */
	uint8_t addr_2 = (page >> 7);
	uint8_t addr_1 = (page & 0x7f) << 1;
	uint8_t addr_0 = 0x00;
	uint8_t cmd[4] = {0xd2, addr_2, addr_1, addr_0};
	spi_acquire(FLASH_SPI_DEV,FLASH_SPI_CS,FLASH_SPI_MODE,FLASH_SPI_CLK);
	spi_transfer_bytes(FLASH_SPI_DEV, FLASH_SPI_CS, 1, cmd, 	NULL, 4);
	spi_transfer_bytes(FLASH_SPI_DEV, FLASH_SPI_CS, 1, cmd, 	NULL, 4); /* 4 dummy bytes */
	spi_transfer_bytes(FLASH_SPI_DEV, FLASH_SPI_CS, 0, NULL, 	buff, buf_size);
	spi_release(FLASH_SPI_DEV);
	AT45DB041E_release();
}


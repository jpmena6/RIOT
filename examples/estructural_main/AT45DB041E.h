/**
 *	@author	Tomas Herrera C
 *
 *	@brief	in 1 block we will save 200 samples
 */

#ifndef AT45DB041E_H
#define AT45DB041E_H

#include <stdint.h>
#include <board.h>

/* from board.h */

/*

#define FRDM_NOR_SPI_DEV               SPI_DEV(0)
#define FRDM_NOR_SPI_CLK               SPI_CLK_5MHZ
#define FRDM_NOR_SPI_CS                SPI_HWCS(0)

*/

#define FLASH_SPI_DEV 	FRDM_NOR_SPI_DEV
#define FLASH_SPI_MODE 	SPI_MODE_0
#define FLASH_SPI_CS 	FRDM_NOR_SPI_CS
#define FLASH_SPI_CLK	FRDM_NOR_SPI_CLK

//#define DEBUG_AT45

/* Add 41k resistor to PC18 pin to GND */
void AT45DB041E_init(void);



/* 4Mbit */
void AT45DB041E_chip_erase(void);

/**
 * 	@brief	page write without built-In Erase 
 *
 *	@param[in]	page		page number 0,2047	
 *	@param[in]	buf_size	buffer size 1,264
 */
void AT45DB041E_page_write(uint16_t page, void * wbuff, uint16_t buf_size);


void AT45DB041E_page_read(uint16_t page, void * buff, uint16_t buf_size);




#endif

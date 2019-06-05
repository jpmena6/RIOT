#ifndef H_APP_ESTRUCTURAL_H
#define H_APP_ESTRUCTURAL_H

#include <stdint.h>


#define SAMPLE_TIME_US (5000)
#define HISTORY_TIME_S (5) /* prefer a number such that HISTORY_TIME_S*SAMPLES_PER_SECOND%20==0 */
//#define EARTHQUAKE_ADXL335X 68000000000
#define EARTHQUAKE_THRESHOLD 1.2

/* to use lowpassfilter define this to 1 */
#define LOWPASSFILTER 0
/* if using lowpass then use THRESHOLD 1.01 TAU 20000 PROB 0.9 */
#define EARTHQUAKE_TAU_US 20000.0
#define EARTHQUAKE_THRESHOLD_PROBABILITY 0.9

#define BUG_TIME_US 1000000  /* the time the green led goes on */
#define SAMPLES_PER_SECOND (1000000/SAMPLE_TIME_US)
#define DELAY_UDP_SENDS_US 40000

#ifdef MODULE_KW41ZRF

#define APP_LED_BLUE 	GPIO_PIN(PORT_A,18)
#define APP_LED_GREEN 	GPIO_PIN(PORT_A,19)
#define APP_LED_RED		GPIO_PIN(PORT_C,1 )
#define APP_GPIO_PIN	GPIO_PIN(PORT_C,6)



#endif

void led_green(uint8_t turn_on);
void led_blue(uint8_t turn_on);
void led_red(uint8_t turn_on);


typedef struct sample_t{
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t ntp_time;
}sample_t;

typedef struct save_sd_t{

	uint16_t sample_counter;
	sample_t * sample_buffer;

}save_sd_t;

extern int64_t ApplyDelay;
extern int64_t TheDeltaTime;
extern int16_t RequestedPage;

void estructural_init(void);

/* sets the time */
void estructural_set_counter(uint32_t ntp_time);

/* called every SAMPLE_TIME_US
 * the pid is the process to notify when to save to SD
 *
 */

void estructural_save_data(sample_t * sample ,void * pid);

/* 
 * @brief save msg content to flash
 *
 *	@return  1 if the flash is full, 0 if there is still space
 * @note this function assumes sample_buffer is full with SAMPLES_PER_SECOND*HISTORY_TIME_S samples
 */

uint8_t save_to_flash(void * msg);

/*
 *	@brief checks if there is a saved earthquake in flash (only in first page tho)
 *
 */
uint8_t have_saved_earthquake(void);

#endif /* _APP_ESTRUCTURAL_H */

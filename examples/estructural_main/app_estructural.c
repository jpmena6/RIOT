#include "app_estructural.h"
#include "periph/gpio.h"
#include "adxl335.h"
#include "AT45DB041E.h"
#include <xtimer.h>
#include <fmt.h>


void led_red(uint8_t turn_on)
{
	#ifdef MODULE_KW41ZRF
	gpio_init(APP_LED_RED, GPIO_OUT);
	if (turn_on)
		gpio_clear(APP_LED_RED);
	else
		gpio_set(APP_LED_RED);
	#endif
}

void led_blue(uint8_t turn_on)
{
	#ifdef MODULE_KW41ZRF
	gpio_init(APP_LED_BLUE, GPIO_OUT);
	if (turn_on)
		gpio_clear(APP_LED_BLUE);
	else
		gpio_set(APP_LED_BLUE);
	#endif
}

void led_green(uint8_t turn_on)
{
	#ifdef MODULE_KW41ZRF
	gpio_init(APP_LED_GREEN, GPIO_OUT);
	if (turn_on)
		gpio_clear(APP_LED_GREEN);
	else
		gpio_set(APP_LED_GREEN);
	#endif
}


void estructural_set_counter(uint32_t ntp_time)
{
	
	uint32_t local_time = xtimer_now_usec();
	TheDeltaTime = ntp_time - local_time; /* the delta to reach realtime */
	ApplyDelay = 1;

}

static sample_t BigBuffer1[SAMPLES_PER_SECOND*HISTORY_TIME_S];
static sample_t BigBuffer2[SAMPLES_PER_SECOND*HISTORY_TIME_S];

static sample_t * FlashBuffer;
static sample_t * RealBuffer;

static float EarthquakeThreshold = 0;

static void calibrate_earthquake(void)
{
	sample_t sample;
	
	const float average = 100.0;
	uint8_t i;
	EarthquakeThreshold = 0;

	for (i=0;i<average;i++){
		adxl335_get(&sample.x, &sample.y, &sample.z);
		int32_t x = bit20_to_int32(sample.x);
		int32_t y = bit20_to_int32(sample.y);
		int32_t z = bit20_to_int32(sample.z);
		EarthquakeThreshold += ((float) x*x + y*y + z*z)/average;
		xtimer_usleep(10000);
	}
	EarthquakeThreshold = EarthquakeThreshold*EARTHQUAKE_THRESHOLD;
}

void estructural_init(void){
	FlashBuffer = BigBuffer1;
	RealBuffer = BigBuffer2;
	adxl335_init();
	AT45DB041E_init();
	calibrate_earthquake();
}


static inline void estructural_switch_buffers(void)
{
	sample_t * aux = FlashBuffer;
	FlashBuffer = RealBuffer;
	RealBuffer = aux;
}




#if LOWPASSFILTER
/* lowpass */
static uint8_t thereis_earthquake(sample_t * sample){

	int32_t x  =bit20_to_int32(sample->x);
	int32_t y  =bit20_to_int32(sample->y);
	int32_t z  =bit20_to_int32(sample->z);
	const float a = (EARTHQUAKE_TAU_US/SAMPLE_TIME_US);
	const float b = (a + 1);
	
	float mag = (float) x*x + y*y + z*z;
	static float earthquake_probability = 0;
	if ( mag > EarthquakeThreshold){
		earthquake_probability = (1 + earthquake_probability*a)/b;
		//puts("I have movement");
	}else{
		earthquake_probability = (0 + earthquake_probability*a)/b;
	}

	//print_float(earthquake_probability,4);
	return (earthquake_probability > EARTHQUAKE_THRESHOLD_PROBABILITY);
}
#else
/* just a threshold comparator */
static uint8_t thereis_earthquake(sample_t * sample){
	int32_t x  = bit20_to_int32(sample->x);
	int32_t y  = bit20_to_int32(sample->y);
	int32_t z  = bit20_to_int32(sample->z);
	float mag = (float) x*x + y*y + z*z;
	if (mag > EarthquakeThreshold)
		return 1;
	return 0;
}
#endif

void notify_save_to_flash(kernel_pid_t * pid_save, uint16_t sample_counter, sample_t * big_buffer)
{

	//char debug[100];
	//sprintf(debug, "kernel_pid = %ld, sample_counter = %d, buffer_addr = %ld", (uint32_t) pid_save, sample_counter ,(uint32_t) &(big_buffer[0]));
	//puts(debug);


	static msg_t msg;
	static save_sd_t msg_sd;
	msg_sd.sample_counter = sample_counter;
	msg_sd.sample_buffer = big_buffer;
	msg.content.ptr = (void * ) &msg_sd;
	msg_send(&msg, *pid_save); /* blocking */
}

void estructural_save_data(sample_t * sample ,void * pid){

	static uint16_t sample_counter = 0;
	static uint8_t have_earthquake = 0;
	
	/* notify process when to save */
	//(void)pid;
	kernel_pid_t * pid_save = (kernel_pid_t *) pid;
	
	//static uint32_t t = 0;
	//static uint32_t x = 0xff;
	//static uint32_t y = 0xfff;
	//static uint32_t z = 0xffff;
	//sample_t fakesample;
	//fakesample.x = x++;
	//fakesample.y = y++;
	//fakesample.z = z++;
	//fakesample.ntp_time = t++;
	
	RealBuffer[sample_counter++] = *sample;
	//RealBuffer[sample_counter++] = fakesample;

	if (sample_counter >= SAMPLES_PER_SECOND*HISTORY_TIME_S){
		sample_counter = 0;
		if (have_earthquake){ /* there has been an earthquake */
			estructural_switch_buffers();
			notify_save_to_flash(pid_save, sample_counter, FlashBuffer);
			
		}
	}
	/* we have an earthquake for the first time*/
	
	if (thereis_earthquake(sample) && !(have_earthquake)){
	
		puts("Earquake mode on !");
		have_earthquake = 1;
		estructural_switch_buffers();
		notify_save_to_flash(pid_save, sample_counter, FlashBuffer);
		sample_counter = 0;
	}



	//printf("x=%lu,y=%lu,z=%lu,ntp=%lu\r\n", sample.x, sample.y, sample.z, sample.ntp_time);
}

/* returns true if there is enough space for an other sample */
static uint8_t _add_sample_to_write_buff(uint8_t * write_buff, sample_t * sample, uint16_t  * pos){
	write_buff[(*pos)++] = (sample->ntp_time & 0xff000000)>>24;
	write_buff[(*pos)++] = (sample->ntp_time & 0x00ff0000)>>16;
	write_buff[(*pos)++] = (sample->ntp_time & 0x0000ff00)>>8;
	write_buff[(*pos)++] = (sample->ntp_time & 0x000000ff)>>0;

	write_buff[(*pos)++] = (sample->x & 0xff0000)>>16; /* last 0xf is unnecesary, could optimize */
	write_buff[(*pos)++] = (sample->x & 0x00ff00)>>8;
	write_buff[(*pos)++] = (sample->x & 0x0000ff)>>0;

	write_buff[(*pos)++] = (sample->y & 0xff0000)>>16; /* last 0xf is unnecesary, could optimize */
	write_buff[(*pos)++] = (sample->y & 0x00ff00)>>8;
	write_buff[(*pos)++] = (sample->y & 0x0000ff)>>0;

	write_buff[(*pos)++] = (sample->z & 0xff0000)>>16; /* last 0xf is unnecesary, could optimize */
	write_buff[(*pos)++] = (sample->z & 0x00ff00)>>8;
	write_buff[(*pos)++] = (sample->z & 0x0000ff)>>0;

	return (*pos < 251);


}

uint8_t save_to_flash(void * msg){
	(void)msg;
	save_sd_t * save_sd_msg =  (save_sd_t *) ((msg_t *) msg)->content.ptr;
	sample_t * sample_buffer = save_sd_msg->sample_buffer;
	uint16_t initial_sample = save_sd_msg->sample_counter;

	static uint16_t page = 0;
	static uint8_t write_buff[264];
	uint16_t write_buff_pos = 0;
	uint16_t current_sample = initial_sample;
	
	uint16_t i;
	uint8_t space_available = 1;
	puts("Saving Earthquake to flash page !");

	for(i=0;i<SAMPLES_PER_SECOND*HISTORY_TIME_S;i++){
		/* sample_buffer[current_sample++] will not work if sample_buffer is not full ! */
		char debug[100];
		sprintf(debug, "current_sample = %d, buffer_addr = %ld",current_sample ,(uint32_t) &(sample_buffer[0]));
		puts(debug);
		xtimer_usleep(1000);
		space_available = _add_sample_to_write_buff(write_buff, &sample_buffer[current_sample++],&write_buff_pos);
		
		if (!space_available){
			//puts("page_write");
			AT45DB041E_page_write(page++, write_buff,write_buff_pos);
			write_buff_pos = 0;
		}else if(i == SAMPLES_PER_SECOND*HISTORY_TIME_S - 1){ /* last iteration */
			//puts("page_write");
			AT45DB041E_page_write(page++, write_buff,write_buff_pos);
			write_buff_pos = 0;
		}
		if (current_sample >= SAMPLES_PER_SECOND*HISTORY_TIME_S){
			current_sample = 0;
		}
		
	}
	
	return (page >= 2048); /* flashfull */
}


uint8_t have_saved_earthquake(void)
{
	uint16_t page = 0;
	uint8_t buff[264];
	AT45DB041E_page_read(page, (void *) buff, sizeof(buff));
	uint8_t info = 0xff;
	uint16_t i;
	for (i=0;i<sizeof(buff);i++)
		info &= buff[i];	

	return (info != 0xff);

}



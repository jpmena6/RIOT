/*
 * Copyright (C) 2017 Baptiste CLENET
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @file
 * @brief       OpenThread test application
 *
 * @author      Baptiste Clenet <bapclenet@gmail.com>
 */

#include <stdio.h>
#include <math.h>
#include "ot.h"
#include "ot_estructural_com.h"
#include "app_estructural.h"
#include "adxl335.h"
#include "periph/pm.h"
#include "periph/gpio.h"
#include "thread.h" /* threading */
#include "xtimer.h"


/*	All processes except openthread are here in main 
 *	(OpenThread answers to communication messages)
 */


char ping_monitor_stack[THREAD_STACKSIZE_SMALL];
static uint8_t PingReply;
void *ping_monitor(void *arg)
{
	(void) arg;
	while(1){
		thread_yield();
		xtimer_sleep(28);
		puts("Sending ping");
		if (coms_ping_server("fd11::100", &PingReply)){
			puts("Error sending PING to Server");
			com_restart();
			led_red(1);
		}
		/* if not received after 3 seconds restart coms */
		xtimer_sleep(2);
		if (!PingReply){
			puts("No ping received");
			com_thread_restart();
			led_red(1);
		}else{
			led_red(0);
		}

	}
	return NULL;
}

int64_t ApplyDelay = 0;
int64_t TheDeltaTime = 0;
/* bug... */
//static uint8_t sample_now = 0;
char sampler_start_stack[THREAD_STACKSIZE_SMALL];

void *sampler_start(void *arg)
{
	(void) arg;

	
	uint64_t the_time;
	//uint8_t ledon = 0;
	gpio_init(APP_GPIO_PIN, GPIO_OUT);
	gpio_init(APP_LED_GREEN, GPIO_OUT);
	xtimer_ticks32_t last_time = xtimer_now();
	
	while(1){
		int32_t delay = 0;
		the_time = xtimer_now_usec() + TheDeltaTime;
		if (ApplyDelay){
			//ledon = 0;
			delay = abs(the_time%BUG_TIME_US);
			if (delay*2 > BUG_TIME_US){
				delay = -(BUG_TIME_US-delay);
			}
			ApplyDelay = 0;
		}
			
		xtimer_periodic_wakeup(&last_time, BUG_TIME_US - delay);
		//gpio_toggle(APP_GPIO_PIN);
		//if (the_time & (1<<24))
		//	ledon = 0;

		//if ((ledon++ & 0b111) == 0b111)
			gpio_toggle(APP_LED_GREEN);
			//gpio_set(APP_LED_GREEN);
			
		//else
		//	gpio_set(APP_LED_GREEN);
		//gpio_toggle(APP_GPIO_PIN);
		//sample_now++; /* bug.. */
		
	}
	return NULL;
}

static kernel_pid_t earthquake_manage_pid;
char earthquake_manage_stack[THREAD_STACKSIZE_MEDIUM];
//static msg_t rcv_queue[1];
void *earthquake_manage(void * arg)
{
	(void) arg;
	//msg_init_queue(rcv_queue, 1);
	msg_t msg;
	uint8_t flash_full = 0;
	while(1){
		msg_receive(&msg); /* blocks until message received */
		//puts("Received message");
		led_blue(1);
		//char debug[100];
		//save_sd_t * save_sd_msg =  (save_sd_t *) msg.content.ptr;
		//sample_t * sample_buffer = save_sd_msg->sample_buffer;
		//sprintf(debug, "time = %lx, x = %lx , y = %lx, z = %lx", sample_buffer[0].ntp_time,sample_buffer[0].x,sample_buffer[0].y,sample_buffer[0].z);
		//puts(debug);
		flash_full = save_to_flash((void *) &msg);
		if(flash_full){
			puts("rebooting !");
			pm_reboot();
		}
	}
	
	return NULL;
}
int16_t RequestedPage = -1;
int main(void)
{
    puts("Estructural App");
	xtimer_init();
	/* turn off led_red once comunication with server is ok */
	led_red(1);


	estructural_init();
	/* OpenThread manages comunications and priority is -3 in this implementation */
	uint8_t err = com_autoinit();
	if (err){
		printf("Error %d on init\r\n", err);
		pm_reboot();
	}


	/* ping monitor */
	thread_create(	ping_monitor_stack, sizeof(ping_monitor_stack),
                  	THREAD_PRIORITY_MAIN + 5, THREAD_CREATE_STACKTEST,
                   	ping_monitor, NULL, "ping_monitor");	


	if (have_saved_earthquake()){
		led_blue(0);
		led_red(1);
		led_green(0);
		while(!PingReply){
			xtimer_sleep(5);
		}
		led_green(1);
		puts("Begin send all to server !!");
		com_send_all_server(DELAY_UDP_SENDS_US);
		//coms_send_to("fd11::100", PORT_SERVER, "I am a MCU !");
		puts("End sending all to server");
		while(1){
			xtimer_usleep(300000);
			puts("Sending Data ready !");
			com_send_data_ready();
			xtimer_usleep(300000);
			if (RequestedPage >= 0)
				_coms_send_page(RequestedPage);

			led_blue(0);
			led_red(0);
			led_green(1);
		} 
	}
		
	


	/* manages earthquakes */
	earthquake_manage_pid = thread_create(	earthquake_manage_stack, sizeof(earthquake_manage_stack),
                  	THREAD_PRIORITY_MAIN + 4, THREAD_CREATE_STACKTEST,
                   	earthquake_manage, NULL, "earthquake_manage");


	/* green led indicator */
	thread_create(	sampler_start_stack, sizeof(sampler_start_stack),
                  	THREAD_PRIORITY_MAIN - 2, THREAD_CREATE_STACKTEST,
                   	sampler_start, NULL, "sampler_start");


	while(1){
		thread_yield();
		xtimer_usleep(SAMPLE_TIME_US);
		/* get new data and save to buffer */
		sample_t sample;
		sample.ntp_time = xtimer_now_usec() + TheDeltaTime;
		adxl335_get(&sample.x, &sample.y, &sample.z);
		estructural_save_data(&sample ,(void *)&earthquake_manage_pid);
		

		

	
	}
    return 0;
}

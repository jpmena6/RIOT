#ifndef _OT_COMS_H
#define _OT_COMS_H

#include "ot.h"
#include "openthread/udp.h"
#include <string.h>
#include <stdint.h>

#define PORT_SYNC 	1111
#define PORT_SERVER 8888
#define PORT_P2P	7777
#define IP6_SERVER "fd11::100"


#define IP_PREFIX "fd11:1212:0:0:%x:%x:%x:%x"

#define IP6_GENERIC "0011:2233:4455:6677:8899:aabb:ccdd:eeff"
#define IP6_LEN sizeof(IP6_GENERIC)+1

#define PING_SERVER_MSG "alo?"



typedef struct appUdpParams{
	char * mIp;
	uint16_t mPort;
	otUdpReceive mCallBack;
}appUdpParams;

typedef struct appUdpSendParams{
	char * mPeerIp;
	char * mLocalIp;
	uint16_t mPort;
	char * mMsg;
	otUdpSocket	* mUdpSocket;
	uint16_t  mLength;
}appUdpSendParams;




typedef struct appPingParams{
	char * mPeerIp;
	char * mLocalIp;
	char * mMsg;
	void * mCallback;
}appPingParams;

uint8_t com_autoinit(void);

uint8_t com_init(void);

uint8_t com_deinit(void);

uint8_t com_thread_restart(void);

uint8_t com_thread_deinit(void);

uint8_t com_thread_init(void);

uint8_t com_restart(void);

uint8_t coms_send_to(char * aIpv6, uint16_t aPort, char * aMsg);

uint8_t coms_send_to_n(char * aPeerIpv6 ,uint16_t aPort, char * aMsg, uint16_t n);

/* requires to untegister callback in cli.c if using cli example app */
uint8_t coms_ping_server(char * aPeerIpv6, uint8_t * ping_res);

void coms_process_ping(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);

void coms_process_sync(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);

void coms_process_p2p(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);


/**
 *	Commands the server can send to the device:
 *	1) Clear the flash 	(F)
 *  2) Reboot			(R)
 *  3) Resend Page		(SXXXX) 0<=XXXX<=2046
 *
 *  Commands the device can send to the server:
 *	1) Send Page		(SXXXXDDD[data])  0<=XXXX<=2046, D = amount of bytes in [data] [ASCII]
 *  2) Data Ready 		(D) all data has been sent
 */
void coms_process_server(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);

/**
 *	@brief send all pages to server
 *	@param[in]	delay_us: delay between udp packets
 *
 */
void com_send_all_server(uint32_t delay_us);

/**
 *	@brief notify server we are idle
 *
 */
void com_send_data_ready(void);


void _coms_send_page(uint16_t page);

#endif /*_OT_COMS_H*/

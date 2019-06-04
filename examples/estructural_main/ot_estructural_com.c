#include "ot_estructural_com.h"
#include "app_estructural.h"
#include "AT45DB041E.h"
#include "periph/pm.h"
#include <stdio.h>
#include "xtimer.h"


static otUdpSocket	aUdpSocketSync;
static otUdpSocket	aUdpSocketP2p;
static otUdpSocket	aUdpSocketServer;


static char MyIp[IP6_LEN];


uint8_t com_deinit(void)
{
	uint8_t ena = 0;
	uint8_t res = ot_call_command("com_estructural_enable", (void *) &ena, NULL);
	if (res){
		puts("error com_estructural_enable 0\r\n");
		return res;
	}
	return res;

}

uint8_t com_thread_deinit(void)
{
	uint8_t ena = 0;
	uint8_t res = ot_call_command("com_estructural_thread_enable", (void *) &ena, NULL);
	if (res){
		puts("error com_thread_deinit()\r\n");
		return res;
	}
	return res;
}

uint8_t com_thread_init(void)
{
	uint8_t ena = 1;
	uint8_t res = ot_call_command("com_estructural_thread_enable", (void *) &ena, NULL);
	if (res){
		puts("error com_thread_init()\r\n");
		return res;
	}
	return res;
}

uint8_t com_thread_restart(void)
{
	uint8_t res = com_thread_deinit();
	if (res){
		puts("error com_thread_deinit()\r\n");
		return res;
	}

	res = com_thread_init();
	if (res){
		puts("error com_thread_init()\r\n");
		return res;
	}
	return res;
}

uint8_t com_restart(void)
{

	uint8_t res = com_deinit();
	if (res){
		puts("error com_deinit()\r\n");
		return res;
	}

	res = com_init();
	if (res){
		puts("error com_init()\r\n");
		return res;
	}
	return res;
}


static uint8_t com_udp_init(void)
{
	appUdpParams aUdpParamP2p	 = {.mPort = PORT_P2P,\
								 	.mCallBack = coms_process_p2p,\
									.mIp = MyIp};
	
	appUdpParams aUdpParamServer = {.mPort = PORT_SERVER,\
								 	.mCallBack = (otUdpReceive) &coms_process_server,\
									.mIp = MyIp};
	appUdpParams aUdpParamSync	 = {.mPort = PORT_SYNC,\
								 	.mCallBack = (otUdpReceive) &coms_process_sync,\
									.mIp = MyIp};

	

	uint8_t res = ot_call_command("udp_estructural_init", (void *)&aUdpParamP2p, 	(void*)&aUdpSocketP2p);
	if (res)
		return res;
	
	res = ot_call_command("udp_estructural_init", (void *)&aUdpParamServer, (void*)&aUdpSocketServer);
	if (res)
		return res;

	res = ot_call_command("udp_estructural_init", (void *)&aUdpParamSync, 	(void*)&aUdpSocketSync);
	if (res)
		return res;

	return res;
}

uint8_t com_init(void)
{
	uint8_t ena = 1;
	uint8_t res = ot_call_command("com_estructural_enable", (void *) &ena, NULL);
	if (res){
		puts("error com_estructural_enable 1\r\n");
		return res;
	}
	return res;

}

uint8_t com_autoinit(void)
{

	uint8_t res;

	res = com_deinit();
	if (res){
		puts("com_deinit() failed\r\n");
		return res;
	}
    uint16_t panid = 0x1234;

    res = ot_call_command("panid", (void*) &panid, NULL);
	if (res)
		return res;

	/* create global ip using hwaddr */	
	res = ot_call_command("ip6_estructural_create_ip", NULL, (void *)MyIp);
	if (res)
		return res;

	puts(MyIp);
	
	res = ot_call_command("ip6_estructural_ipadd", (void *)MyIp, NULL);
	if (res){
		return res;	
	}
	puts("IP set OK !");
	
	
	com_udp_init();
	
	res =  com_init();
	if (res){
		puts("error com_init()\r\n");
		return res;
	}
	return res;
}

static appPingParams pingParams = {	.mLocalIp = MyIp,\
									.mMsg = PING_SERVER_MSG,\
									.mCallback = &coms_process_ping};

static uint8_t * PingRes;
uint8_t coms_ping_server(char * aPeerIpv6, uint8_t * ping_res)
{
	//puts("Sending ping..");
	pingParams.mPeerIp = aPeerIpv6;
	*ping_res = 0;
	PingRes = ping_res;
	uint8_t res = ot_call_command("com_estructural_ping", (void *) &pingParams, NULL);

	if (res)
		return res;
	return res;
}

void coms_process_ping(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
	(void)aContext;
	(void)aMessage;
	(void)aMessageInfo;

	puts("Received Ping !");
	*PingRes = 1;

}


void coms_process_sync(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
	(void)aContext;
	//static uint32_t RTT;
	static uint32_t ntp;

	otIp6Address peerAddr = aMessageInfo->mPeerAddr;
	(void) peerAddr;

	char buff[150];
	char * numbuff;
	uint8_t length      = otMessageRead(aMessage, otMessageGetOffset(aMessage), buff, sizeof(buff) - 1);
	buff[length] = '\0';
	numbuff = &buff[1]; 

	//puts("Received UDP to P2P:");
	//puts(buff);
	//char debug_buff[50];
	/* Received RTT or NTP */
	if (buff[0] == 'R'){

	}else if (buff[0] == 'N'){
		ntp = strtoul(numbuff,0,10);
		//sprintf(debug_buff,"buff=%s, ntp=%ld", numbuff, ntp);
		//puts(debug_buff);
		/* update timer */
		//estructural_set_counter(NTP + (RTT>>1));/* NTP + RTT/2 */
		estructural_set_counter(ntp);
	}

}


void coms_process_p2p(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
	(void)aContext;
	(void)aMessage;
	(void)aMessageInfo;

	otIp6Address peerAddr = aMessageInfo->mPeerAddr;
	(void) peerAddr;

	char buf[1500];
	uint16_t length      = otMessageRead(aMessage, otMessageGetOffset(aMessage), buf, sizeof(buf) - 1);
	buf[length] = '\0';

	//gpio_toggle(PIN2);
	puts("Received UDP to P2P:");
	puts(buf);
}


void com_send_data_ready(void)
{
	coms_send_to(IP6_SERVER, PORT_SERVER,"D0000");
}

void _coms_send_page(uint16_t page)
{
	char page_str[] = "XXXX";
	sprintf(page_str,"%04d",page);
	const char data[] = "264";
	uint8_t buff[264+8+1] = {'S',page_str[0], page_str[1], page_str[2], page_str[3], data[0], data[1], data[2]};
	AT45DB041E_page_read(page, (void *) &buff[8], 264);
	coms_send_to_n(IP6_SERVER, PORT_SERVER,(char *) buff,264+8);

}

void coms_process_server(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
	(void)aContext;
	(void)aMessage;
	(void)aMessageInfo;

	otIp6Address peerAddr = aMessageInfo->mPeerAddr;
	(void) peerAddr;

	char buff[150];
	char * numbuff;
	uint8_t length      = otMessageRead(aMessage, otMessageGetOffset(aMessage), buff, sizeof(buff) - 1);
	buff[length] = '\0';
	numbuff = &buff[1]; 


	if (buff[0] == 'F'){ 		/* Delete the Flash */
	}else if (buff[0] == 'R'){ 	/* Reboot device and delete the flash */
		/* is has been seen that erase might not arrive, so we will erase the flash here */
		puts("Erasing Flash !!");
		AT45DB041E_chip_erase();
		puts("Rebooting !");
		pm_reboot();
	}else if(buff[0] == 'S'){	/* send a page */
		printf("Server requested page: %d\r\n",atoi(numbuff));
		RequestedPage = atoi(numbuff);
		//_coms_send_page((uint16_t) atoi(numbuff));

	}
}



void com_send_all_server(uint32_t delay_us)
{
	uint16_t p;
	for (p = 0; p<2048; p++){
		printf("page %04d\r\n", p);
		_coms_send_page(p);
		xtimer_usleep(delay_us);
	}

}

uint8_t coms_send_to_n(char * aPeerIpv6 ,uint16_t aPort, char * aMsg, uint16_t n){
	uint8_t res;
	appUdpSendParams aUdpSendParams = {	.mPeerIp = aPeerIpv6,\
										.mLocalIp = MyIp,\
										.mPort = aPort,\
										.mLength = n,\
										.mMsg = aMsg};

	switch (aPort)
	{
		case PORT_P2P:
			aUdpSendParams.mUdpSocket = &aUdpSocketP2p;
			break;
		case PORT_SERVER:
			aUdpSendParams.mUdpSocket = &aUdpSocketServer;
			break;
		case PORT_SYNC:
			aUdpSendParams.mUdpSocket = &aUdpSocketSync;
			break;
		default:
			aUdpSendParams.mUdpSocket = &aUdpSocketP2p;
	}	

	res = ot_call_command("com_udp_estructural_send_n", (void *) &aUdpSendParams, NULL);
	if (res)
		return res;
	return res;
}

uint8_t coms_send_to(char * aPeerIpv6 ,uint16_t aPort, char * aMsg)
{
	uint8_t res;
	appUdpSendParams aUdpSendParams = {	.mPeerIp = aPeerIpv6,\
										.mLocalIp = MyIp,\
										.mPort = aPort,\
										.mLength = 0xffff,\
										.mMsg = aMsg};

	switch (aPort)
	{
		case PORT_P2P:
			aUdpSendParams.mUdpSocket = &aUdpSocketP2p;
			break;
		case PORT_SERVER:
			aUdpSendParams.mUdpSocket = &aUdpSocketServer;
			break;
		case PORT_SYNC:
			aUdpSendParams.mUdpSocket = &aUdpSocketSync;
			break;
		default:
			aUdpSendParams.mUdpSocket = &aUdpSocketP2p;
	}	

	res = ot_call_command("com_udp_estructural_send", (void *) &aUdpSendParams, NULL);
	if (res)
		return res;
	return res;

}

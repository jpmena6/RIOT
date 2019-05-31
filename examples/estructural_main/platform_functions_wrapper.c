/*
 * Copyright (C)
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 * @ingroup     net
 * @file
 * @brief       Implementation of OpenThread functions wrapper. They are used to call OT functions from OT thread
 *
 * @author      Jose Ignacio Alamos <jialamos@inria.cl>
 * @author      Baptiste CLENET <bapclenet@gmail.com>
 * @}
 */


#ifdef _APP_ESTRUCTURAL_
#include "../../../examples/estructural_main/ot_estructural_com.h"
#endif 

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "thread.h"
#include "openthread/ip6.h"
#include "openthread/icmp6.h"
#include "openthread/thread.h"
#include "openthread/thread_ftd.h"
#include "openthread/joiner.h"
#include "openthread/udp.h"
#include "openthread/openthread.h"
#include "ot.h"


#define ENABLE_DEBUG (0)
#include "debug.h"

typedef uint8_t OT_COMMAND;

OT_COMMAND ot_channel(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_eui64(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_extaddr(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_ipaddr(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_masterkey(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_networkname(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_mode(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_panid(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_parent(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_state(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_thread(otInstance* ot_instance, void* arg, void* answer);

#ifdef _APP_ESTRUCTURAL_
/* Added by Tomas */
OT_COMMAND ot_udp_estructural_init(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_ip6_estructural_ipadd(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_com_estructural_enable(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_ip6_estructural_create_ip(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_udp_estructural_send(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_udp_estructural_send_n(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_estructural_ping(otInstance* ot_instance, void* arg, void* answer);
OT_COMMAND ot_estructural_thread_enable(otInstance* ot_instance, void* arg, void* answer);
#endif

/**
 * @brief   Struct containing an OpenThread job command
 */
typedef struct {
    const char *name;                                   /**< A pointer to the job name string. */
    OT_COMMAND (*function)(otInstance*, void*, void*);  /**< function to be called */
} ot_command_t;

const ot_command_t otCommands[] =
{
    /* channel: arg NULL: get channel in answer | arg not NULL: set channel */
    { "channel", &ot_channel },
    /* eui64 : arg NULL: get eui64 in answer | arg not NULL: set eui64 */
    { "eui64", &ot_eui64 },
    /* extaddr: arg NULL: get extaddr in answer | arg not NULL: set extaddr */
    { "extaddr", &ot_extaddr },
    /* ipaddr: arg NULL: get nb ipaddr in answer | arg not NULL: get ipaddr[arg] */
    { "ipaddr", &ot_ipaddr },
    /* masterkey: arg NULL: get masterkey in answer | arg not NULL: set masterkey */
    { "masterkey", &ot_masterkey },
    /* mode: arg NULL: get mode in answer | arg not NULL: set mode */
    { "mode", ot_mode },
    /* networkname: arg NULL: get networkname in answer | arg not NULL: set networkname */
    { "networkname", &ot_networkname },
    /* panid: arg NULL: get panid in answer | arg not NULL: set panid */
    { "panid", &ot_panid },
    /* parent: arg NULL: get parent in answer */
    { "parent", &ot_parent },
    /* state: arg NULL: get state in answer */
    { "state", &ot_state },
    /* thread: arg "start"/"stop": start/stop thread operation */
    { "thread", &ot_thread },
	#ifdef _APP_ESTRUCTURAL_
    /* bind sockets and rx callbacs */
    { "udp_estructural_init", &ot_udp_estructural_init },
    /* add a global IPv6 to iface */
	{ "ip6_estructural_ipadd",&ot_ip6_estructural_ipadd},
	/* enable or disable coms */
	{ "com_estructural_enable",&ot_com_estructural_enable},
	/* enable or disable coms */
	{ "ip6_estructural_create_ip",&ot_ip6_estructural_create_ip},
	/* send a string via UDP */
	{ "com_udp_estructural_send",&ot_udp_estructural_send},
	/* send a message of len n via UDP */
	{ "com_udp_estructural_send_n",&ot_udp_estructural_send_n},
	/* ping */
	{ "com_estructural_ping",&ot_estructural_ping},
	/* enable or disable thread */
	{"com_estructural_thread_enable", &ot_estructural_thread_enable},
	#endif
};

#ifdef _APP_ESTRUCTURAL_

/**
 *	@brief add ipv6 address to iface
 *
 */

OT_COMMAND ot_ip6_estructural_ipadd(otInstance* ot_instance, void* arg, void* answer)
{
	/*otIp6SubscribeMulticastAddress*/

	
	(void)answer;
	otNetifAddress aAddress;

	uint8_t err;

	err = otIp6AddressFromString((char *)arg, &aAddress.mAddress);
	if (err){
		return err;
	}
    aAddress.mPrefixLength = 64;
    aAddress.mPreferred    = true;
    aAddress.mValid        = true;
	
	return otIp6AddUnicastAddress(ot_instance, &aAddress);
}


/**
 *	@brief open sockets and bind receive callbacks
 *
 *
 */

OT_COMMAND ot_udp_estructural_init(otInstance* ot_instance, void* arg, void* answer)
{

	(void) answer;
	(void) arg;

	appUdpParams * aUdpParam = (appUdpParams *) arg;

	puts("Initializing Udp server !");
	puts(aUdpParam->mIp);

	otSockAddr sockaddr;
	memset(&sockaddr,0,sizeof(otSockAddr));
	//otIp6AddressFromString(aUdpParam->mIp, &sockaddr.mAddress);
	otIp6AddressFromString("::", &sockaddr.mAddress);
	sockaddr.mPort = aUdpParam->mPort;
	sockaddr.mScopeId = OT_NETIF_INTERFACE_ID_THREAD;

	otUdpSocket	* aUdpSocket = (otUdpSocket *) answer;


	uint8_t res = otUdpOpen(ot_instance, aUdpSocket, aUdpParam->mCallBack, NULL);
	if (res)
		return res;

	puts("UdpOpen Ok !");


	res = otUdpBind(aUdpSocket, &sockaddr);
	if (res)
		return res;
	
	puts("UdpBind Ok !");
	
	return 0;
}


OT_COMMAND ot_udp_estructural_send_n(otInstance* ot_instance, void* arg, void* answer){
	(void) answer;

	uint8_t err;

	appUdpSendParams * aUdpSendParams = (appUdpSendParams *) arg;

	char * aPeerIpv6 			= aUdpSendParams->mPeerIp;
	char * aLocalIpv6 			= aUdpSendParams->mLocalIp;
	//char aLocalIpv6[] = "fd11:1313::100";
	uint16_t aPort 				= aUdpSendParams->mPort;
	char * aMsg 				= aUdpSendParams->mMsg;
	uint16_t aLen				= aUdpSendParams->mLength;
	otUdpSocket	* aUdpSocket	= aUdpSendParams->mUdpSocket;

	//puts("UdpOpen..");
	//err = otUdpOpen(ot_instance, aUdpSocket, NULL, NULL);
	//if (err)
	//	return err;

	otSockAddr sockaddr;
	memset(&sockaddr,0,sizeof(otSockAddr));
	otIp6AddressFromString(aPeerIpv6,&sockaddr.mAddress);
	sockaddr.mPort = aPort;
	sockaddr.mScopeId = OT_NETIF_INTERFACE_ID_THREAD;
	//puts("UdpConnect..");
	//err = otUdpConnect(aUdpSocket, 	&sockaddr);
	//if (err){
	//	return err;
	//}
	
	/* message info */
	otMessageInfo messageInfo;
	memset(&messageInfo, 0, sizeof(messageInfo));
	otIp6AddressFromString(aPeerIpv6,&messageInfo.mPeerAddr);
	otIp6AddressFromString(aLocalIpv6,&messageInfo.mSockAddr);
	messageInfo.mPeerPort		= aPort;
	messageInfo.mSockPort		= aPort;
	messageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;

	/* message payload */
	otMessage * message = NULL;

	//puts("Allocating memory..");
	message = otUdpNewMessage(ot_instance, true);
	if (!message){
		return 0xaa;
	}

	//puts("Appending message..");
    err = otMessageAppend(message, aMsg, aLen);
	if (err){
		otMessageFree(message);
		return err;	
	}

	//puts("Sending..");
	err = otUdpSend(aUdpSocket, message, &messageInfo);
	if (err){
		if (message != NULL){
			otMessageFree(message);
		}
		otUdpClose(aUdpSocket);
		return err;
	}

	//otUdpClose(aUdpSocket);

	return err;

}

OT_COMMAND ot_udp_estructural_send(otInstance* ot_instance, void* arg, void* answer)
{

	(void) answer;

	uint8_t err;

	appUdpSendParams * aUdpSendParams = (appUdpSendParams *) arg;

	char * aPeerIpv6 			= aUdpSendParams->mPeerIp;
	char * aLocalIpv6 			= aUdpSendParams->mLocalIp;
	//char aLocalIpv6[] = "fd11:1313::100";
	uint16_t aPort 				= aUdpSendParams->mPort;
	char * aMsg 				= aUdpSendParams->mMsg;
	otUdpSocket	* aUdpSocket	= aUdpSendParams->mUdpSocket;

	//puts("UdpOpen..");
	//err = otUdpOpen(ot_instance, aUdpSocket, NULL, NULL);
	//if (err)
	//	return err;

	otSockAddr sockaddr;
	memset(&sockaddr,0,sizeof(otSockAddr));
	otIp6AddressFromString(aPeerIpv6,&sockaddr.mAddress);
	sockaddr.mPort = aPort;
	sockaddr.mScopeId = OT_NETIF_INTERFACE_ID_THREAD;
	//puts("UdpConnect..");
	//err = otUdpConnect(aUdpSocket, 	&sockaddr);
	//if (err){
	//	return err;
	//}
	
	/* message info */
	otMessageInfo messageInfo;
	memset(&messageInfo, 0, sizeof(messageInfo));
	otIp6AddressFromString(aPeerIpv6,&messageInfo.mPeerAddr);
	otIp6AddressFromString(aLocalIpv6,&messageInfo.mSockAddr);
	messageInfo.mPeerPort		= aPort;
	messageInfo.mSockPort		= aPort;
	messageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;

	/* message payload */
	otMessage * message = NULL;

	//puts("Allocating memory..");
	message = otUdpNewMessage(ot_instance, true);
	if (!message){
		return 0xaa;
	}

	//puts("Appending message..");
    err = otMessageAppend(message, aMsg, strlen(aMsg));
	if (err){
		otMessageFree(message);
		return err;	
	}

	//puts("Sending..");
	err = otUdpSend(aUdpSocket, message, &messageInfo);
	if (err){
		if (message != NULL){
			otMessageFree(message);
		}
		otUdpClose(aUdpSocket);
		return err;
	}

	//otUdpClose(aUdpSocket);

	return err;

}


void OTCALL aJoinerCallback(otError aError, void *aContext)
{
	(void) aContext;
    switch (aError)
    {
    case OT_ERROR_NONE:
        printf("Join success\r\n");
        break;

    default:
        printf("Join failed [%s]\r\n", otThreadErrorToString(aError));
        break;
    }
}

OT_COMMAND ot_estructural_thread_enable(otInstance* ot_instance, void* arg, void* answer)
{
	uint8_t enable = *(uint8_t *) arg;
	uint8_t res;
	(void) answer;

	res = otThreadSetEnabled(ot_instance, enable);
	if (res){
		puts("enable Thread error");
		return res;
	}
	return res;

}

OT_COMMAND ot_com_estructural_enable(otInstance* ot_instance, void* arg, void* answer)
{
	uint8_t t_or_f = *(uint8_t *) arg;
	uint8_t res;
	(void) answer;

	otThreadSetRouterRoleEnabled(ot_instance, 0); /* only end device */

	if (t_or_f){
		res = otLinkSetChannel(ot_instance, 26);
		if (res){
			puts("set Channel error");
			return res;
		}
	}

	res = otIp6SetEnabled(ot_instance, t_or_f);
	if (res){
		puts("enable IP error");
		return res;
	}

	res = otThreadSetEnabled(ot_instance, t_or_f);
	if (res){
		puts("enable Thread error");
		return res;
	}



	/*
	const uint8_t *currentPSKc = otThreadGetPSKc(ot_instance);
	char currentPSKd[20];
	sprintf(currentPSKd, "%s",currentPSKc);

	otJoinerStart(ot_instance, currentPSKd, NULL,
                      aVendorName, aVendorModel, aVendorSwVersion, NULL,
                      aJoinerCallback, NULL);
	*/
	return res;

}



OT_COMMAND ot_ip6_estructural_create_ip(otInstance* ot_instance, void* arg, void* answer)
{
	(void) arg;
	/*2e94abc07c075ce4 = 8 bytes long = 64 bits*/
	otExtAddress extaddr;
	extaddr = *((otExtAddress *)otLinkGetExtendedAddress(ot_instance));
	//otLinkGetFactoryAssignedIeeeEui64(ot_instance,&extaddr);

	/*
	sprintf(answer,IP_PREFIX,	((hwaddr & 0xffff000000000000)	>>48),\
								((hwaddr & 0xffff00000000)		>>32),\
								((hwaddr & 0xffff0000)			>>16),\
								((hwaddr & 0xffff)				>>0 ));
	*/
	sprintf(answer,IP_PREFIX,	extaddr.m8[0] + (extaddr.m8[1]<<8),\
								extaddr.m8[2] + (extaddr.m8[3]<<8),\
								extaddr.m8[4] + (extaddr.m8[5]<<8),\
								extaddr.m8[6] + (extaddr.m8[7]<<8));
	return 0;

}

static otIcmp6Handler aHandler;
OT_COMMAND ot_estructural_ping(otInstance* ot_instance, void* arg, void* answer)
{
	(void) answer;
	appPingParams * pingParams			= (appPingParams *) arg;
	char * aPeerIpv6 					= pingParams->mPeerIp;
	char * aLocalIpv6					= pingParams->mLocalIp;	
	char * aMsg							= pingParams->mMsg;
	otIcmp6ReceiveCallback aCallback 	= (otIcmp6ReceiveCallback) pingParams->mCallback;

	uint8_t err = OT_ERROR_NONE;

	static uint8_t registered = 0;
	if (!registered){
		registered = 1;
		puts("Regist. ping Handler");
		aHandler.mReceiveCallback = aCallback;
		//(void) aHandler;
		err = otIcmp6RegisterHandler(ot_instance, &aHandler);
		if (err != OT_ERROR_NONE){
			return err;
		}
	}

	/* message info */
	otMessageInfo messageInfo;
	memset(&messageInfo, 0, sizeof(messageInfo));
	otIp6AddressFromString(aPeerIpv6,&messageInfo.mPeerAddr);
	otIp6AddressFromString(aLocalIpv6,&messageInfo.mSockAddr);
	messageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;


	/* enable ping auto handle */
	otIcmp6SetEchoEnabled(ot_instance,1);

	/* message payload */
	otMessage * message = NULL;

	//puts("Allocating memory..");
	message = otUdpNewMessage(ot_instance, true);
	if (!message){
		return 0xaa;
	}

	//puts("Appending message..");
    err = otMessageAppend(message, aMsg, strlen(aMsg));
	if (err != OT_ERROR_NONE){
		if (message != NULL)
			otMessageFree(message);
		return err;	
	}
	//puts("Sending ping..");
	err = otIcmp6SendEchoRequest(ot_instance, message, &messageInfo, 1);

	if (err != OT_ERROR_NONE && message != NULL)
    {
        otMessageFree(message);
    }

	
	return err;
	
}

#endif /* _APP_ESTRUCTURAL_ */


uint8_t ot_exec_command(otInstance *ot_instance, const char* command, void *arg, void* answer) {
    uint8_t res = 0xFF;
    /* Check running thread */
    if (openthread_get_pid() == thread_getpid()) {
        for (uint8_t i = 0; i < sizeof(otCommands) / sizeof(otCommands[0]); i++) {
            if (strcmp(command, otCommands[i].name) == 0) {
                res = (*otCommands[i].function)(ot_instance, arg, answer);
                break;
            }
        }
        if (res == 0xFF) {
            DEBUG("Wrong ot_COMMAND name\n");
            res = 1;
        }
    } else {
        DEBUG("ERROR: ot_exec_job needs to run in OpenThread thread\n");
    }
    return res;
}

void output_bytes(const char* name, const uint8_t *aBytes, uint8_t aLength)
{
#if ENABLE_DEBUG
    DEBUG("%s: ", name);
    for (int i = 0; i < aLength; i++) {
        DEBUG("%02x", aBytes[i]);
    }
    DEBUG("\n");
#else
    (void)name;
    (void)aBytes;
    (void)aLength;
#endif
}

OT_COMMAND ot_channel(otInstance* ot_instance, void* arg, void* answer) {
    if (answer != NULL) {
        *((uint8_t *) answer) = otLinkGetChannel(ot_instance);
        DEBUG("Channel: %04x\n", *((uint8_t *) answer));
    } else if (arg != NULL) {
        uint8_t channel = *((uint8_t *) arg);
        otLinkSetChannel(ot_instance, channel);
    } else {
        DEBUG("ERROR: wrong argument\n");
    }
    return 0;
}

OT_COMMAND ot_eui64(otInstance* ot_instance, void* arg, void* answer) {
    (void)arg;

    if (answer != NULL) {
        otExtAddress address;
        otLinkGetFactoryAssignedIeeeEui64(ot_instance, &address);
        output_bytes("eui64", address.m8, OT_EXT_ADDRESS_SIZE);
        *((otExtAddress *) answer) = address;
    } else {
        DEBUG("ERROR: wrong argument\n");
    }
    return 0;
}


OT_COMMAND ot_extaddr(otInstance* ot_instance, void* arg, void* answer) {
    (void)arg;

    if (answer != NULL) {
        answer = (void*)otLinkGetExtendedAddress(ot_instance);
        output_bytes("extaddr", (const uint8_t *)answer, OT_EXT_ADDRESS_SIZE);
    } else {
        DEBUG("ERROR: wrong argument\n");
    }
    return 0;
}

OT_COMMAND ot_ipaddr(otInstance* ot_instance, void* arg, void* answer) {
    uint8_t cnt = 0;
    for (const otNetifAddress *addr = otIp6GetUnicastAddresses(ot_instance); addr; addr = addr->mNext) {
        if (arg != NULL && answer != NULL && cnt == *((uint8_t *) arg)) {
            *((otNetifAddress *) answer) = *addr;
            return 0;
        }
        cnt++;
    }
    if (answer != NULL) {
        *((uint8_t *) answer) = cnt;
    } else {
        DEBUG("ERROR: wrong argument\n");
    }
    return 0;
}

OT_COMMAND ot_masterkey(otInstance* ot_instance, void* arg, void* answer) {
    if (answer != NULL) {
        const otMasterKey* masterkey = otThreadGetMasterKey(ot_instance);
        *((otMasterKey *) answer) = *masterkey;
        output_bytes("masterkey", (const uint8_t *)answer, OT_MASTER_KEY_SIZE);
    } else if (arg != NULL) {
        otThreadSetMasterKey(ot_instance, (otMasterKey*)arg);
    } else {
        DEBUG("ERROR: wrong argument\n");
    }
    return 0;
}

OT_COMMAND ot_mode(otInstance* ot_instance, void* arg, void* answer) {
    (void)answer;

    if (arg != NULL) {
        otLinkModeConfig link_mode;
        memset(&link_mode, 0, sizeof(otLinkModeConfig));
        char mode[6];
        memcpy(mode, (char*)arg, 5);
        mode[5] = '\0';
        for (char *arg = &mode[0]; *arg != '\0'; arg++) {
            switch (*arg) {
                case 'r':
                    link_mode.mRxOnWhenIdle = 1;
                    break;
                case 's':
                    link_mode.mSecureDataRequests = 1;
                    break;
                case 'd':
                    link_mode.mDeviceType = 1;
                    break;
                case 'n':
                    link_mode.mNetworkData = 1;
                    break;
            }
        }
        otThreadSetLinkMode(ot_instance, link_mode);
        DEBUG("OT mode changed to %s\n", (char*)arg);
    } else {
        DEBUG("ERROR: wrong argument\n");
    }
    return 0;
}

OT_COMMAND ot_networkname(otInstance* ot_instance, void* arg, void* answer) {
    if (answer != NULL) {
        const char* networkName = otThreadGetNetworkName(ot_instance);
        strcpy((char*) answer, networkName);
        DEBUG("networkname: %.*s\n", OT_NETWORK_NAME_MAX_SIZE, networkName);
    } else if (arg != NULL) {
        otThreadSetNetworkName(ot_instance, (char*) arg);
    } else {
        DEBUG("ERROR: wrong argument\n");
    }
    return 0;
}


OT_COMMAND ot_panid(otInstance* ot_instance, void* arg, void* answer) {
    if (answer != NULL) {
        *((uint16_t *) answer) = otLinkGetPanId(ot_instance);
        DEBUG("PanID: %04x\n", *((uint16_t *) answer));
    } else if (arg != NULL) {
        /* Thread operation needs to be stopped before setting panid */
        otThreadSetEnabled(ot_instance, false);
        uint16_t panid = *((uint16_t *) arg);
        otLinkSetPanId(ot_instance, panid);
        otThreadSetEnabled(ot_instance, true);
    } else {
        DEBUG("ERROR: wrong argument\n");
    }
    return 0;
}

OT_COMMAND ot_parent(otInstance* ot_instance, void* arg, void* answer) {
    (void)arg;

    if (answer != NULL) {
        otRouterInfo parentInfo;
        otThreadGetParentInfo(ot_instance, &parentInfo);
        output_bytes("parent", (const uint8_t *)parentInfo.mExtAddress.m8, sizeof(parentInfo.mExtAddress));
        DEBUG("Rloc: %x\n", parentInfo.mRloc16);
        *((otRouterInfo *) answer) = parentInfo;
    } else {
        DEBUG("ERROR: wrong argument\n");
    }
    return 0;
}

OT_COMMAND ot_state(otInstance* ot_instance, void* arg, void* answer) {
    (void)arg;

    if (answer != NULL) {
        otDeviceRole state = otThreadGetDeviceRole(ot_instance);
        *((otDeviceRole *) answer) = state;
        DEBUG("state: ");
        switch (state) {
            case OT_DEVICE_ROLE_DISABLED:
                puts("disabled");
                break;
            case OT_DEVICE_ROLE_DETACHED:
                puts("detached");
                break;
            case OT_DEVICE_ROLE_CHILD:
                puts("child");
                break;
            case OT_DEVICE_ROLE_ROUTER:
                puts("router");
                break;
            case OT_DEVICE_ROLE_LEADER:
                puts("leader");
                break;
            default:
                puts("invalid state");
                break;
        }
    } else {
        DEBUG("ERROR: wrong argument\n");
    }
    return 0;
}

OT_COMMAND ot_thread(otInstance* ot_instance, void* arg, void* answer) {
    (void)answer;

    if (arg != NULL) {
        if (strcmp((char*)arg, "start") == 0) {
            otThreadSetEnabled(ot_instance, true);
            DEBUG("Thread start\n");
        } else if (strcmp((char*)arg, "stop") == 0) {
            otThreadSetEnabled(ot_instance, false);
            DEBUG("Thread stop\n");
        } else {
            DEBUG("ERROR: thread available args: start/stop\n");
        }
    } else {
        DEBUG("ERROR: wrong argument\n");
    }
    return 0;
}

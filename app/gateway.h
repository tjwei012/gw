/*
 * gateway.h
 *
 *  Created on: May 22, 2014
 *      Author: walker
 */

#ifndef GATEWAY_H_
#define GATEWAY_H_
#include <arpa/inet.h>
#include "aca.h"

#define CHILD_ITEM_NONE 0
#define CHILD_ITEM_OFFLINE 1
#define CHILD_ITEM_ONLINE 2

#define CHILD_TYPE_LIGHT 2

typedef struct st_child{
	uint8_t index;
	struct in6_addr addr;
    char  device_code[ARRAYENT_DEVICE_NAME_LENGTH];
	uint8_t type;
	uint8_t itemStatus;
	struct st_child* pNext;
}ST_CHILD;

struct gateway{
	arrayent_config_t config;
	ST_CHILD* firstChild;
	uint16_t maxChildNum;
};

int initGateway(void);

int AxLogin_JIP(struct in6_addr* addr, uint8_t type);
int AxLogout_JIP(struct in6_addr* addr);
int AxSetProperty(struct in6_addr* addr, char* property);
int AxDelete_JIP(struct in6_addr* addr);
int AxMsg_JIP(struct in6_addr* addr,const char* cmd,const char* value);

#endif /* GATEWAY_H_ */

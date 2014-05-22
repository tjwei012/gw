/*
 * gateway.c
 *
 *  Created on: May 22, 2014
 *      Author: walker
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "gateway.h"
#include "aca.h"
#include "assert.h"

#define ARR_GATEWAY_NAME         "NXP000000"
#define ARR_GATEWAY_PASSWORD     "5D"

#define ARR_DEVICE_PASSWORD     "XX"




/* Leave the Device Key as is unless instructed otherwise by Arrayent. */
#define ARR_GATEWAY_AES_KEY      "00345C4F6592FF6AF13DCC3841EBD52C"


/* The Product ID is one of Arrayent's mechanisms for identifying device traffic.
 * Leave it as is unless instructed otherwise by Arrayent. */
#define ARR_PRODUCT_ID      1101

/* Leave the Product Key as is unless instructed otherwise by Arrayent. */
#define ARR_PRODUCT_AES_KEY      "7EFF77215F8BFB963AD45D571D2BF8BC"

/* The following are hostnames for Arrayent Cloud load balancers.
 * Leave them as is unless instructed otherwise by Arrayent. */
#define ARR_LB_HOSTNAME_1 "axalent-dlb1.arrayent.com"
#define ARR_LB_HOSTNAME_2 "axalent-dlb2.arrayent.com"
#define ARR_LB_HOSTNAME_3 "axalent-dlb3.arrayent.com"

#define ARR_LB_UDP_PORT 80
#define ARR_LB_TCP_PORT 80

#define MAX_CHILD_NUM 16

struct gateway g_gateway;

static ST_CHILD* findChildByAddr(struct in6_addr* addr, const ST_CHILD* firstChild);
static ST_CHILD* findChildByIndex(uint8_t index, const ST_CHILD* firstChild);
static uint8_t allocIndex(const ST_CHILD* firstChild, const uint16_t maxChild);
static void deleteChild(ST_CHILD** firstChild, ST_CHILD* child);
static void insertChild(const ST_CHILD** firstChild, ST_CHILD* child);

// Check to see if Arrayent has a connection to the cloud */
static int8_t checkArrayentConnection(void) {
    arrayent_net_status_t status;

    if(ArrayentNetStatus(&status) != ARRAYENT_SUCCESS) {
        printf("Failed to read network status\r\n");
        return -1;
    }

    if(status.connected_to_server == 0) {
        return -1;
    }

    return 0;
}

void* readProperty(void* arg){
	char buffer[256];
	int len;
	int ret;
	int userIndex;
	char *cmd = NULL;
	char *value = NULL;
	char *split = " ";
	ST_CHILD* child;

	while(1){

		   len = sizeof(buffer);

	       if((ret = ArrayentRecvProperty(&userIndex, buffer, &len, UINT32_MAX))
	               != ARRAYENT_SUCCESS) {
	           printf("Failed to read message from Arrayent with error %d\r\n", ret);
	           continue;
	       }

	       if(len){
	    	   printf("Received property \"%s\" from device %d.\n\r", buffer, userIndex);

	    	   if(userIndex == 0){

	    		  cmd = strtok(buffer, split);

	    		  	//gateway attribute.
	    		  	if (0 == strcmp("permitjoin", cmd)) {
	    		  		value = strtok(NULL, split);

	    		  		AxMsg_JIP(NULL, cmd, value);
	    		  	}

	    		  	if (0 == strcmp("deletedevice", cmd)) {
	    		  		value = strtok(NULL, split);
	    		  		int childIndex = atoi(value);
	    		  		child = findChildByIndex(childIndex, g_gateway.firstChild);
	    		  		if(child){
	    		  			AxMsg_JIP(&(child->addr), cmd, value);
	    		  		}
	    		  	}

	    	   }else{
	    		   cmd = strtok(buffer, split);
    		  	   value = strtok(NULL, split);
    		  	   child = findChildByIndex(userIndex, g_gateway.firstChild);

    		  	   if(child){
    		  		   AxMsg_JIP(&(child->addr), cmd, value);
    		  	   }
	    	   }

	       }

	       usleep(5);

	   }
}

int initGateway(){

	g_gateway.config.product_id = ARR_PRODUCT_ID;
	g_gateway.config.product_aes_key = ARR_PRODUCT_AES_KEY;
	g_gateway.config.load_balancer_domain_names[0] = ARR_LB_HOSTNAME_1;
	g_gateway.config.load_balancer_domain_names[1] = ARR_LB_HOSTNAME_2;
	g_gateway.config.load_balancer_domain_names[2] = ARR_LB_HOSTNAME_3;
	g_gateway.config.load_balancer_udp_port = ARR_LB_UDP_PORT;
	g_gateway.config.load_balancer_tcp_port = ARR_LB_TCP_PORT;
	g_gateway.config.device_name = ARR_GATEWAY_NAME;
	g_gateway.config.device_password = ARR_GATEWAY_PASSWORD;
	g_gateway.config.device_aes_key = ARR_GATEWAY_AES_KEY;
	g_gateway.maxChildNum = MAX_CHILD_NUM;


	/*config the ACA*/
	arrayent_return_e r = ArrayentConfigure(&(g_gateway.config));
	if (r != ARRAYENT_SUCCESS) {
		printf("Configuring Arrayent failed!");
		return 0;
	}

	// Start the Arrayent stack
	if (ArrayentInit() != ARRAYENT_SUCCESS) {
		printf(("Failed to start ACA!\r\n"));
	} else {
		printf(("Started ACA\r\n"));
	}

	// Wait for Arrayent to finish connecting to their server
	while (checkArrayentConnection() != 0) {
		printf("Waiting for Arrayent to finish connecting...\r\n");
		usleep(1000000);
	}

	printf("Successfully connect to the server!\r\n");

	pthread_t thread;
	pthread_create(&thread, NULL, readProperty, NULL);

	return 1;
}

/*
 * @return: ARRAYENT_SUCCESS
 */
int AxLogin_JIP(struct in6_addr* addr, uint8_t type){

	ST_CHILD* child = findChildByAddr(addr, g_gateway.firstChild);

	if(!child){
		child = (ST_CHILD*)malloc(sizeof(*child));

		assert(child != NULL);

		child->addr = *addr;
		child->type = type;
		child->index = allocIndex(g_gateway.firstChild, g_gateway.maxChildNum);

		if(!child->index){
			return ARRAYENT_FAILURE;
		}

		sprintf(child->device_code, "%s%02d", g_gateway.config.device_name, child->index);
		insertChild((const ST_CHILD**)&(g_gateway.firstChild), child);
	}

	child->itemStatus = CHILD_ITEM_ONLINE;


	return ArrayentChildLogin(child->index, child->device_code, ARR_DEVICE_PASSWORD);
}


int AxLogout_JIP(struct in6_addr* addr){

	ST_CHILD* child = findChildByAddr(addr, g_gateway.firstChild);

	if(!child){
		return ARRAYENT_FAILURE;
	}

	child->itemStatus = CHILD_ITEM_OFFLINE;

	return ArrayentChildLogout(child->index);
}

int AxDelete_JIP(struct in6_addr* addr){
	ST_CHILD* child = findChildByAddr(addr, g_gateway.firstChild);

	if(!child){
		return ARRAYENT_FAILURE;
	}

	child->itemStatus = CHILD_ITEM_NONE;

	ArrayentChildLogout(child->index);

	deleteChild(&(g_gateway.firstChild),child);

	free((void*)child);

	return ARRAYENT_SUCCESS;
}

static ST_CHILD* findChildByAddr(struct in6_addr* addr, const ST_CHILD* firstChild){
	ST_CHILD* result = (ST_CHILD*)firstChild;

	while(result){
		if(IN6_ARE_ADDR_EQUAL(&(result->addr),addr)){
			break;
		}

		result = (ST_CHILD*)result->pNext;
	}

	return result;
}

static ST_CHILD* findChildByIndex(uint8_t index, const ST_CHILD* firstChild){
	ST_CHILD* result = (ST_CHILD*)firstChild;

	while(result){
		if(index == result->index){
			break;
		}

		result = (ST_CHILD*)result->pNext;
	}

	return result;
}

static uint8_t allocIndex(const ST_CHILD* firstChild, const uint16_t maxChild){
	uint16_t i = 0;
	ST_CHILD *p;
	for(i = 1; i <= maxChild; i++){
		p = findChildByIndex(i,firstChild);
		if(!p){
			break;
		}
	}

	return (i < maxChild)? i:0;
}

static void insertChild(const ST_CHILD** firstChild, ST_CHILD* child){
	ST_CHILD *prev;

	if(*firstChild == NULL){
		*firstChild = child;
		return;
	}

	prev = (ST_CHILD*)firstChild;

	while(prev){

		if(prev->pNext == NULL
				|| ((prev->index < child->index) &&((prev->pNext)->index > child->index))){
			break;

		}

	}

	child->pNext = prev->pNext;
	prev->pNext = child;
}


static void deleteChild(ST_CHILD** firstChild, ST_CHILD* child){

	ST_CHILD *prev;

	if(*firstChild == NULL){
		return;
	}

	if((*firstChild)->index == child->index){
		*firstChild = NULL;
		return;
	}

	prev = (ST_CHILD *)*firstChild;

	while(prev->pNext){
		if(prev->pNext->index == child->index){
			prev->pNext = prev->pNext->pNext;
		}
	}
}



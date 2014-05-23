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
#include <libxml/parser.h>
#include <libxml/tree.h>
#include "gateway.h"
#include "aca.h"
#include "assert.h"

//#define ARR_GATEWAY_NAME         "NXP000000"
//#define ARR_GATEWAY_PASSWORD     "5D"

#define ARR_DEVICE_PASSWORD     "XX"

#define WORK_DIR ""
#define CONFIG_FILE "gw_nxp.xml"

///* Leave the Device Key as is unless instructed otherwise by Arrayent. */
//#define ARR_GATEWAY_AES_KEY      "00345C4F6592FF6AF13DCC3841EBD52C"
//
///* The Product ID is one of Arrayent's mechanisms for identifying device traffic.
// * Leave it as is unless instructed otherwise by Arrayent. */
//#define ARR_PRODUCT_ID      1101
//
///* Leave the Product Key as is unless instructed otherwise by Arrayent. */
//#define ARR_PRODUCT_AES_KEY      "7EFF77215F8BFB963AD45D571D2BF8BC"
//
///* The following are hostnames for Arrayent Cloud load balancers.
// * Leave them as is unless instructed otherwise by Arrayent. */
//#define ARR_LB_HOSTNAME_1 "axalent-dlb1.arrayent.com"
//#define ARR_LB_HOSTNAME_2 "axalent-dlb2.arrayent.com"
//#define ARR_LB_HOSTNAME_3 "axalent-dlb3.arrayent.com"
//
//#define ARR_LB_UDP_PORT 80
//#define ARR_LB_TCP_PORT 80
//
//#define MAX_CHILD_NUM 16

struct gateway g_gateway;

static ST_CHILD* findChildByAddr(struct in6_addr* addr,
		const ST_CHILD* firstChild);
static ST_CHILD* findChildByIndex(uint8_t index, const ST_CHILD* firstChild);
static uint8_t allocIndex(const ST_CHILD* firstChild, const uint16_t maxChild);
static void deleteChild(ST_CHILD** firstChild, ST_CHILD* child);
static void insertChild(const ST_CHILD** firstChild, ST_CHILD* child);
static void refreshChidlInfo(void);
static int saveGateway(void);
static int loadGateway(void);

// Check to see if Arrayent has a connection to the cloud */
static int8_t checkArrayentConnection(void) {
	arrayent_net_status_t status;

	if (ArrayentNetStatus(&status) != ARRAYENT_SUCCESS) {
		printf("Failed to read network status\r\n");
		return -1;
	}

	if (status.connected_to_server == 0) {
		return -1;
	}

	return 0;
}

void* readProperty(void* arg) {
	char buffer[256];
	uint16_t len;
	int ret;
	uint8_t userIndex;
	char *cmd = NULL;
	char *value = NULL;
	char *split = " ";
	ST_CHILD* child;

	while (1) {

		len = sizeof(buffer);

		if ((ret = ArrayentRecvProperty(&userIndex, buffer, &len, UINT32_MAX))
				!= ARRAYENT_SUCCESS) {
			printf("Failed to read message from Arrayent with error %d\r\n",
					ret);
			continue;
		}

		if (len) {
			printf("Received property \"%s\" from device %d.\n\r", buffer,
					userIndex);

			if (userIndex == 0) {

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
					if (child) {
						AxMsg_JIP(&(child->addr), cmd, value);
					}
				}

			} else {
				cmd = strtok(buffer, split);
				value = strtok(NULL, split);
				child = findChildByIndex(userIndex, g_gateway.firstChild);

				if (child) {
					AxMsg_JIP(&(child->addr), cmd, value);
				}
			}

		}

		usleep(5);

	}
}

int initGateway() {

	if(loadGateway()!=ARRAYENT_SUCCESS){
		return ARRAYENT_FAILURE;
	}

	/*config the ACA*/
	arrayent_return_e r = ArrayentConfigure(&(g_gateway.config));
	printf("r=%d\n", r);
	if (r != ARRAYENT_SUCCESS) {
		printf("Configuring Arrayent failed!");
		return ARRAYENT_FAILURE;
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

	sleep(2);

	refreshChidlInfo();

	pthread_t thread;
	pthread_create(&thread, NULL, readProperty, NULL);

	return ARRAYENT_SUCCESS;
}

void fillDeviceCode(ST_CHILD* child){
	sprintf(child->device_code, "%s", g_gateway.config.device_name);
	sprintf(child->device_code + strlen(g_gateway.config.device_name) - 2,
			"%02d", child->index);
}

/*
 * @return: ARRAYENT_SUCCESS
 */
int AxLogin_JIP(struct in6_addr* addr, uint8_t type) {

	ST_CHILD* child = findChildByAddr(addr, g_gateway.firstChild);

	if (!child) {
		child = (ST_CHILD*) malloc(sizeof(*child));

		assert(child != NULL);

		child->addr = *addr;
		child->type = type;
		child->index = allocIndex(g_gateway.firstChild, g_gateway.maxChildNum);

		if (!child->index) {
			return ARRAYENT_FAILURE;
		}

		fillDeviceCode(child);
		insertChild((const ST_CHILD**) &(g_gateway.firstChild), child);
	}

	child->itemStatus = CHILD_ITEM_ONLINE;

	refreshChidlInfo();

	return ArrayentChildLogin(child->index, child->device_code,
	ARR_DEVICE_PASSWORD);
}

int AxLogout_JIP(struct in6_addr* addr) {

	ST_CHILD* child = findChildByAddr(addr, g_gateway.firstChild);

	if (!child) {
		return ARRAYENT_FAILURE;
	}

	child->itemStatus = CHILD_ITEM_OFFLINE;

	return ArrayentChildLogout(child->index);
}

int AxDelete_JIP(struct in6_addr* addr) {
	ST_CHILD* child = findChildByAddr(addr, g_gateway.firstChild);

	if (!child) {
		return ARRAYENT_FAILURE;
	}

	child->itemStatus = CHILD_ITEM_NONE;

	ArrayentChildLogout(child->index);

	deleteChild(&(g_gateway.firstChild), child);

	free((void*) child);

	refreshChidlInfo();

	return ARRAYENT_SUCCESS;
}

static ST_CHILD* findChildByAddr(struct in6_addr* addr,
		const ST_CHILD* firstChild) {
	ST_CHILD* result = (ST_CHILD*) firstChild;

	while (result) {
		if (IN6_ARE_ADDR_EQUAL(&(result->addr), addr)) {
			break;
		}

		result = (ST_CHILD*) result->pNext;
	}

	return result;
}

static ST_CHILD* findChildByIndex(uint8_t index, const ST_CHILD* firstChild) {
	ST_CHILD* result = (ST_CHILD*) firstChild;

	while (result) {
		if (index == result->index) {
			break;
		}

		result = (ST_CHILD*) result->pNext;
	}

	return result;
}

static uint8_t allocIndex(const ST_CHILD* firstChild, const uint16_t maxChild) {
	uint16_t i = 0;
	ST_CHILD *p;
	for (i = 1; i <= maxChild; i++) {
		p = findChildByIndex(i, firstChild);
		if (!p) {
			break;
		}
	}

	return (i < maxChild) ? i : 0;
}

static void insertChild(const ST_CHILD** firstChild, ST_CHILD* child) {
	ST_CHILD *prev;

	if (*firstChild == NULL) {
		*firstChild = child;
		child->pNext = NULL;
		return;
	}

	prev = (ST_CHILD*) *firstChild;

	while (prev) {

		if (prev->pNext == NULL
				|| ((prev->index < child->index)
						&& ((prev->pNext)->index > child->index))) {
			break;

		}

	}

	child->pNext = prev->pNext;
	prev->pNext = child;
}

static void deleteChild(ST_CHILD** firstChild, ST_CHILD* child) {

	ST_CHILD *prev;

	if (*firstChild == NULL) {
		return;
	}

	if ((*firstChild)->index == child->index) {
		*firstChild = NULL;
		return;
	}

	prev = (ST_CHILD *) *firstChild;

	while (prev->pNext) {
		if (prev->pNext->index == child->index) {
			prev->pNext = prev->pNext->pNext;
		}
	}
}

static void refreshChidlInfo() {
	char* buffer;
	ST_CHILD* child = g_gateway.firstChild;

	buffer = malloc(g_gateway.maxChildNum * 2 + 1);

	if (!buffer)
		return;

	memset(buffer, '0', g_gateway.maxChildNum * 2);
	buffer[g_gateway.maxChildNum * 2] = 0;

	while (child) {
		if (child->type == 2) {

			buffer[2 * (child->index - 1)] = '8';
			buffer[2 * (child->index - 1) + 1] = '2';
			printf("child index =%d", child->index);
		}
		child = child->pNext;
	}

	printf("2 buffer %s\n", buffer);
	ArrayentSetProperty("childInfo", buffer);

	saveGateway();
}

static int saveGateway(){
	char tmp[128];

	//define the doc pointer, create root node
	xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
	xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "gateway");
	//set the root node.
	xmlDocSetRootElement(doc, root_node);

	sprintf(tmp, "%d", g_gateway.maxChildNum);
	xmlNewProp(root_node, BAD_CAST "maxChildNum", BAD_CAST tmp);

	xmlNewProp(root_node, BAD_CAST "version", BAD_CAST "1");

	//create a new node, fill the content and add it to root node.
	xmlNodePtr configNode = xmlNewNode(NULL, BAD_CAST "config");
	xmlAddChild(root_node, configNode);
	xmlNewProp(configNode, BAD_CAST "dlb1",
	BAD_CAST g_gateway.config.load_balancer_domain_names[0]);
	xmlNewProp(configNode, BAD_CAST "dlb2",
	BAD_CAST g_gateway.config.load_balancer_domain_names[1]);
	xmlNewProp(configNode, BAD_CAST "dlb3",
	BAD_CAST g_gateway.config.load_balancer_domain_names[2]);

	sprintf(tmp, "%d", g_gateway.config.load_balancer_tcp_port);
	xmlNewProp(configNode, BAD_CAST "tcp_port", BAD_CAST tmp);

	sprintf(tmp, "%d", g_gateway.config.load_balancer_udp_port);
	xmlNewProp(configNode, BAD_CAST "udp_port", BAD_CAST tmp);

	sprintf(tmp, "%d", g_gateway.config.product_id);
	xmlNewProp(configNode, BAD_CAST "productid", BAD_CAST tmp);

	xmlNewProp(configNode, BAD_CAST "product_aes",
	BAD_CAST g_gateway.config.product_aes_key);

	xmlNewProp(configNode, BAD_CAST "code",
	BAD_CAST g_gateway.config.device_name);
	xmlNewProp(configNode, BAD_CAST "code_suffix",
	BAD_CAST g_gateway.config.device_password);
	xmlNewProp(configNode, BAD_CAST "devic_aes",
	BAD_CAST g_gateway.config.device_aes_key);

	sprintf(tmp, "%d", g_gateway.config.device_can_multi_attribute);
	xmlNewProp(configNode, BAD_CAST "multi_attribute", BAD_CAST tmp);

	//创建一个儿子和孙子节点
	xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "children");
	xmlAddChild(root_node, node);

	ST_CHILD* child = g_gateway.firstChild;
	while (child) {

		xmlNodePtr childNode = xmlNewNode(NULL, BAD_CAST "child");
		xmlAddChild(node, childNode);

		sprintf(tmp, "%d", child->index);
		xmlNewProp(configNode, BAD_CAST "index", BAD_CAST tmp);

		inet_ntop(AF_INET6, &(child->addr), tmp, INET6_ADDRSTRLEN);
		xmlNewProp(configNode, BAD_CAST "addr", BAD_CAST tmp);

		sprintf(tmp, "%d", child->type);
		xmlNewProp(configNode, BAD_CAST "type", BAD_CAST tmp);

	}

	//save xml file
	int nRel = xmlSaveFile(CONFIG_FILE, doc);
	if (nRel != -1) {
		//release the doc.
		xmlFreeDoc(doc);
		return ARRAYENT_SUCCESS;
	}

	return ARRAYENT_FAILURE;
}

static int loadGateway() {
	xmlDocPtr pdoc = NULL;
	xmlNodePtr proot = NULL, curNode = NULL, childNode = NULL;
	xmlChar *szAttr;
	ST_CHILD* child;
	char *pTmp;

	// open xml Doc
	//xmlKeepBlanksDefault(0);
	pdoc = xmlReadFile(CONFIG_FILE, "UTF-8", XML_PARSE_RECOVER);

	if (pdoc == NULL) {
		printf("open file %s error!\n", CONFIG_FILE);
		return ARRAYENT_FAILURE;
	}

	// get xml root elem.
	proot = xmlDocGetRootElement(pdoc);

	if (proot == NULL) {
		printf("error: %s format error）！\n", CONFIG_FILE);
		return ARRAYENT_FAILURE;
	}

	/* find the root : gateway*/
	if (xmlStrcmp(proot->name, BAD_CAST "gateway") != 0) {
		printf("error: %s format error）！\n", CONFIG_FILE);
		return ARRAYENT_FAILURE;
	}

	/* read version info */
	if (xmlHasProp(proot, BAD_CAST "version")) {

	}

	curNode = proot->xmlChildrenNode;

	printf("curNode name:%s\n", curNode->name);
	if (0 == strcmp((char*)curNode->name, "config")) {

		if (xmlHasProp(curNode, BAD_CAST "dlb1")) {
			szAttr = xmlGetProp(curNode, BAD_CAST "dlb1");
			pTmp = (char*) malloc(1 + strlen((char*) szAttr));

			assert(pTmp);

			strcpy(pTmp, (const char*) szAttr);
			g_gateway.config.load_balancer_domain_names[0] = pTmp;
			printf("dlb1=%s\n", g_gateway.config.load_balancer_domain_names[0]);
		}

		if (xmlHasProp(curNode, BAD_CAST "dlb2")) {
			szAttr = xmlGetProp(curNode, BAD_CAST "dlb2");
			pTmp = (char*) malloc(1 + strlen((char*) szAttr));

			assert(pTmp);

			strcpy(pTmp, (const char*) szAttr);
			g_gateway.config.load_balancer_domain_names[1] = pTmp;

			printf("dlb1=%s\n", g_gateway.config.load_balancer_domain_names[1]);
		}

		if (xmlHasProp(curNode, BAD_CAST "dlb3")) {
			szAttr = xmlGetProp(curNode, BAD_CAST "dlb3");
			pTmp = (char*) malloc(1 + strlen((char*) szAttr));

			assert(pTmp);

			strcpy(pTmp, (const char*) szAttr);
			g_gateway.config.load_balancer_domain_names[2] = pTmp;

			printf("dlb1=%s\n", g_gateway.config.load_balancer_domain_names[2]);
		}

		if (xmlHasProp(curNode, BAD_CAST "udp_port")) {
			szAttr = xmlGetProp(curNode, BAD_CAST "udp_port");
			g_gateway.config.load_balancer_udp_port = atoi((char*)szAttr);

			printf("udp=%d\n", g_gateway.config.load_balancer_udp_port);
		}

		if (xmlHasProp(curNode, BAD_CAST "tcp_port")) {
			szAttr = xmlGetProp(curNode, BAD_CAST "tcp_port");
			g_gateway.config.load_balancer_tcp_port = atoi((char*)szAttr);
			printf("udp=%d\n", g_gateway.config.load_balancer_tcp_port);
		}

		if (xmlHasProp(curNode, BAD_CAST "productid")) {
			szAttr = xmlGetProp(curNode, BAD_CAST "tcp_port");
			g_gateway.config.product_id = atoi((char*)szAttr);
			printf("udp=%d\n", g_gateway.config.product_id);
		}

		if (xmlHasProp(curNode, BAD_CAST "product_aes")) {
			szAttr = xmlGetProp(curNode, BAD_CAST "product_aes");
			pTmp = (char*) malloc(1 + strlen((char*) szAttr));

			assert(pTmp);

			strcpy(pTmp, (const char*) szAttr);
			g_gateway.config.product_aes_key = pTmp;
			printf("aes=%s\n", g_gateway.config.product_aes_key);
		}

		if (xmlHasProp(curNode, BAD_CAST "code")) {
			szAttr = xmlGetProp(curNode, BAD_CAST "code");
			pTmp = (char*) malloc(1 + strlen((char*) szAttr));

			assert(pTmp);

			strcpy(pTmp, (const char*) szAttr);
			g_gateway.config.device_name = pTmp;
			printf("devicename=%s\n", g_gateway.config.device_name);
		}

		if (xmlHasProp(curNode, BAD_CAST "code_suffix")) {
			szAttr = xmlGetProp(curNode, BAD_CAST "code_suffix");
			pTmp = (char*) malloc(1 + strlen((char*) szAttr));

			assert(pTmp);

			strcpy(pTmp, (const char*) szAttr);
			g_gateway.config.device_password = pTmp;
			printf("devicepwd=%s\n", g_gateway.config.device_password);
		}

		if (xmlHasProp(curNode, BAD_CAST "devic_aes")) {
			szAttr = xmlGetProp(curNode, BAD_CAST "devic_aes");
			pTmp = (char*) malloc(1 + strlen((char*) szAttr));

			assert(pTmp);

			strcpy(pTmp, (const char*) szAttr);
			g_gateway.config.device_aes_key = pTmp;
			printf("g_gateway.config.device_aes_key=%s\n", g_gateway.config.device_aes_key);
		}

		if (xmlHasProp(curNode, BAD_CAST "multi_attribute")) {
			szAttr = xmlGetProp(curNode, BAD_CAST "multi_attribute");
			g_gateway.config.device_can_multi_attribute = atoi((char*)szAttr);
		}
	}

	curNode = curNode->next;
	if (0 == strcmp((char*)curNode->name, "children")) {

		childNode = curNode->children;

		while (childNode) {

			child = malloc(sizeof(ST_CHILD));

			assert(child);

			if (xmlHasProp(childNode, BAD_CAST "index")) {
				szAttr = xmlGetProp(child, BAD_CAST "index");
				child->index = atoi((char*)szAttr);
			}

			if (xmlHasProp(childNode, BAD_CAST "addr")) {
				szAttr = xmlGetProp(child, BAD_CAST "index");
				inet_pton(AF_INET6, (const char*)szAttr, &(child->addr));
			}

			if (xmlHasProp(childNode, BAD_CAST "type")) {
				szAttr = xmlGetProp(child, BAD_CAST "type");
				child->type = atoi((char*)szAttr);
			}

			fillDeviceCode(child);
			insertChild((const ST_CHILD**)&(g_gateway.firstChild),child);
		}
	}

	/* close clean */
	xmlFreeDoc(pdoc);
	xmlCleanupParser();

	return ARRAYENT_SUCCESS;
}

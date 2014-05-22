/*
 * gateway.h
 *
 *  Created on: May 22, 2014
 *      Author: walker
 */

#ifndef GATEWAY_H_
#define GATEWAY_H_
#include <arpa/inet.h>

int AxLogin_JIP(struct in6_addr* addr, uint8_t type);
int AxLogout_JIP(struct in6_addr* addr);
int AxDelete_JIP(struct in6_addr* addr);
int AxMsg_JIP(struct in6_addr* addr,const char* cmd,const char* value);

#endif /* GATEWAY_H_ */

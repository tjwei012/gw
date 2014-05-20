/*
 * jip_cli.h
 *
 *  Created on: May 19, 2014
 *      Author: axalent
 */

#ifndef JIP_CLI_H_
#define JIP_CLI_H_
#include "JIP.h"
int jip_ipv6 (char *arg);
int jip_device (char *arg);
int jip_mib (char *arg);
int jip_var (char *arg);
int jip_set (char *arg);
int jip_print(char *arg);
int jip_discover (char *arg);
int jip_print_node (tsNode *psNode, void *pvUser);
#endif /* JIP_CLI_H_ */

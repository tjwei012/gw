#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <libtecla.h>
#include "jip_cli.h"
#include "gateway.h"

#include <JIP.h>

/** JIP context structure */
extern tsJIP_Context sJIP_Context;

/** When non-zero, this global means the user is done using this program. */
extern int iQuit;

/** Port to conenct to */
extern int iPort;

/* Filter variables */
extern char *filter_ipv6;
extern char *filter_device;
extern char *filter_mib;
extern char *filter_var;

/* Connection details */
char *pcBorderRouterIPv6Address = "::0";
char *pcGatewayIPv4Address = "0.0.0.0";
int iUse_TCP = 0; /* UDP by default */

void CbJipNetworkChange(teJIP_NetworkChangeEvent eEvent, struct _tsNode *psNode)
{
	char str[32];
	struct in6_addr* addr = &psNode->sNode_Address.sin6_addr;
	switch(eEvent)
	{
	case E_JIP_NODE_JOIN:
		sprintf(str,"join ");
		AxLogin_JIP(addr,2);
		break;
	case E_JIP_NODE_LEAVE:
		sprintf(str,"leave ");
		AxLogout_JIP(addr);
		break;
	case E_JIP_NODE_MOVE:
		sprintf(str,"move ");
		AxDelete_JIP(addr);
		break;
	default:
		break;
	}
	printf("xxxxxxxxxxxxxx node %s :ID %d\n",str,psNode->u32DeviceId);

}


int AxMsg_JIP(struct in6_addr* addr,const char* cmd,const char* value)
{
	//int data = atoi(value);
	char ipv6_addr[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, addr, ipv6_addr, INET6_ADDRSTRLEN);
	printf("%s\n", ipv6_addr);

	if(strcmp(cmd,"permitjion") == 0)
	{

	}
	if(strcmp(cmd,"delete") == 0)
	{
		JipSet(ipv6_addr,"BulbControl","",value);
	}
	if(strcmp(cmd,"light") == 0)
	{
		JipSet(ipv6_addr,"BulbControl","Mode",value);
	}
	if(strcmp(cmd,"lum") == 0)
	{
		JipSet(ipv6_addr,"BulbControl","LumCurrent",value);
	}
}

int JipAccess()
{
	FILE *f,*fb;
	char *line = NULL;
	size_t len = 0;
	int i = 0;
	f = fopen("/etc/freeradius2/users.6LoWPAN", "r");
	if(f == NULL)
	{
		printf("open file error");
	}
	fb = fopen("/tmp/ax.tmp", "wt+");
	if(fb == NULL)
	{
		printf("open file error");
	}
//	killall -HUP radiusd
	while(getline(&line,&len,f) != -1)
	{
		i++;
		printf("line %d :%s\n",i,line);
		if(i > 5)
		{
			if((NULL != strstr(line,"Cleartext-Password"))||(NULL != strstr(line,"Vendor-Specific")))
			{
				if(*line == '#')
					*line = ' ';
			}
		}
		fwrite(line,strlen(line),1,fb);
	}
	if (line)
		free(line);
	fclose(f);
	fclose(fb);
	system("mv -f /tmp/ax.tmp /etc/freeradius2/users.6LoWPAN");
	system("killall -HUP radiusd  >/dev/null 2>&1");

}

int JipInit()
{

	JipAccess();

    if (eJIP_Init(&sJIP_Context, E_JIP_CONTEXT_CLIENT) != E_JIP_OK)
    {
        printf("JIP startup failed\n\r");
        return -1;
    }
    if (eJIP_Connect4(&sJIP_Context, pcGatewayIPv4Address, pcBorderRouterIPv6Address, iPort, iUse_TCP) != E_JIP_OK)
	{
		printf("JIP connect failed\n\r");
		return -1;
	}

    eJIPService_MonitorNetwork(&sJIP_Context,CbJipNetworkChange);
    return 0;
}


int JipSet(char *addr,char *mib,char *var,char *value)
{
	 jip_ipv6 (addr);
	 //jip_device ("0x08040001");
	 jip_mib (mib);
	 jip_var (var);
	 jip_set (value);
	 return 0;
}








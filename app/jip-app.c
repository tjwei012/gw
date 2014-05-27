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
#include <unistd.h>
#include "jip_cli.h"
#include "gateway.h"
#include "jia_app.h"
#include <JIP.h>

/** JIP context structure */
extern tsJIP_Context sJIP_Context;

/** Port to conenct to */
extern int iPort;

/* Connection details */
char *pcBorderRouterIPv6Address = "::0";
char *pcGatewayIPv4Address = "0.0.0.0";
int iUse_TCP = 0; /* UDP by default */
int JipAddFlag = 0;
void CbJipNetworkChange(teJIP_NetworkChangeEvent eEvent, struct _tsNode *psNode)
{
	char str[32];
	char ipv6_addr[INET6_ADDRSTRLEN];
	struct in6_addr* addr = &psNode->sNode_Address.sin6_addr;
	inet_ntop(AF_INET6, addr, ipv6_addr, INET6_ADDRSTRLEN);
//	uint64_t u64MAC_Address;
//	unsigned char mac[13];
//	memcpy(&u64MAC_Address,&addr->s6_addr[8],sizeof(uint64_t));
//	u64MAC_Address = be64toh(u64MAC_Address);
//	sprintf(mac,"%02X%02X%02X%02X%02X%02X",
//			addr->s6_addr[10],
//			addr->s6_addr[11],
//			addr->s6_addr[12],
//			addr->s6_addr[13],
//			addr->s6_addr[14],
//			addr->s6_addr[15]);
	switch(eEvent)
	{
	case E_JIP_NODE_JOIN:
		sprintf(str,"join");
		AxLogin_JIP(addr,2);
		break;
	case E_JIP_NODE_LEAVE:
		sprintf(str,"leave");
		AxLogout_JIP(addr);
		break;
	case E_JIP_NODE_MOVE:
		sprintf(str,"move");
		AxDelete_JIP(addr);
		break;
	default:
		break;
	}
	printf("/***********************************\n");
	printf("node %s! IP: %s\n",str,ipv6_addr);
	printf("************************************/\n");

}


int AxMsg_JIP(struct in6_addr* addr,const char* cmd,const char* value)
{
	//int data = atoi(value);
	char ipv6_addr[INET6_ADDRSTRLEN];

	printf("AxMsg_JIP: cmd=%s, value=%s\n", cmd, value);

	if(addr == NULL){

		if(strcmp(cmd,"permitjoin") == 0)
		{
			printf("add\n");
			JipAccessAdd();
//			JipAddFlag = 1;
		}

	}else{

		inet_ntop(AF_INET6, addr, ipv6_addr, INET6_ADDRSTRLEN);

		if(strcmp(cmd,"light") == 0)
		{
			JipSet(ipv6_addr,"BulbControl","Mode",value);
			printf("set light: %s\n", value);
		}

		if(strcmp(cmd,"lum") == 0)
		{
			JipSet(ipv6_addr,"BulbControl","LumCurrent",value);
		}

		if(strcmp(cmd,"deletedevice") == 0)
		{

			printf("11\n");
			JipSet(ipv6_addr,"NodeControl","FactoryReset","1");
			JipNodeRemove(&sJIP_Context,addr);
			printf("22\n");
			JipAccessRemove(addr);
			printf("33\n");
		}
	}
	return 0;
}

int JipAccessAdd()
{
	printf("addb\n");
//	DBG_vPrintf( "%s\n", __FUNCTION__);
	FILE *f,*fb;
	char *line = NULL,*pTmp=NULL;
	size_t len = 0;
	int i = 0;
	f = fopen("/etc/freeradius2/users.6LoWPAN", "r");
	if(f == NULL)
	{
		printf("open file error");
		return -1;
	}
	fb = fopen("/tmp/ax.tmp", "wt+");
	if(fb == NULL)
	{
		printf("open file error");
		return -1;
	}

	while(getline(&line,&len,f) != -1)
	{
		i++;
		printf("line %d :%s\n",i,line);
		pTmp = line;
		if(i > 5)
		{
			if((pTmp[0] == '#')	&& (NULL != strstr(line,"Cleartext-Password")))
			{
					while(pTmp[0]==' '||pTmp[0]=='#')pTmp++;
					printf("line:%s\n",pTmp);
			}

			if((pTmp[0] == '#')&&(NULL != strstr(line,"Vendor-Specific"))){
				while(pTmp[0]=='#')pTmp++;
				printf("line:%s\n",pTmp);
			}
		}
		fwrite(pTmp,strlen(pTmp),1,fb);
	}

	if (line)
		free(line);

	fclose(f);
	fclose(fb);
	system("/etc/init.d/radiusd disable");
	system("/etc/init.d/radiusd stop");
	system("cp -f /tmp/ax.tmp /etc/freeradius2/users.6LoWPAN");
	system("/etc/init.d/radiusd start");
	system("/etc/init.d/radiusd enable");

	printf("addss\n");
	JipReset();
	printf("reset success\n");
	return 0;
}

int JipAccessRemove(struct in6_addr* addr)
{
//	DBG_vPrintf( "%s\n", __FUNCTION__);
	FILE *f,*fb;
	char *line = NULL;
	size_t len = 0;
	int i = 0;
	char mac[13];
	int nextflag = 0;
	sprintf(mac,"%02X%02X%02X%02X%02X%02X",
			addr->s6_addr[10],
			addr->s6_addr[11],
			addr->s6_addr[12],
			addr->s6_addr[13],
			addr->s6_addr[14],
			addr->s6_addr[15]);

	printf("mac = %s\n", mac);

	f = fopen("/etc/freeradius2/users.6LoWPAN", "r");
	if(f == NULL)
	{
		printf("open file error\n");
		return -1;
	}
	fb = fopen("/tmp/ax.tmp", "wt+");
	if(fb == NULL)
	{
		printf("open file error\n");
		return -1;
	}

	while(getline(&line,&len,f) != -1)
	{
		i++;
		printf("line %d :%s\n",i,line);
		if(i > 5)
		{
			if((NULL != strstr(line,"Cleartext-Password"))||(NULL != strstr(line,"Vendor-Specific")))
			{
				if(NULL != strstr(line,mac))
				{
					if(*line != '#') fputc('#',fb);
					nextflag = 1;
					fwrite(line,strlen(line),1,fb);
					continue;
				}
				if(nextflag)
				{
					if(*line != '#') fputc('#',fb);
					nextflag = 0;
				}
			}
		}
		fwrite(line,strlen(line),1,fb);
	}
	if (line)
		free(line);
	fclose(f);
	fclose(fb);
	system("/etc/init.d/radiusd disable");
	system("/etc/init.d/radiusd stop");
	system("cp -f /tmp/ax.tmp /etc/freeradius2/users.6LoWPAN");
	system("/etc/init.d/radiusd start");
	system("/etc/init.d/radiusd enable");

	JipReset();
	printf("reset success\n");
	return 0;
}

int JipAccessCmp()
{
//	DBG_vPrintf( "%s\n", __FUNCTION__);
	FILE *f1,*f2;
	char c1,c2;
	f1 = fopen("/etc/freeradius2/users.6LoWPAN", "r");
	if(f1 == NULL)
	{
		printf("open file error\n");
		return -1;
	}
	f2 = fopen("/tmp/ax.tmp", "r");
	if(f2 == NULL)
	{
		printf("open file error\n");
		return -1;
	}
	while(!feof(f1) && !feof(f2))
	{
		c1 = fgetc(f1);
		c2 = fgetc(f2);
		//printf("c1: %c ,c2: %c\n",c1,c2);
		if(c1 != c2)
		{
			fclose(f1);
			fclose(f2);
			return -1;
		}
	}
	fclose(f1);
	fclose(f2);
	return 0;
}

void *JipAccessWhitelist(void *arg)
{
	int count = 0;
	while(1){
		if(JipAddFlag){
			if(JipAccessCmp() != 0 ){
				JipAccessAdd();
				JipAddFlag = 0;
				count = 0;
			}else{
				if(count < 15) count ++;
				else{
					count = 0;
					JipAddFlag = 0;
				}
			}
		}
		sleep(2);
	}

}

int JipInit()
{

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
//
//    pthread_t thread_access;
//    pthread_create(&thread_access, NULL,JipAccessWhitelist, NULL);

    eJIPService_MonitorNetwork(&sJIP_Context,CbJipNetworkChange);
    return 0;
}

int JipReset()
{

	eJIPService_MonitorNetworkStop(&sJIP_Context);

	if(eJIP_Destroy(&sJIP_Context) != E_JIP_OK){
		printf("JIP destroy failed\n\r");
		return -1;
	}


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


teJIP_Status JipNodeRemove(tsJIP_Context *psJIP_Context,  struct in6_addr* addr)
{

	tsNode *psNode = NULL;
	tsNetwork *psNet = &psJIP_Context->sNetwork;
	tsJIPAddress   sAddress;
	//teJIP_Status eStatus = E_JIP_ERROR_FAILED;

	memset (&sAddress, 0, sizeof(struct sockaddr_in6));
	sAddress.sin6_family  = AF_INET6;
	sAddress.sin6_port    = htons(JIP_DEFAULT_PORT);
	memcpy(&sAddress.sin6_addr.s6_addr[0],addr,sizeof(struct in6_addr));

	psNode = psJIP_LookupNode(psJIP_Context,&sAddress);
	if (psNode)
	{
//		/* Got pointer to the node, lock the linked list now so that we can remove it. */
//		eJIP_Lock(psJIP_Context);
//		(void)psJIP_NodeListRemove(&psJIP_Context->sNetwork.psNodes, psNode);
//
//		/* Decrement count of nodes */
//		psNet->u32NumNodes--;
//		//eStatus = E_JIP_OK;
//		eJIP_Unlock(psJIP_Context);
	}
	else
	{
		DBG_Printf(DBG_JIP, "Failed to remove node\n");
		return E_JIP_ERROR_FAILED;
	}

	/* Node found to remove from the network */
	DBG_Printf_IPv6Address(DBG_JIP, psNode->sNode_Address.sin6_addr);
	CbJipNetworkChange(E_JIP_NODE_MOVE,psNode);

    /* Now we can free the node */
 //   return eJIP_NetFreeNode(psJIP_Context, psNode);
}





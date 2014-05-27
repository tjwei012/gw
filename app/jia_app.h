/*
 * jia_app.h
 *
 *  Created on: May 22, 2014
 *      Author: axalent
 */

#ifndef JIA_APP_H_
#define JIA_APP_H_

//# if __BYTE_ORDER == __LITTLE_ENDIAN
//#warning little endian
//#  define htobe64(x) __bswap_64 (x)
//#  define htole64(x) (x)
//#  define be64toh(x) __bswap_64 (x)
//#  define le64toh(x) (x)
//#else
//#warning big endian
//#  define htobe64(x) (x)
//#  define htole64(x) __bswap_64 (x)
//#  define be64toh(x) (x)
//#  define le64toh(x) __bswap_64 (x)
//#endif /* Byte order */

#define DBG_JIP 1
#define DBG_Printf(a,b,ARGS...) do {  if (a) printf(b, ## ARGS); } while(0)
#define DBG_Printf_IPv6Address(a,b) do {                                   \
    if (a) {                                                                \
        char buffer[INET6_ADDRSTRLEN] = "Could not determine address\n";    \
        inet_ntop(AF_INET6, &b, buffer, INET6_ADDRSTRLEN);                  \
        printf("%s\n", buffer);                                             \
    }                                                                       \
} while(0)

void CbJipNetworkChange(teJIP_NetworkChangeEvent eEvent, struct _tsNode *psNode);
int AxMsg_JIP(struct in6_addr* addr,const char* cmd,const char* value);
int JipAccess();
int JipInit();
int JipSet(char *addr,char *mib,char *var,char *value);
void *JipAccessWhitelist(void *arg);
int JipAccessAdd();
int JipAccessRemove(struct in6_addr* addr);
int JipAccessCmp();
teJIP_Status JipNodeRemove(tsJIP_Context *psJIP_Context,  struct in6_addr* addr);
#endif /* JIA_APP_H_ */

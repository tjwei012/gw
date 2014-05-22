/*
 ============================================================================
 Netvox_example:

 Demonstrate that a system consists of a gateway, a motion sensor, a smoke sensor and a switch.

 1.a gateway will connect the server first.
 2.Create the login sessions for the child devices. Here is motion sensor, smoke senor and switch.
 3.Send and receive property messages for the child devices.

 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "aca.h"

extern int JipSet(char *value);
extern int JipInit();
/******************************************************
 *                    Constants
 ******************************************************/

/* Device Configuration Table
 *
 * The sample application uses WICED's Device Configuration Table (DCT) to store
 * persistent information, such as Wi-Fi Access Point credentials (see
 * wifi_config_dct.h), device credentials for authenticating devices that want
 * to connect to the Arrayent Cloud, security keys for encrypting/decrypting messages
 * to/from the Arrayent Cloud, and so on. The DCT is part of the WICED platform,
 * and is provided as a means for simplifying access to non-volatile flash.
 * All questions regarding the DCT should be directed to Broadcom. */

/* Device Credentials
 *
 * The Device Name and Device Password below comprise the unique credentials
 * for your device. The combination of Device Name and Device Password is called
 * a Device Code. You can think of the Device Code as the Arrayent Cloud's
 * username and password for this device. When you initialize the ACA,
 * you provide it a Device Code. The ACA logs in to the Arrayent Cloud using
 * these credentials, and the Arrayent Cloud maps useful information
 * (such as IP address) to the Device Code so that the Cloud can route subsequent
 * messages back to the device.
 *
 * Note that the credentials below are shared across all users of the WICED IDE,
 * which is likely to result in sporadic device behavior if two users are attempting
 * to control their devices at the same time. See temp_control.c for more information
 * on this and on how to get a unique set of device credentials. */
#define ARR_GATEWAY_NAME         "NXP000000"
#define ARR_GATEWAY_PASSWORD     "5D"

#define ARR_DEVICE_NAME1         "NXP000001"
#define ARR_DEVICE_PASSWORD1     "XX"

//#define ARR_DEVICE_NAME2         "NETVOX0002"
//#define ARR_DEVICE_PASSWORD2     "57"
//
//#define ARR_DEVICE_NAME3         "NETVOX0003"
//#define ARR_DEVICE_PASSWORD3     "D7"



/* The Device Key (ARR_DEVICE_AES_KEY) and Product Key (ARR_PRODUCT_AES_KEY)
 * below are part of Arrayent's protocol for securing communications between
 * devices and the Arrayent Cloud. As an application developer, Arrayent's
 * security mechanism is transparent. All you need to do is store the security
 * credentials in non-volatile flash and supply the credentials to the ACA when
 * you initialize it. The ACA will take care of the rest. */

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

#ifndef UINT32_MAX
#define UINT32_MAX  (0xffffffff)
#endif

uint8_t g_motion_triggered = 0;
uint8_t g_smoke_triggered = 0;
uint8_t g_switch_status = 0 ;

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

/*
 * emulates sending property when a motion or smoke is detected.
 */
void* sendPropertyOfChild(void* arg){

	while(1){
		if(checkArrayentConnection()==0){

			/*generate a motion randomly*/
			g_motion_triggered = ((rand()%20)==0);

			if(g_motion_triggered){
				if(ArrayentSetChildProperty(1, "motion 1") != ARRAYENT_SUCCESS) {
				   printf("Send switch to Arrayent server failed\r\n");
				}

				g_motion_triggered = 0;
			}

//			if(g_smoke_triggered){
//
//				if(ArrayentSetChildProperty(2, "smoke 1") != ARRAYENT_SUCCESS) {
//					printf("Send switch to Arrayent server failed\r\n");
//				  }
//
//				g_smoke_triggered = 0;
//			}

		}

		/*sleep 1 s*/
		sleep(1);
	}

	return NULL;

}

int main(void) {
#if 0
    arrayent_config_t arrayent_config;
    arrayent_config.product_id = ARR_PRODUCT_ID;
    arrayent_config.product_aes_key = ARR_PRODUCT_AES_KEY;
    arrayent_config.load_balancer_domain_names[0] = ARR_LB_HOSTNAME_1;
    arrayent_config.load_balancer_domain_names[1] = ARR_LB_HOSTNAME_2;
    arrayent_config.load_balancer_domain_names[2] = ARR_LB_HOSTNAME_3;
    arrayent_config.load_balancer_udp_port = ARR_LB_UDP_PORT;
	arrayent_config.load_balancer_tcp_port = ARR_LB_TCP_PORT;
	arrayent_config.device_name = ARR_GATEWAY_NAME;
	arrayent_config.device_password = ARR_GATEWAY_PASSWORD;
	arrayent_config.device_aes_key = ARR_GATEWAY_AES_KEY;

    /*config the ACA*/
    arrayent_return_e r = ArrayentConfigure(&arrayent_config);
    if (r != ARRAYENT_SUCCESS) {
        printf("Configuring Arrayent failed!");
        return -1;
    }

    // Start the Arrayent stack
   if(ArrayentInit() != ARRAYENT_SUCCESS) {
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

   ArrayentChildLogin(1, ARR_DEVICE_NAME1, ARR_DEVICE_PASSWORD1);
//   ArrayentChildLogin(2, ARR_DEVICE_NAME2, ARR_DEVICE_PASSWORD2);
//   ArrayentChildLogin(3, ARR_DEVICE_NAME3, ARR_DEVICE_PASSWORD3);

   pthread_t thread;
   pthread_create(&thread, NULL, sendPropertyOfChild, NULL);


   uint8_t userIndex = 0;
   char buffer[128];
   uint16_t len = 0;
   int ret = 0;
#endif
   JipInit();

   while(1);

   /*receive property*/
#if 0
   while(1){

	   len = sizeof(buffer);

       if((ret = ArrayentRecvProperty(&userIndex, buffer, &len, UINT32_MAX))
               != ARRAYENT_SUCCESS) {
           printf("Failed to read message from Arrayent with error %d\r\n", ret);
           continue;
       }

       if(len){
    	   printf("Received property \"%s\" from device %d.\n\r", buffer, userIndex);
    	   if(strcmp(buffer,"") == 0)
    	   if(strcmp(buffer,"light 0") == 0)
			   JipSet("0");
    	   if(strcmp(buffer,"light 1") == 0)
    		   JipSet("1");
       }

       usleep(5);

   }
#endif
	return EXIT_SUCCESS;

}

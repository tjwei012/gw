/*
 * main.c
 *
 *  Created on: May 22, 2014
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "gateway.h"

int main(void) {


	initGateway();

	eBR_Init();

	while(1){
		usleep(1);
	}
	return EXIT_SUCCESS;
}

/*
 * init.c
 *
 *  Created on: Mar 19, 2025
 *      Author: es23018
 */

#include "FreeRTOS.h"
#include "task.h"
#include "stdio.h"
#include "Console.h"
#include "main.h"
#include "init.h"

static int CapabilityFunc( int argc, char** argv, void* ctx )
{
	(void)argc;
	(void)argv;
	(void)ctx;
	printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\nOK",
	    1, // has spindle
		1, // has spindle status
		1, // has stepper
		1, // has stepper move relative
		1, // has stepper move speed
		1, // has stepper move async
		1, // has stepper status
		1, // has stepper refrun
		1, // has stepper refrun timeout
		1, // has stepper refrun skip
		1, // has stepper refrun stay enabled
		1, // has stepper reset
		1, // has stepper position
		1, // has stepper config
		1, // has stepper config torque
		1, // has stepper config throvercurr
		1, // has stepper config powerena
		1, // has stepper config stepmode
		1, // has stepper config timeoff
		1, // has stepper config timeon
		1, // has stepper config timefast
		1, // has stepper config mmperturn
		1, // has stepper config posmax
		1, // has stepper config posmin
		1, // has stepper config posref
		1, // has stepper config stepsperturn
		1  // has stepper cancel
	);
	return 0;
}



void init(TIM_HandleTypeDef tim_handle, SPI_HandleTypeDef* hspi1, TIM_HandleTypeDef* tim1_handle, TIM_HandleTypeDef* tim4_handle) {
	  // set up console
	  ConsoleHandle_t console_handle = CONSOLE_CreateInstance( 4*configMINIMAL_STACK_SIZE, configMAX_PRIORITIES - 5  );

	  CONSOLE_RegisterCommand(console_handle, "capability", "prints a specified string of capability bits", CapabilityFunc, NULL);


	  init_spindle(console_handle, tim_handle);
	  init_stepper(console_handle, hspi1, tim1_handle, tim4_handle);
}

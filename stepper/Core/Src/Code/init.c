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

static int CapabilityFunc( int argc, char** argv, void* ctx )
{
	(void)argc;
	(void)argv;
	(void)ctx;
	printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\nOK",
	    0, // has spindle
		0, // has spindle status
		0, // has stepper
		0, // has stepper move relative
		0, // has stepper move speed
		0, // has stepper move async
		0, // has stepper status
		0, // has stepper refrun
		0, // has stepper refrun timeout
		0, // has stepper refrun skip
		0, // has stepper refrun stay enabled
		0, // has stepper reset
		0, // has stepper position
		0, // has stepper config
		0, // has stepper config torque
		0, // has stepper config throvercurr
		0, // has stepper config powerena
		0, // has stepper config stepmode
		0, // has stepper config timeoff
		0, // has stepper config timeon
		0, // has stepper config timefast
		0, // has stepper config mmperturn
		0, // has stepper config posmax
		0, // has stepper config posmin
		0, // has stepper config posref
		0, // has stepper config stepsperturn
		0  // has stepper cancel
	);
	return 0;
}




void init(TIM_HandleTypeDef tim_handle) {
	  // set up console
	  ConsoleHandle_t console_handle = CONSOLE_CreateInstance( 4*configMINIMAL_STACK_SIZE, configMAX_PRIORITIES - 5  );

	  CONSOLE_RegisterCommand(console_handle, "capability", "prints a specified string of capability bits", CapabilityFunc, NULL);

	  init_spindle(console_handle, tim_handle);
}

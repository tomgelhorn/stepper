/*
 * spindle.c
 *
 *  Created on: Mar 19, 2025
 *      Author: es23018
 */
#include "Spindle.h"
#include "Console.h"
#include "FreeRTOSConfig.h"
#include "main.h"

typedef struct {
	int direction;
	TIM_HandleTypeDef pwm;
} SpindleContext;

void SPINDLE_SetDirection(SpindleHandle_t h, void* context, int direction) {
	(void) h;

	SpindleContext* ctx = (SpindleContext*) context;

	ctx->direction = direction;
}

void SPINDLE_SetDutyCycle(SpindleHandle_t h, void* context, float duty) {
	(void) h;

	SpindleContext* ctx = (SpindleContext*) context;

	int arr = TIM2->ARR;

	if (ctx->direction) {
		TIM2->CCR3 = 0;
		TIM2->CCR4 = (int)((float)arr * duty);
	}
	else {
		TIM2->CCR3 = (int)((float)arr * duty);
		TIM2->CCR4 = 0;
	}
}

void SPINDLE_EnaPWM(SpindleHandle_t h, void* context, int enable) {
	(void) h;

	SpindleContext* ctx = (SpindleContext*) context;

	HAL_GPIO_WritePin(SPINDLE_ENA_L_GPIO_Port, SPINDLE_ENA_L_Pin, enable);
	HAL_GPIO_WritePin(SPINDLE_ENA_R_GPIO_Port, SPINDLE_ENA_R_Pin, enable);

	if (enable) {
		HAL_TIM_PWM_Start(&ctx->pwm, TIM_CHANNEL_3);
		HAL_TIM_PWM_Start(&ctx->pwm, TIM_CHANNEL_4);
	}
	else {
		HAL_TIM_PWM_Stop(&ctx->pwm, TIM_CHANNEL_3);
		HAL_TIM_PWM_Stop(&ctx->pwm, TIM_CHANNEL_4);
	}
}

SpindleContext ctx;

void init_spindle(ConsoleHandle_t console_handle, TIM_HandleTypeDef tim_handle) {
	ctx.direction = 0;
	ctx.pwm = tim_handle;

	// set up spindle
	SpindlePhysicalParams_t spindle_params;
	spindle_params.maxRPM             =  9000.0f;
	spindle_params.minRPM             = -9000.0f;
	spindle_params.absMinRPM          =  1600.0f;
	spindle_params.setDirection       = SPINDLE_SetDirection;
	spindle_params.setDutyCycle       = SPINDLE_SetDutyCycle;
	spindle_params.enaPWM             = SPINDLE_EnaPWM;
	spindle_params.context            = &ctx;

	SPINDLE_CreateInstance( 4*configMINIMAL_STACK_SIZE, configMAX_PRIORITIES - 3, console_handle, &spindle_params);
}

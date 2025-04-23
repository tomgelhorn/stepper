#include "main.h"
#include "init.h"
#include "LibL6474.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "LibL6474Config.h"
#include "stm32f7xx_hal_spi.h"
#include "stm32f7xx_hal_tim.h"
#include "FreeRTOS.h"
#include "task.h"

typedef struct {
	L6474_Handle_t h;
	int is_powered;
	int is_referenced;
	int is_canceled;

	int position;

	TaskHandle_t step_task;
} StepperContext;

static int StepTimerCancelAsync(void* pPWM);

static void* StepLibraryMalloc( unsigned int size )
{
     return malloc(size);
}

static void StepLibraryFree( const void* const ptr )
{
     free((void*)ptr);
}

static int StepDriverSpiTransfer( void* pIO, char* pRX, const char* pTX, unsigned int length )
{
	// byte based access, so keep in mind that only single byte transfers are performed!

	HAL_StatusTypeDef status = 0;

	for ( unsigned int i = 0; i < length; i++ )
	{
		HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 0);

		status |= HAL_SPI_TransmitReceive(pIO, pTX + i, pRX + i, 1, 10000);

		HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);

		HAL_Delay(1);
	}

	if (status != HAL_OK) {
		return -1;
	}

	return 0;
}

static void StepDriverReset(void* pGPO, int ena)
{
	HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, !ena);

	return;
}

static void StepLibraryDelay()
{
	return;
}

static void StepTimer( void* pPWM, int dir, unsigned int numPulses )
{
	HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, !!dir);
	for (int i = 0; i < numPulses; i++) {
//		HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, 1);
//		HAL_Delay(1);
//		HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, 0);
		HAL_Delay(1);
	}
}

typedef struct {
	void *pPWM;
	int dir;
	unsigned int numPulses;
	void (*doneClb)(L6474_Handle_t);
	L6474_Handle_t h;
} StepTaskParams;


static int reset(StepperContext* stepper_ctx){
	L6474_BaseParameter_t param;
	param.stepMode = smMICRO16;
	param.OcdTh = ocdth6000mA;
	param.TimeOnMin = 0x29;
	param.TimeOffMin = 0x29;
	param.TorqueVal = 0x11;
	param.TFast = 0x19;

	int result = 0;

	result |= L6474_ResetStandBy(stepper_ctx->h);
	result |= L6474_Initialize(stepper_ctx->h, &param);

	result |= L6474_SetPowerOutputs(stepper_ctx->h, 0);


	stepper_ctx->is_powered = 0;
	stepper_ctx->is_referenced = 0;


	return result;
}

static int powerena(StepperContext* stepper_ctx, int argc, char** argv) {
	if (argc == 2) {
		printf("Current Powerstate: %d\r\n", stepper_ctx->is_powered);
		return 0;
	}
	else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
		int ena = atoi(argv[3]);
		if (ena != 0 && ena != 1) {
			printf("Invalid argument for powerena\r\n");
			return -1;
		}
		stepper_ctx->is_powered = ena;
		return L6474_SetPowerOutputs(stepper_ctx->h, ena);
	}
	else {
		printf("Invalid number of arguments\r\n");
		return -1;
	}

}

static int reference(StepperContext* stepper_ctx, int argc, char** argv) {
	stepper_ctx->is_referenced = 1; //hehe
	return 0;
}


static int config(StepperContext* stepper_ctx, int argc, char** argv) {
	if (argc < 2) {
		printf("Invalid number of arguments\r\n");
		return -1;
	}
	if (strcmp(argv[1], "powerena") == 0) {
		return powerena(stepper_ctx, argc, argv);
	}
	else {
		printf("Invalid command\r\n");
		return -1;
	}
}

static int move(StepperContext* stepper_ctx, int steps) {
	if (stepper_ctx->is_powered != 1) {
		printf("Stepper not powered\r\n");
		return -1;
	}
	if (stepper_ctx->is_referenced != 1) {
		printf("Stepper not referenced\r\n");
		return -1;
	}
	return L6474_StepIncremental(stepper_ctx->h, steps);
}

static int initialize(StepperContext* stepper_ctx) {
	reset(stepper_ctx);
	stepper_ctx->is_powered = 1;
	stepper_ctx->is_referenced = 1;
	stepper_ctx->position = 0;

	return L6474_SetPowerOutputs(stepper_ctx->h, 1);
}

static int stepperConsoleFunction(int argc, char** argv, void* ctx) {
	StepperContext* stepper_ctx = (StepperContext*)ctx;
	int result = 0;

	if (argc == 0) {
		printf("Invalid number of arguments\r\n");
		return -1;
	}
	if (strcmp(argv[0], "move") == 0 )
	{
		result = move(stepper_ctx, atoi(argv[1]));
	}
	else if (strcmp(argv[0], "reset") == 0) {
		result = reset(stepper_ctx);
	}
	else if (strcmp(argv[0], "config") == 0) {
		result = config(stepper_ctx, argc, argv);
	}
	else if (strcmp(argv[0], "reference") == 0) {
		result = reference(stepper_ctx, argc, argv);
	}
	else if (strcmp(argv[0], "cancel") == 0) {
		result = StepTimerCancelAsync(NULL);
	}
	else if (strcmp(argv[0], "init") == 0){
		result = initialize(stepper_ctx);
	}
	else {
		printf("Invalid command\r\n");
		return -1;
	}
	if (result == 0) {
		printf("OK\r\n");
	}
	else {
		printf("FAIL\r\n");
	}
	return result;
}

L6474x_Platform_t p;
StepperContext stepper_ctx;

static void StepTask(void* p){
	StepTaskParams* params = (StepTaskParams*)p;
	HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, !!params->dir);
	for (unsigned int i = 0; i < params->numPulses; i++) {
		if (stepper_ctx.is_canceled) {
			break;
		}
//		HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, 1);
//		HAL_Delay(1);
//		HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, 0);
		HAL_Delay(1);
	}

	params->doneClb(params->h);


	free(params);
	StepTimerCancelAsync(NULL);
	vTaskDelete(NULL);
}

static int StepAsyncTimer(void *pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t),L6474_Handle_t h){
//	StepTaskParams* params = (StepTaskParams*)pvPortMalloc(sizeof(StepTaskParams));
//	params->pPWM = pPWM;
//	params->dir = dir;
//	params->numPulses = numPulses;
//	params->doneClb = doneClb;
//	params->h = h;
//
//	stepper_ctx.is_canceled = 0;
//	xTaskCreate(StepTask, "stepper", 10000, params, configMAX_PRIORITIES-5, &(stepper_ctx.step_task));

	TIM_HandleTypeDef* tim1_handle = (TIM_HandleTypeDef*)pPWM;



	HAL_TIM_OnePulse_Init(tim1_handle, TIM_CHANNEL_1);
	HAL_TIM_OnePulse_Stop_IT(tim1_handle, TIM_CHANNEL_1);
	__HAL_TIM_SET_AUTORELOAD(tim1_handle, 6969);
	HAL_TIM_GenerateEvent(tim1_handle, TIM_EVENTSOURCE_UPDATE);
	HAL_TIM_OnePulse_Start_IT(tim1_handle, TIM_CHANNEL_1);
	__HAL_TIM_ENABLE(tim1_handle);

	return 0;
}



static int StepTimerCancelAsync(void* pPWM)
{
	stepper_ctx.is_canceled = 1;
	return 0;
}

void init_stepper(ConsoleHandle_t console_handle, SPI_HandleTypeDef* hspi1, TIM_HandleTypeDef* tim1_handle, TIM_HandleTypeDef* tim4_handle){

	HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);
	HAL_TIM_PWM_Start_IT(tim4_handle, TIM_CHANNEL_4);


	// pass all function pointers required by the stepper library
	// to a separate platform abstraction structure

	p.malloc     = StepLibraryMalloc;
	p.free       = StepLibraryFree;
	p.transfer   = StepDriverSpiTransfer;
	p.reset      = StepDriverReset;
	p.sleep      = StepLibraryDelay;
	//p.step       = StepTimer;
	p.stepAsync  = StepAsyncTimer;
	p.cancelStep = StepTimerCancelAsync;


	// now create the handle
	stepper_ctx.h = L6474_CreateInstance(&p, hspi1, NULL, tim1_handle);
	//reset(h);


	CONSOLE_RegisterCommand(console_handle, "stepper", "Stepper main Command", stepperConsoleFunction, &stepper_ctx);
}

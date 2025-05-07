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
	int is_running;

	int position;

	void (*done_callback)(L6474_Handle_t);
	int remaining_pulses;
	TIM_HandleTypeDef* htim1_handle;
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

		status |= HAL_SPI_TransmitReceive(pIO, (uint8_t*)pTX + i, (uint8_t*)pRX + i, 1, 10000);

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
	(void) pGPO;
	HAL_GPIO_WritePin(STEP_RSTN_GPIO_Port, STEP_RSTN_Pin, !ena);

	return;
}

static void StepLibraryDelay()
{
	return;
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
	param.TorqueVal = 0x29;
	param.TFast = 0x19;

	int result = 0;

	result |= L6474_ResetStandBy(stepper_ctx->h);
	result |= L6474_Initialize(stepper_ctx->h, &param);

	result |= L6474_SetPowerOutputs(stepper_ctx->h, 0);


	stepper_ctx->is_powered = 0;
	stepper_ctx->is_referenced = 0;
	stepper_ctx->is_running = 0;

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
	int result = 0;
	int poweroutput = 0;
	uint32_t timeout_ms = 0;
	if (argc == 2) {
		if (strcmp(argv[1], "-s") == 0) {
			//Referenzfahrt überspringen
			stepper_ctx->is_referenced = 1;
			stepper_ctx->position = 0;
			return result;
		}
		else if (strcmp(argv[1], "-e") == 0) {
			// Power aktiv lassen
			poweroutput = 1;
		}
		else {
			printf("Invalid argument for reference\r\n");
			return -1;
		}
	}
	else if(argc == 3){
		if (strcmp(argv[1], "-t") == 0) {
			timeout_ms = atoi(argv[2]) * 1000;
		}
		else {
			printf("Invalid argument for reference\r\n");
			return -1;
		}
	}

	const uint32_t start_time = HAL_GetTick();
	result |= L6474_SetPowerOutputs(stepper_ctx->h, 1);
	if(HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET) {
		// already at reference
		L6474_StepIncremental(stepper_ctx->h, 100000000);
		while(HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) == GPIO_PIN_RESET){
			if (timeout_ms > 0 && HAL_GetTick() - start_time > timeout_ms) {
				StepTimerCancelAsync(NULL);
				printf("Timeout while waiting for reference switch\r\n");
				return -1;
			}
		}
		StepTimerCancelAsync(NULL);
	}
	L6474_StepIncremental(stepper_ctx->h, -1000000000);
	while(HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) != GPIO_PIN_RESET){
		if (timeout_ms > 0 && HAL_GetTick() - start_time > timeout_ms) {
			StepTimerCancelAsync(NULL);
			printf("Timeout while waiting for reference switch\r\n");
			return -1;
		}
	}
	StepTimerCancelAsync(NULL);

	stepper_ctx->is_referenced = 1;
	stepper_ctx->position = 0;
	result |= L6474_SetPowerOutputs(stepper_ctx->h, poweroutput);
	return result;

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

void start_tim1(int pulses) {
	int current_pulses = (pulses >= 65535) ? 65535 : pulses;
	stepper_ctx.remaining_pulses = pulses - current_pulses;

	if (current_pulses != 1) {
		HAL_TIM_OnePulse_Stop_IT(stepper_ctx.htim1_handle, TIM_CHANNEL_1);
		__HAL_TIM_SET_AUTORELOAD(stepper_ctx.htim1_handle, current_pulses);
		HAL_TIM_GenerateEvent(stepper_ctx.htim1_handle, TIM_EVENTSOURCE_UPDATE);
		HAL_TIM_OnePulse_Start_IT(stepper_ctx.htim1_handle, TIM_CHANNEL_1);
		__HAL_TIM_ENABLE(stepper_ctx.htim1_handle);
	}
	else {
		stepper_ctx.done_callback(stepper_ctx.h);
	}
}


void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef* htim) {
	if ((stepper_ctx.done_callback != 0) && ((htim->Instance->SR & (1 << 2)) == 0)) {
		if (stepper_ctx.remaining_pulses > 0) {
			start_tim1(stepper_ctx.remaining_pulses);
		}
		else {
			stepper_ctx.done_callback(stepper_ctx.h);
			stepper_ctx.is_running = 0;
		}
	}
}

static int StepAsyncTimer(void* pPWM, int dir, unsigned int numPulses, void (*doneClb)(L6474_Handle_t), L6474_Handle_t h) {
	(void)pPWM;
	(void)h;

	stepper_ctx.is_running = 1;

	HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, !!dir);

	stepper_ctx.done_callback = doneClb;

	start_tim1(numPulses);

	return 0;
}



static int StepTimerCancelAsync(void* pPWM)
{
	(void)pPWM;

	if (stepper_ctx.is_running) {
		HAL_TIM_OnePulse_Stop_IT(stepper_ctx.htim1_handle, TIM_CHANNEL_1);
		stepper_ctx.done_callback(stepper_ctx.h);
		stepper_ctx.is_referenced = 0;
	}

	return 0;
}

void init_stepper(ConsoleHandle_t console_handle, SPI_HandleTypeDef* hspi1, TIM_HandleTypeDef* tim1_handle, TIM_HandleTypeDef* tim4_handle){
	HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);
	tim4_handle->Instance->CCR4 = tim4_handle->Instance->ARR / 2;
	HAL_TIM_PWM_Start(tim4_handle, TIM_CHANNEL_4);

	p.malloc     = StepLibraryMalloc;
	p.free       = StepLibraryFree;
	p.transfer   = StepDriverSpiTransfer;
	p.reset      = StepDriverReset;
	p.sleep      = StepLibraryDelay;
	p.stepAsync  = StepAsyncTimer;
	p.cancelStep = StepTimerCancelAsync;

	stepper_ctx.h = L6474_CreateInstance(&p, hspi1, NULL, tim1_handle);
	stepper_ctx.htim1_handle = tim1_handle;

	CONSOLE_RegisterCommand(console_handle, "stepper", "Stepper main Command", stepperConsoleFunction, &stepper_ctx);
}

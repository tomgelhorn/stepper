#include "main.h"
#include "init.h"
#include "LibL6474.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "LibL6474Config.h"
#include "stm32f7xx_hal_spi.h"


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
		HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, 1);
		HAL_Delay(1);
		HAL_GPIO_WritePin(STEP_PULSE_GPIO_Port, STEP_PULSE_Pin, 0);
		HAL_Delay(1);
	}

}

static void StepTimerCancelAsync()
{
  return;
}

void teststepper(int argc, char** argv, void* ctx) {
	(void)argc;
	(void)argv;

	L6474_Handle_t h = *(L6474_Handle_t*)ctx;

	L6474_BaseParameter_t param;
	param.stepMode = smMICRO16;
	param.OcdTh = ocdth6000mA;
	param.TimeOnMin = 0x29;
	param.TimeOffMin = 0x29;
	param.TorqueVal = 0x11;
	param.TFast = 0x19;

	int result = 0;

	result |= L6474_ResetStandBy(h);

	result |= L6474_Initialize(h, &param);

	result |= L6474_SetPowerOutputs(h, 1);

	if ( result == 0 )
	{
	    result |= L6474_StepIncremental(h, 1000 );
	}
}

static int reset(L6474_Handle_t h){
	L6474_BaseParameter_t param;
	param.stepMode = smMICRO16;
	param.OcdTh = ocdth6000mA;
	param.TimeOnMin = 0x29;
	param.TimeOffMin = 0x29;
	param.TorqueVal = 0x11;
	param.TFast = 0x19;

	int result = 0;

	result |= L6474_ResetStandBy(h);

	result |= L6474_Initialize(h, &param);

	result |= L6474_SetPowerOutputs(h, 1);

	return result;
}

static int move(L6474_Handle_t h, int steps) {
	return L6474_StepIncremental(h, steps);
}

static int stepperConsoleFunction(int argc, char** argv, void* ctx) {
	L6474_Handle_t h = *(L6474_Handle_t*)ctx;
	int result = 0;

	if (argc == 0) {
		printf("Invalid number of arguments\r\n");
		return -1;
	}
	if (strcmp(argv[0], "move") == 0 )
	{
		move(h, atoi(argv[1]));
	}
	else if (strcmp(argv[0], "reset") == 0) {
		result = reset(h);
	}
	else {
		printf("Invalid command\r\n");
		return -1;
	}

	return result;
}

L6474x_Platform_t p;
L6474_Handle_t h;

void init_stepper(ConsoleHandle_t console_handle, SPI_HandleTypeDef* hspi1){

	HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);


	// pass all function pointers required by the stepper library
	// to a separate platform abstraction structure

	p.malloc     = StepLibraryMalloc;
	p.free       = StepLibraryFree;
	p.transfer   = StepDriverSpiTransfer;
	p.reset      = StepDriverReset;
	p.sleep      = StepLibraryDelay;
	p.step       = StepTimer;
	//p.cancelStep = StepTimerCancelAsync;


	// now create the handle
	h = L6474_CreateInstance(&p, hspi1, NULL, NULL);
	//reset(h);


	CONSOLE_RegisterCommand(console_handle, "stepper", "Stepper main Command", stepperConsoleFunction, &h);
}

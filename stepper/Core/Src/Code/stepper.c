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

#define STEPS_PER_TURN 200
#define RESOLUTION 16
#define MM_PER_TURN 4

typedef struct {
	L6474_Handle_t h;
	int is_powered;
	int is_referenced;
	int is_running;

	void (*done_callback)(L6474_Handle_t);
	int remaining_pulses;
	TIM_HandleTypeDef* htim1_handle;
	TIM_HandleTypeDef* htim4_handle;

	int steps_per_turn;
	int resolution;
	float mm_per_turn;

	int position_min_steps;
	int position_max_steps;
	int position_ref_steps;
} StepperContext;

static int StepTimerCancelAsync(void* pPWM);
void set_speed(StepperContext* stepper_ctx, int steps_per_second);

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
	param.TorqueVal = 0x11;
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
	int is_skip = 0;
	uint32_t timeout_ms = 0;

	for (int i = 1; i < argc; ) {
		// skip
		if (strcmp(argv[i], "-s") == 0) {
			is_skip = 1;
			i++;
		}

		// power
		else if (strcmp(argv[i], "-e") == 0) {
			poweroutput = 1;
			i++;
		}

		// speed
		else if (strcmp(argv[i], "-t") == 0) {
			if (i == argc - 1) {
				printf("Invalid number of arguments\r\n");
				return -1;
			}

			timeout_ms = atoi(argv[2]) * 1000;
			if (timeout_ms <= 0) {
				printf("Invalid timeout value\r\n");
				return -1;
			}
			i += 2;
		}

		else {
			printf("Invalid Flag\r\n");
			return -1;
		}
	}


	if (!is_skip) {
		const uint32_t start_time = HAL_GetTick();
		result |= L6474_SetPowerOutputs(stepper_ctx->h, 1);
		set_speed(stepper_ctx, 500);
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
		while(HAL_GPIO_ReadPin(REFERENCE_MARK_GPIO_Port, REFERENCE_MARK_Pin) != GPIO_PIN_RESET && result == 0) {
			if (timeout_ms > 0 && HAL_GetTick() - start_time > timeout_ms) {
				StepTimerCancelAsync(NULL);
				printf("Timeout while waiting for reference switch\r\n");
				result = -1;
			}
		}
		StepTimerCancelAsync(NULL);
	}

	if (result == 0) {
		stepper_ctx->is_referenced = 1;
		L6474_SetAbsolutePosition(stepper_ctx->h, stepper_ctx->position_ref_steps);
	}

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
	else if(strcmp(argv[1], "torque") == 0){
		if (argc == 2) {
			int value = 0;
			L6474_GetProperty(stepper_ctx->h, L6474_PROP_TORQUE, &value);
			printf("%d\r\n", value);
			return 0;
		}
		else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
			return L6474_SetProperty(stepper_ctx->h, L6474_PROP_TORQUE, atoi(argv[3]));
		}
		else {
			printf("Invalid number of arguments\r\n");
			return -1;
		}
	}
	else if(strcmp(argv[1], "timeon") == 0){
		if(stepper_ctx->is_powered == 1){
			return -1;
		}
		if (argc == 2) {
			int value = 0;
			L6474_GetProperty(stepper_ctx->h, L6474_PROP_TON, &value);
			printf("%d\r\n", value);
			return 0;
		}
		else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
			return L6474_SetProperty(stepper_ctx->h, L6474_PROP_TON, atoi(argv[3]));
		}
		else {
			printf("Invalid number of arguments\r\n");
			return -1;
		}
	}
	else if(strcmp(argv[1], "timeoff") == 0){
		if(stepper_ctx->is_powered == 1){
			return -1;
		}
		if (argc == 2) {
			int value = 0;
			L6474_GetProperty(stepper_ctx->h, L6474_PROP_TOFF, &value);
			printf("%d\r\n", value);
			return 0;
		}
		else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
			return L6474_SetProperty(stepper_ctx->h, L6474_PROP_TOFF, atoi(argv[3]));
		}
		else {
			printf("Invalid number of arguments\r\n");
			return -1;
		}
	}
	else if(strcmp(argv[1], "timefast") == 0){
		if(stepper_ctx->is_powered == 1){
			return -1;
		}
		if (argc == 2) {
			int value = 0;
			L6474_GetProperty(stepper_ctx->h, L6474_PROP_TFAST, &value);
			printf("%d\r\n", value);
			return 0;
		}
		else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
			return L6474_SetProperty(stepper_ctx->h, L6474_PROP_TFAST, atoi(argv[3]));
		}
		else {
			printf("Invalid number of arguments\r\n");
			return -1;
		}
	}
	else if(strcmp(argv[1], "throvercurr") == 0){
		if (argc == 2) {
			int value = 0;
			L6474_GetProperty(stepper_ctx->h, L6474_PROP_OCDTH, &value);
			printf("%d\r\n", value);
			return 0;
		}
		else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
			return L6474_SetProperty(stepper_ctx->h, L6474_PROP_OCDTH, atoi(argv[3]));
		}
		else {
			printf("Invalid number of arguments\r\n");
			return -1;
		}
	}
	else if(strcmp(argv[1], "stepmode") == 0){
		if(stepper_ctx->is_powered == 1){
			return -1;
		}
		if (argc == 2) {
			printf("%d\r\n", stepper_ctx->resolution);
			return 0;
		}
		else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
			int resolution = atoi(argv[3]);

			L6474x_StepMode_t step_mode;

			switch (resolution) {
				case 1:
					step_mode = smFULL;
					break;
				case 2:
					step_mode = smHALF;
					break;
				case 4:
					step_mode = smMICRO4;
					break;
				case 8:
					step_mode = smMICRO8;
					break;
				case 16:
					step_mode = smMICRO16;
					break;
				default:
					printf("Invalid step mode\r\n");
					return -1;
			}
			stepper_ctx->resolution = resolution;
			return L6474_SetStepMode(stepper_ctx->h, step_mode);
		}
		else {
			printf("Invalid number of arguments\r\n");
			return -1;
		}
	}
	else if(strcmp(argv[1], "stepsperturn") == 0){
		if (argc == 2) {
			printf("%d\r\n", stepper_ctx->steps_per_turn);
			return 0;
		}
		else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
			stepper_ctx->steps_per_turn = atoi(argv[3]);
			return 0;
		}
		else {
			printf("Invalid number of arguments\r\n");
			return -1;
		}
	}
	else if(strcmp(argv[1], "mmperturn") == 0){
		if (argc == 2) {
			printf("%f\r\n", stepper_ctx->mm_per_turn);
			return 0;
		}
		else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
			stepper_ctx->mm_per_turn = atoff(argv[3]);
			return 0;
		}
		else {
			printf("Invalid number of arguments\r\n");
			return -1;
		}
	}
	else if(strcmp(argv[1], "posmin") == 0){
		if (argc == 2) {
			printf("%f\r\n", (float)(stepper_ctx->position_min_steps * stepper_ctx->mm_per_turn) / (float)(stepper_ctx->steps_per_turn  * stepper_ctx->resolution));
			return 0;
		}
		else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
			float value_float = atoff(argv[3]);


			stepper_ctx->position_min_steps = (value_float * stepper_ctx->steps_per_turn  * stepper_ctx->resolution) / stepper_ctx->mm_per_turn;;
			return 0;
		}
		else {
			printf("Invalid number of arguments\r\n");
			return -1;
		}
	}
	else if(strcmp(argv[1], "posmax") == 0){
		if (argc == 2) {
			printf("%f\r\n", (float)(stepper_ctx->position_max_steps * stepper_ctx->mm_per_turn) / (float)(stepper_ctx->steps_per_turn  * stepper_ctx->resolution));
			return 0;
		}
		else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
			float value_float = atoff(argv[3]);


			stepper_ctx->position_max_steps = (value_float * stepper_ctx->steps_per_turn  * stepper_ctx->resolution) / stepper_ctx->mm_per_turn;;
			return 0;
		}
		else {
			printf("Invalid number of arguments\r\n");
			return -1;
		}
	}
	else if(strcmp(argv[1], "posref") == 0){
		if (argc == 2) {
			printf("%f\r\n", (float)(stepper_ctx->position_ref_steps * stepper_ctx->mm_per_turn) / (float)(stepper_ctx->steps_per_turn  * stepper_ctx->resolution));
			return 0;
		}
		else if (argc == 4 && strcmp(argv[2], "-v") == 0) {
			float value_float = atoff(argv[3]);


			stepper_ctx->position_ref_steps = (value_float * stepper_ctx->steps_per_turn  * stepper_ctx->resolution) / stepper_ctx->mm_per_turn;;
			return 0;
		}
		else {
			printf("Invalid number of arguments\r\n");
			return -1;
		}
	}
	else {
		printf("Invalid command\r\n");
		return -1;
	}
}
void set_speed(StepperContext* stepper_ctx, int steps_per_second) {
	int clk = HAL_RCC_GetHCLKFreq();

	int quotient = clk / (steps_per_second * 2); // magic 2

	int i = 0;

	while ((quotient / (i + 1)) > 65535) i++;

	__HAL_TIM_SET_PRESCALER(stepper_ctx->htim4_handle, i);
	__HAL_TIM_SET_AUTORELOAD(stepper_ctx->htim4_handle, (quotient / (i + 1)) - 1);
	stepper_ctx->htim4_handle->Instance->CCR4 = stepper_ctx->htim4_handle->Instance->ARR / 2;
}

static int move(StepperContext* stepper_ctx, int argc, char** argv) {
	if (stepper_ctx->is_powered != 1) {
		printf("Stepper not powered\r\n");
		return -1;
	}
	if (stepper_ctx->is_referenced != 1) {
		printf("Stepper not referenced\r\n");
		return -1;
	}
	if (argc < 2) {
		printf("Invalid number of arguments\r\n");
		return -1;
	}

	int position = atoi(argv[1]);
	int speed = 1000;

	int is_async = 0;
	int is_relative = 0;

	for (int i = 2; i < argc; ) {
		// async
		if (strcmp(argv[i], "-a") == 0) {
			is_async = 1;
			i++;
		}

		// relative
		else if (strcmp(argv[i], "-r") == 0) {
			is_relative = 1;
			i++;
		}

		// speed
		else if (strcmp(argv[i], "-s") == 0) {
			if (i == argc - 1) {
				printf("Invalid number of arguments\r\n");
				return -1;
			}

			speed = atoi(argv[i + 1]);
			i += 2;
		}

		else {
			printf("Invalid Flag\r\n");
			return -1;
		}
	}

	int steps_per_second = (speed * stepper_ctx->steps_per_turn * stepper_ctx->resolution) / (60 * stepper_ctx->mm_per_turn);

	if (steps_per_second < 1) {
		printf("Speed too small\r\n");
		return -1;
	}

	set_speed(stepper_ctx, steps_per_second);

	int steps = (position * stepper_ctx->steps_per_turn  * stepper_ctx->resolution) / stepper_ctx->mm_per_turn;

	if (!is_relative) {
		int absolute_position;
		L6474_GetAbsolutePosition(stepper_ctx->h, &absolute_position);

		steps -= absolute_position;
	}

	if (steps == 0 || steps == -1 || steps == 1) {
		printf("No movement\r\n");
		return -1;
	}

	int resulting_steps;
	L6474_GetAbsolutePosition(stepper_ctx->h, &resulting_steps);
	resulting_steps += steps;

	if (resulting_steps < stepper_ctx->position_min_steps || resulting_steps > stepper_ctx->position_max_steps) {
		printf("Position out of bounds\r\n");
		return -1;
	}

	if (is_async) {
		return L6474_StepIncremental(stepper_ctx->h, steps);
	}
	else {
		int result = L6474_StepIncremental(stepper_ctx->h, steps);

		while (stepper_ctx->is_running);

		return result;
	}
}

static int initialize(StepperContext* stepper_ctx) {
	reset(stepper_ctx);
	stepper_ctx->is_powered = 1;
	stepper_ctx->is_referenced = 1;

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
		result = move(stepper_ctx, argc, argv);
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
	else if (strcmp(argv[0], "position") == 0){
		int position;
		L6474_GetAbsolutePosition(stepper_ctx->h, &position);
		printf("%.4f\r\n", (float)(position * stepper_ctx->mm_per_turn) / (float)(stepper_ctx->steps_per_turn  * stepper_ctx->resolution));
	}
	else if (strcmp(argv[0], "status") == 0){
		int status;
		if (stepper_ctx->is_powered == 0 && stepper_ctx->is_referenced == 0) {
			status = 0x0;
		}
		else if(stepper_ctx->is_powered == 1 && stepper_ctx->is_referenced == 0) {
			status = 0x1;
		}
		else if(stepper_ctx->is_powered == 0 && stepper_ctx->is_referenced == 1) {
			status = 0x2;
		}
		else {
			status = 0x4;
		}
		L6474_Status_t status_struct;
		L6474_GetStatus(stepper_ctx->h, &status_struct);
		if (status_struct.NOTPERF_CMD != 0 || status_struct.OCD != 0 || status_struct.TH_SD != 0|| status_struct.TH_WARN != 0 || status_struct.UVLO != 0 || status_struct.WRONG_CMD != 0) {
			status = 0x8;
		}
		int out_status = 0;
		out_status |= (status_struct.DIR << 0);
		out_status |= (status_struct.HIGHZ << 1);
		out_status |= (status_struct.NOTPERF_CMD << 2);
		out_status |= (status_struct.OCD << 3);
		out_status |= (status_struct.ONGOING << 4);
		out_status |= (status_struct.TH_SD << 5);
		out_status |= (status_struct.TH_WARN << 6);
		out_status |= (status_struct.UVLO << 7);
		out_status |= (status_struct.WRONG_CMD << 8);

		printf("0x%x\r\n0x%x\r\n%d\r\n", status, out_status, stepper_ctx->is_running);
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
	stepper_ctx.done_callback = doneClb;

	HAL_GPIO_WritePin(STEP_DIR_GPIO_Port, STEP_DIR_Pin, !!dir);



	start_tim1(numPulses);

	return 0;
}



static int StepTimerCancelAsync(void* pPWM)
{
	(void)pPWM;

	if (stepper_ctx.is_running) {
		HAL_TIM_OnePulse_Stop_IT(stepper_ctx.htim1_handle, TIM_CHANNEL_1);
		stepper_ctx.done_callback(stepper_ctx.h);
	}

	return 0;
}

void init_stepper(ConsoleHandle_t console_handle, SPI_HandleTypeDef* hspi1, TIM_HandleTypeDef* tim1_handle, TIM_HandleTypeDef* tim4_handle){
	HAL_GPIO_WritePin(STEP_SPI_CS_GPIO_Port, STEP_SPI_CS_Pin, 1);
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
	stepper_ctx.htim4_handle = tim4_handle;

	stepper_ctx.steps_per_turn = STEPS_PER_TURN;
	stepper_ctx.resolution = RESOLUTION;
	stepper_ctx.mm_per_turn = MM_PER_TURN;

	stepper_ctx.position_min_steps = 0;
	stepper_ctx.position_max_steps = 100000;
	stepper_ctx.position_ref_steps = 0;

	CONSOLE_RegisterCommand(console_handle, "stepper", "Stepper main Command", stepperConsoleFunction, &stepper_ctx);
}


// --------------------------------------------------------------------------------------------------------------------
#include <windows.h>
#include <stdio.h>

// --------------------------------------------------------------------------------------------------------------------
#include "stm32f7xx_hal.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

// --------------------------------------------------------------------------------------------------------------------


// --------------------------------------------------------------------------------------------------------------------
SPI_HandleTypeDef hspi1 = { .Instance = SPI1, .ErrorCode = 0, .Lock = HAL_UNLOCKED, .hdmarx = 0, .hdmatx = 0 };
TIM_HandleTypeDef htim1 = { .Instance = TIM1, .hdma = 0, .Lock = HAL_UNLOCKED, .Channel = 0 };
TIM_HandleTypeDef htim2 = { .Instance = TIM2, .hdma = 0, .Lock = HAL_UNLOCKED, .Channel = 0 };
TIM_HandleTypeDef htim4 = { .Instance = TIM4, .hdma = 0, .Lock = HAL_UNLOCKED, .Channel = 0 };

// --------------------------------------------------------------------------------------------------------------------
void* pvPortMalloc(size_t xSize)
// --------------------------------------------------------------------------------------------------------------------
{
    void* p = malloc(xSize);
    return p;
}

// --------------------------------------------------------------------------------------------------------------------
void vPortFree(void* pv)
// --------------------------------------------------------------------------------------------------------------------
{
    free(pv);
}

// --------------------------------------------------------------------------------------------------------------------
size_t xPortGetFreeHeapSize(void)
// --------------------------------------------------------------------------------------------------------------------
{
    return 0xFFFFFFFF;
}

// --------------------------------------------------------------------------------------------------------------------
size_t xPortGetMinimumEverFreeHeapSize(void)
// --------------------------------------------------------------------------------------------------------------------
{
    return 0xFFFFFFFF;
}

//! No implementation needed, but stub provided in case application already calls vPortInitialiseBlocks
// --------------------------------------------------------------------------------------------------------------------
void vPortInitialiseBlocks(void)
// --------------------------------------------------------------------------------------------------------------------
{
    return;
}

// --------------------------------------------------------------------------------------------------------------------
void vAssertCalled(const char* const pcFileName, unsigned long ulLine)
// --------------------------------------------------------------------------------------------------------------------
{
    volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;

    /* Parameters are not used. */
    (void)ulLine;
    (void)pcFileName;

    taskENTER_CRITICAL();
    {
        /* You can step out of this function to debug the assertion by using
        the debugger to set ulSetToNonZeroInDebuggerToContinue to a non-zero
        value. */
        while (ulSetToNonZeroInDebuggerToContinue == 0)
        {
        }
    }
    taskEXIT_CRITICAL();
}

// --------------------------------------------------------------------------------------------------------------------
void vApplicationMallocFailedHook(void)
// --------------------------------------------------------------------------------------------------------------------
{
    taskDISABLE_INTERRUPTS();
    for (;;) { ; }
}

// --------------------------------------------------------------------------------------------------------------------
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char* pcTaskName)
// --------------------------------------------------------------------------------------------------------------------
{
    (void)pcTaskName;
    (void)pxTask;

    taskDISABLE_INTERRUPTS();
    for (;;) { ; }
}

// --------------------------------------------------------------------------------------------------------------------
static int CapabilityFunc(int argc, char** argv, void* ctx)
// --------------------------------------------------------------------------------------------------------------------
{
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
        0 // has stepper cancel
    );
    return 0;
}


// remove this in case it is implemented by the Gated Mode Master Slave Timers in your Controller Logic!
// --------------------------------------------------------------------------------------------------------------------
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef* htim)
// --------------------------------------------------------------------------------------------------------------------
{
    (void)htim;
}


// --------------------------------------------------------------------------------------------------------------------
int main( int argc, char** argv )
// --------------------------------------------------------------------------------------------------------------------
{
    printf("Hello World\r\n");
    vTaskStartScheduler();

    return -1;
}
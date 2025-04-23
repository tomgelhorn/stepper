/*
 * init.h
 *
 *  Created on: Mar 19, 2025
 *      Author: es23018
 */

#ifndef INC_CODE_INIT_H_
#define INC_CODE_INIT_H_

#include "main.h"
#include "Console.h"

void init(TIM_HandleTypeDef tim2_handle, SPI_HandleTypeDef* hspi1, TIM_HandleTypeDef* tim1_handle, TIM_HandleTypeDef* tim4_handle);
void init_spindle(ConsoleHandle_t console_handle, TIM_HandleTypeDef tim_handle);
void init_stepper(ConsoleHandle_t console_handle, SPI_HandleTypeDef* hspi1, TIM_HandleTypeDef* tim1_handle, TIM_HandleTypeDef* tim4_handle);
#endif /* INC_CODE_INIT_H_ */

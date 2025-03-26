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

void init(TIM_HandleTypeDef tim_handle);
void init_spindle(ConsoleHandle_t console_handle, TIM_HandleTypeDef tim_handle);

#endif /* INC_CODE_INIT_H_ */

#ifndef RCC_CONF_H
#define RCC_CONF_H

#include "stm32f411xe.h"

typedef enum {
	SUCCESS,
	HSI_ERROR,
	HSE_NOT_READY,
	PLL_ERROR,
	CSS_HSE_NOT_STABLE,
	FLASH_LATENCY_NOT_CHANGED,
	SW_PLL_NOT_SET
}RCC_Result;

RCC_Result RCC_Clock_to_100MHz(void);

#endif

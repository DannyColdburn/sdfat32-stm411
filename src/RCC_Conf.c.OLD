#include "RCC_Conf.h"

RCC_Result RCC_Clock_to_100MHz(void){
	uint8_t Err_Cycles = UINT8_MAX;
	
	//Check if HSI Ready
	while(!(RCC->CR & RCC_CR_HSIRDY)){
		Err_Cycles--;
		if(!Err_Cycles) return HSI_ERROR;
	}
	Err_Cycles = UINT8_MAX;

	//Set High Speed External enable
	RCC->CR	|= RCC_CR_HSEON;
	while(!(RCC->CR & RCC_CR_HSERDY)){
		Err_Cycles--;
		if(!Err_Cycles) return HSE_NOT_READY;
	}
	Err_Cycles = UINT8_MAX;
	
	//Begin to configure prescalers for PLL
	//Config PLL MAIN Division
	//It is recommended to not exceed 2MHz to limit PLL jitter
	//We use 25MHz external resonator.
	//HSE => PLLM
	RCC->PLLCFGR  |= 1 << 22; //Select HSE as input for PLL
	RCC->PLLCFGR	&= ~(0x3Fu);
	RCC->PLLCFGR 	|= 25u;			//25MHz HSE divided by 25 = 1MHz VCO input
	
	RCC->PLLCFGR 	&= ~(0x1FFu << 6);
	RCC->PLLCFGR 	|= 200u << 6;			// 1MHz * 200 = 200MHz VCO multiplication
	
	RCC->PLLCFGR 	&= ~(0x3u << 16); //Clear it just in case to get Division Factor 2 for main system clock
	//PLLQ is used for USB and it must not exceed 48MHz
	//Also APB1 is clamped to 50MHz Max, so we must take care of APB1 Prescaler
	RCC->CFGR		|= 0x4u << 10;  //Division Factor is 2; 100 / 2 = 50MHz
	while(Err_Cycles--);				//Wait for APB1 division to take effect
	Err_Cycles = UINT8_MAX;
	
	//Enable Clock Secutity
	RCC->CR 	|= RCC_CR_CSSON;
	while(Err_Cycles--);
	if(!(RCC->CR & RCC_CR_CSSON)) return CSS_HSE_NOT_STABLE;
	Err_Cycles = UINT8_MAX;
	
	//Enable PLL
	RCC->CR 	|= RCC_CR_PLLON;
	while(!(RCC->CR & RCC_CR_PLLRDY)){
		if(!Err_Cycles--) return PLL_ERROR;
	}
	Err_Cycles = UINT8_MAX;
	
	//Set FLASH latency to match with system clock. In case of 2.7v - 3.6v and 100MHz it is 3 wait states
	//Write Latency to ACR
	//Read Latency to check if it is taken into account
	//Change system clock source
	//Check system clock source (SWS bits)
	FLASH->ACR 	|= FLASH_ACR_LATENCY_3WS;
	
	while(Err_Cycles--);
	if((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != FLASH_ACR_LATENCY_3WS) return FLASH_LATENCY_NOT_CHANGED;
	Err_Cycles = UINT8_MAX;
	
	//Set system clock source to PLL
	RCC->CFGR |= RCC_CFGR_SW_PLL;
	while(Err_Cycles--);
	if((RCC->CFGR & RCC_CFGR_SWS_PLL) != RCC_CFGR_SWS_PLL) return SW_PLL_NOT_SET;
	
	return SUCCESS;	
}

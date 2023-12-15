#include "DL_USART_F411.h"

static void _configureGPIO();
static void _configureUSART();

void USART_Init(){
    _configureGPIO();
    _configureUSART();
}
void USART_Send(uint8_t *data){
    uint8_t *s = data;
    while(*s){
        while(!(USART1->SR & USART_SR_TXE));
        USART1->DR = *s;
        s++;
    }
    while(!(USART1->SR & USART_SR_TXE));
    USART1->DR = '\n';
}


static void _configureGPIO(){
    //USART AF - 7
    //PA 9 - TX
    //PA 10 - RX
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA->MODER |= 0xA << 18;
    GPIOA->AFR[1] |= 0x77 << 4;
}

static void _configureUSART(){
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    USART1->BRR = 868; // 1666 10416
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE;
    USART1->CR1 |= USART_CR1_UE;
}



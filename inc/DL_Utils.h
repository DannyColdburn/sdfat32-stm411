#ifndef DL_UTILS_H
#define DL_UTILS_H

#include "stm32f411xe.h"

#define BIT_CLR(x, y) x &= ~(y)
#define BIT_SET(x, y) x |= y

#define GPIO_MODER_INPUT    0x0
#define GPIO_MODER_OUTPUT   0x1
#define GPIO_MODER_ALT_FUN  0x2
#define GPIO_MODER_ANALOG   0x3

#define GPIO_TYPER_PUSHPULL     0x0     //TODO: Rename this already
#define GPIO_TYPER_OPENDRAIN    0x1

#define GPIO_OSPEEDR_LOW        0x0   
#define GPIO_OSPEEDR_MEDIUM     0x1
#define GPIO_OSPEEDR_FAST       0x2
#define GPIO_OSPEEDR_HIGH       0x3

#define GPIO_PUPDR_FLOAT        0x0
#define GPIO_PUPDR_PULLUP       0x1
#define GPIO_PUPDR_PULLDOWN     0x2

#define GPIO_AF0_SYS            0x0
#define GPIO_AF1_TIM1_TIM2      0x1
#define GPIO_AF2_TIM3_5         0x2
#define GPIO_AF3_TIM9_11        0x3
#define GPIO_AF4_I2C1_3         0x4
#define GPIO_AF5_SPI1_4         0x5
#define GPIO_AF6_SPI3_5         0x6
#define GPIO_AF7_USART1_2       0x7
#define GPIO_AF8_USART6         0x8
#define GPIO_AF9_I2C2_3         0x9
#define GPIO_AF10_OTG_FS        0xA
#define GPIO_AF11               0xB
#define GPIO_AF12_SDIO          0xC
#define GPIO_AF13               0xD
#define GPIO_AF14               0xE
#define GPIO_AF15_EVENTOUT      0xF


typedef struct{
    uint8_t MODER : 2;
    uint8_t OTYPER : 1;
    uint8_t OSPEEDR : 2;
    uint8_t PUPDR : 2;
    uint8_t AFR : 4;
}GPIOconf_t;

void DL_GPIO_Config(GPIO_TypeDef *port, uint8_t pin, GPIOconf_t *conf);
void DL_delay_ticks(uint32_t ticks);

#endif

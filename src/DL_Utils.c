#include "DL_Utils.h"

void DL_GPIO_Config(GPIO_TypeDef *port, uint8_t pin, GPIOconf_t *conf){
    port->MODER     &= ~(0x3 << (pin * 2));
    port->OTYPER    &= ~(0x1 << pin);
    port->OSPEEDR   &= ~(0x3 << (pin * 2));
    port->PUPDR     &= ~(0x3 << (pin * 2));
    port->AFR[0]    %= ~(0xF << (pin *4));

    port->MODER     |= conf->MODER << (pin * 2);
    port->OTYPER    |= conf->OTYPER << (pin);
    port->OSPEEDR   |= conf->OSPEEDR << (pin * 2);
    port->PUPDR     |= conf->PUPDR << (pin * 2);
    port->AFR[0]    |= conf->AFR << (pin * 4);
}

void DL_delay_ticks(uint32_t ticks){
    while(ticks--);
}
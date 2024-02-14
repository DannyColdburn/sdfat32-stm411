#include "DL_SPI.h"
#include "DL_Utils.h"

//TODO: REWRITE THIS. SHOULD BE TWO OPERATIONS: FIRST ONE IS LIKE INTERFACE FOR OTHER LIBS, SECOND ONE IS STANDALONE APP

/// @brief Inits SPI with given parameters
/// @param spi Pointer to SPI typedef
/// @param conf Struct with parameters
void DL_SPI_Init(SPI_TypeDef *spi, SPI_InitStruct *conf){
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    {
        DL_GPIO_Config_t miso_conf = {
            .MODER = GPIO_MODER_ALT_FUN,
            .AFR = GPIO_AF5_SPI1_4,
            .OSPEEDR = GPIO_OSPEEDR_HIGH,
            .OTYPER = GPIO_OTYPER_PUSHPULL,
            .PUPDR = GPIO_PUPDR_PULLUP,
        };

        DL_GPIO_Config_t mosi_conf = {
            .MODER = GPIO_MODER_ALT_FUN,
            .AFR = GPIO_AF5_SPI1_4,
            .OSPEEDR = GPIO_OSPEEDR_HIGH,
            .OTYPER = GPIO_OTYPER_PUSHPULL,
            .PUPDR = GPIO_PUPDR_PULLDOWN
        };

        DL_GPIO_Config_t sclk_conf = {
            .MODER = GPIO_MODER_ALT_FUN,
            .AFR = GPIO_AF5_SPI1_4,
            .OSPEEDR = GPIO_OSPEEDR_HIGH,
            .OTYPER = GPIO_OTYPER_PUSHPULL,
            .PUPDR = GPIO_PUPDR_PULLUP,
        };

        DL_GPIO_Config_t slvs_conf = {
            .MODER = GPIO_MODER_OUTPUT,
            .OSPEEDR = GPIO_OSPEEDR_HIGH,
            .OTYPER = GPIO_OTYPER_PUSHPULL,
            .PUPDR = GPIO_PUPDR_PULLUP,
            .AFR = GPIO_AF0_SYS,
        };
        DL_GPIO_Config(GPIOA, 7, &mosi_conf);
        DL_GPIO_Config(GPIOA, 6, &miso_conf);
        DL_GPIO_Config(GPIOA, 5, &sclk_conf);
        DL_GPIO_Config(GPIOA, 4, &slvs_conf);
    }


    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;    
    spi->CR1 = 0;
    spi->CR2 = 0;
    
    uint16_t *settings = (uint16_t *) conf;
    spi->CR1 |= *settings;
    settings++;
    spi->CR2 = *settings;

    spi->CR1 |= SPI_CR1_SPE;
}



void DL_SPI_Send(SPI_TypeDef *spi, uint8_t *data, uint32_t length){
    if(length){
        for(uint32_t i = 0; i < length; i++){
            DL_SPI_sendByte(spi, data[i]);
        }
    } else {
        for(uint8_t *c = data; *c; c++){
            DL_SPI_sendByte(spi, *c);
        }
    }
    while(spi->SR & SPI_SR_BSY);
    DL_delay_ticks(100);
}

void DL_SPI_Recv(SPI_TypeDef *spi, uint8_t *buffer, uint32_t length){
    spi->DR;
    for(int i = 0; i < length; i++){
        DL_SPI_sendByte(spi, 0x00);
        while(!(spi->SR & SPI_SR_RXNE));
        buffer[i] = spi->DR;
        while(SPI1->SR & SPI_SR_BSY);
    }
    DL_delay_ticks(50);
}



void DL_SPI_sendByte(SPI_TypeDef *spi, uint8_t byte){
    while(!(spi->SR & SPI_SR_TXE));
    spi->DR = byte;
    while(!(spi->SR & SPI_SR_RXNE));
    spi->DR;
}

uint8_t DL_SPI_readByte(SPI_TypeDef *spi){
    while(!(spi->SR & SPI_SR_TXE));
    spi->DR = 0;
    while(!(spi->SR & SPI_SR_RXNE));
    return spi->DR;
    //TODO: Maybe it should content some error timer when there is no RXNE raised
}
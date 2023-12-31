#include "main.h"
#include "DL_Debug.h"
#include "malloc.h"

void Init_Periphery();

SDCardInfo_t SDCard;
SDCardFile_t *file;
uint8_t tryCount = 10;

int main(){
    Init_Periphery();

    DBG("\r\n\r\n--- Program started ---\n");

    DL_SDCARD_Init(SPI1, GPIOA, 4);
    while(!DL_SDCARD_Mount(&SDCard)) {
        DBG("Mount failed, try again...");
        DL_delay_ticks(10000000);
        DL_SDCARD_Init(SPI1, GPIOA, 4);
        if(tryCount--) {
            DBG("SDCard refuses to start");
            goto skip;
        }
    }

    file = DL_SDCARD_Open(&SDCard, "5.txt", FILE_WRITE);
    if (!file) goto skip;

    for (int i = 0; i < 100; i++) {
        uint8_t data[64] = {0};
        sprintf((char *) data, "Writing %i data with %i\r\n", i, i);
        if (!DL_SDCard_WriteString(&SDCard, file, data)) goto skip;
    }

    

    skip:
    DBG("--- Program ended ---\n");
    while(1){
        
    }
}

void Init_Periphery(){
    RCC_Clock_to_100MHz();
    USART_Init();

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    GPIOconf_t csConf = {
        .MODER = GPIO_MODER_OUTPUT,
        .OSPEEDR = GPIO_OSPEEDR_HIGH,
        .OTYPER = GPIO_TYPER_PUSHPULL,
        .PUPDR = GPIO_PUPDR_PULLDOWN,
    };

    DL_GPIO_Config(GPIOA, 4, &csConf);
    
    SPI_InitStruct spiConf = {
        .MSTR = 1,
        .SSI = 1,
        .SSM = 1,
        .BR = 0x7,
    };

    DL_SPI_Init(SPI1, &spiConf);
}
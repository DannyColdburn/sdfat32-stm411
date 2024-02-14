#include "main.h"
#include "DL_Debug.h"
#include "malloc.h"

void Init_Periphery(void);

SDCardInfo_t SDCard;
SDCardFile_t *file;
uint8_t tryCount = 10;
uint8_t buff[512];
uint8_t data[1024];

int main(void){
    RCC_SetSysClock(RCC_100MHz);
    RCC_Result RES = RCC_GetLastError();

    uint32_t MHz = RCC_GetSysClock();
    
    // RCC_Clock_to_100MHz();
    Init_Periphery();
    DBGF("Current clock: %i", MHz);
    DBG("\r\n\r\n--- Program started ---\n");
    DBG("Wait ~1ms before start SDCard");
    DL_delay_ticks(1000000);

    if (!DL_SDCARD_Init(SPI1, GPIOA, 4)) goto skip;
    // DL_SDCARD_Read(0x0, buff);

    if (!DL_SDCARD_Mount(&SDCard)) goto skip;

    file = DL_SDCARD_Open(&SDCard, "4.txt", FILE_WRITE);
    if (!file) goto skip;

    DL_SDCard_WriteString(&SDCard, file, "THIS IS NOT A DRILL, I REPEAT, THIS IS NOT A DRILL, PROGRAM WORKS WITHOUT A DEBUG, DAUUM, DA HELL IS DAT? WHO MADE DIS\r\n");
    DL_SDCard_WriteString(&SDCard, file, "GET DA HELL OUTA DIS PLACE, DIIS SO FOCKEN CURSD, SO BLOODY DIABOLICAL. TO ALL STATION, PROCEED TO EXTRACTION POINZ\r\n");

    // for (int j = 0; j < 10; j++){
    //     for(int i = 0; i < 512; i ++) {
    //         uint8_t strg[256];
    //         memset(strg, 0, 256);
    //         sprintf(strg, "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEe, WRITING STRING %i\r\n", i);
    //         if (!DL_SDCard_WriteString(&SDCard, file, strg)) goto skip;
    //     }

    //     file->readPosition = file->fileSize - (191 * 3);
    //     if (!DL_SDCard_FileRead(&SDCard, file, data, 191 * 3 + 10)) goto skip;
    // }
    file->readPosition = file->fileSize - (191 * 3);
    if (!DL_SDCard_FileRead(&SDCard, file, data, 191 * 3 + 10)) goto skip;
    DBGC(data, 1024);

    skip:
    DBG("--- Program ended ---\n");
    while(1){
        
    }
}

void Init_Periphery(){
    USART_Init();

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    DL_GPIO_Config_t csConf = {
        .MODER = GPIO_MODER_OUTPUT,
        .OSPEEDR = GPIO_OSPEEDR_HIGH,
        .OTYPER = GPIO_OTYPER_PUSHPULL,
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

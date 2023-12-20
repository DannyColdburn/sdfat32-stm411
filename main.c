#include "main.h"
#include "DL_Debug.h"
#include "malloc.h"

void Init_Periphery();

int main(){
    Init_Periphery();

    DBG("\r\n\r\n--- Program started ---\n");
    if (!DL_SDCARD_Init(SPI1, GPIOA, 4)) goto skip;
    DBG("- Mounting SD Card -\n");
    SDCardInfo_t CardInfo = {0};
    if (!DL_SDCARD_Mount(&CardInfo)) goto skip;


    // CHECK READING OPERATION
    SDCardFile_t *file;
    // file = DL_SDCARD_Open(&CardInfo, "ABCD.txt", FILE_READ);
    // if(!file) {
    //     DBG("File ABCD.txt not found\n");
    //     goto skip;
    // }

    // uint8_t content[2048] = {0};
    // while (DL_SDCard_FileRead(&CardInfo, file, content, 129)) {
    //     //DBGF("%s\n", content);
    //     USART_Send(content);
    //     DL_delay_ticks(5000000);
    // }
    // free(file);

    file = DL_SDCARD_Open(&CardInfo, "myfile.txt", FILE_WRITE);
    if(!file){
        DBG("File myfile.txt not found!\n");
        goto skip;
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
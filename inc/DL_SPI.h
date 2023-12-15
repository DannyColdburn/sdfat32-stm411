#ifndef DL_SPI_H
#define DL_SPI_H

#include "stm32f411xe.h"
#include "main.h"

typedef struct{
    uint8_t CPHA            : 1;
    uint8_t CPOL            : 1;
    uint8_t MSTR            : 1;
    uint8_t BR              : 3;
    uint8_t _reserved1_     : 1;
    uint8_t LBSFirst        : 1;
    uint8_t SSI             : 1;
    uint8_t SSM             : 1;
    uint8_t RX_Only         : 1;
    uint8_t DFF             : 1;
    uint8_t CRCNEXT         : 1;
    uint8_t CRCEN           : 1;
    uint8_t BIDIOE          : 1;
    uint8_t BIDIMODE        : 1;

    uint8_t RXDMAEN         : 1;
    uint8_t TXDMAEN         : 1;
    uint8_t SSOE            : 1;
    uint8_t _reserved3_     : 1;
    uint8_t FRF             : 1;
    uint8_t ERRIE           : 1;
    uint8_t RXNEIE          : 1;
    uint8_t TXEIE           : 1;
    uint8_t _reserved2_     : 8;
}SPI_InitStruct;


//* @brief Initializes SPI periphery with given parameters.
//* @brief Sets CR1->SPE
//* @param *spi SPI typedef
//* @param *conf Configuration struct

void DL_SPI_Init(SPI_TypeDef *spi, SPI_InitStruct *conf);

//* @brief Send data over SPI
//* @param *spi Select SPI for sending
//* @param *data data to send
//* @param length number of 8bit packets to send. If zero - assume data is null terminated string
void DL_SPI_Send(SPI_TypeDef *spi, uint8_t *data, uint32_t length);

//* @brief Recieves data from SPI.
//* @param *spi Selects SPI for recieving
//* @param *buffer Recieved data stores inside this array
//* @param lenght Number of bites to be recieved
void DL_SPI_Recv(SPI_TypeDef *spi, uint8_t *buffer, uint32_t length);

//* @brief Sends byte using desired SPI without ASSERT or DEASSERT CS
//* @param *spi Desired SPI
//* @param byte Byte to send
void DL_SPI_sendByte(SPI_TypeDef *spi, uint8_t byte);

//* @brief Recieves byte from desired SPI without ASSERTing or DEASSERTing CS
//* @param *spi SPI to recieve from
//* @return Recieved byte
uint8_t DL_SPI_readByte(SPI_TypeDef *spi);


#endif
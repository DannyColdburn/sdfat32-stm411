#include "DL_SDCARD.h"
#include "DL_Debug.h"

#define CMD0    0
#define CMD1    1
#define CMD8    8
#define CMD58   58
#define CMD59   59
#define ACMD41  41
#define READ_SINGLE_BLOCK    17
#define READ_MULTIPLE_BLOCK  18
#define WRITE_SINGLE_BLOCK   24
#define WRITE_MULTIPLE_BLOCK 25
#define DATA_RESP_ACCEPTED      0x05
#define DATA_RESP_CRC_ERROR     0x0B
#define DATA_RESP_WRITE_ERROR   0x0D

#define SD_R1_InIdleState 			0x01
#define SD_R1_EraseReset  			0x02
#define SD_R1_IllegalCommand		0x04
#define SD_R1_CommandCRCError		0x08
#define SD_R1_EraseSequenceError	0x10
#define SD_R1_AddressError			0x20
#define SD_R1_ParameterError		0x40

// *** Static variable declaration ***//
static SPI_TypeDef      *hSPI = 0;
static GPIO_TypeDef     *csPort = 0;
static uint8_t          csPin = 0;

// *** Static function declaration ***//
static void     set_mode_spi();
static void     spi_send_byte(uint8_t byte);        //TODO: Add check for DL_SPI.h included
static uint8_t  getByte();
static uint32_t getInt();
static uint8_t  spi_read_byte();                    //TODO: If does, then disable that commands
static void     send_command(uint8_t command, uint32_t args);
static void     send_command_ACMD(uint8_t acmd, uint32_t args);
static void     cs_assert();
static void     cs_deassert();
static uint8_t  crc7_gen(uint8_t *data, uint8_t len);
static void     delay(uint32_t t);

// *** Global function definition ***/
uint8_t DL_SDCARD_Init(SPI_TypeDef *spi_instance, GPIO_TypeDef *CS_Port, uint8_t cs_Pin){
    hSPI = spi_instance;
    csPort = CS_Port;
    csPin = cs_Pin;

    set_mode_spi();
    DBG("SDCard switched to SPI mode. Sending CMD0");
    cs_assert(); 
    send_command(CMD0, 0x0);

    uint8_t r1 = getByte();
    if (!(r1 & SD_R1_InIdleState)){
        DBGF("SDCard Failed to get idle state :: Responce is: %x", r1);
        DBG("SDCard: If responce is 0 - Card maybe dead or disconnected");
        return 0;
    }
    DBG("SDCard is in Idle state");

    send_command(CMD8, 0x000001AA);
    r1 = getByte();
    if (r1 != SD_R1_InIdleState) {
        DBG("SDCard failed to ack CMD8");
        return 0;
    }
    uint32_t r3 = getInt();
    if (r3 != 0x000001AA) {
        DBG("SDCard Failed to compare CMD8 value");
        return 0;
    }
    DBG("SDCard CMD8 ACK, bringing it to operation state...");

    uint8_t tryCount = 50;
    uint8_t sof = 0;
    do{
        send_command_ACMD(ACMD41, 0x40000000);
        r1 = getByte();
        // if (r1 = 0xFF){
        //     DBG("ACMD41 Seq gone wrong. Stopping all operations\n");
        //     return 0;
        // }
        if (!(r1 & SD_R1_InIdleState)){
            sof = 1;
            break;
        }
        tryCount--;
        delay(1000);
    } while (tryCount);

    if(!sof){
        DBG("SDCard failed to bring card to operation mode. Try increase try count or delay between readings\n");
        return 0;
    }
    DBG("SDCard is operational");

    send_command(CMD58, 0x0);
    r1 = getByte();

    if (r1 != 0x0){
        DBG("SDCard CMDM58 failed");
        return 0;
    }
    r3 = getInt();
    if (!(r3 & (1 << 30))){
        DBG("SDCard CSS bit not set");
    }
    DBG("SDCard initialized, clear to proceed!\n\n");

    while(hSPI->SR & SPI_SR_BSY);
    delay(50);
    cs_deassert();

    return 1;
}

uint8_t DL_SDCARD_WritePage(uint32_t addr, uint8_t *data){
    // How it should be
    // Sending CMD24 -> Getting R1 resp -> Send one byte -> Sending data tocken -> Send Data Packet -> Getting Data Resp. -> 
    // -> SDCard goes busy -> Wait for Busy end
    uint8_t r1;
    uint8_t dataResp;
    cs_assert();

    send_command(WRITE_SINGLE_BLOCK, addr);
    r1 = getByte();
    if (r1 != 0) {
        DBGF("Write command failed with R1: %u\n", r1);
        return 0;
    }
    // Writing data
    cs_assert();
    r1 = getByte();
    if (r1 != 0xFF) {
        DBG("Failed to get 0xFF before sending data packet");
        return 0;
    }
    spi_send_byte(0xFE); // Command tocken 'Data start'

    for (int i = 0; i < 512; i++) {
        spi_send_byte(data[i]);
    }
    spi_send_byte(0xAA);        // Sending CRC
    spi_send_byte(0xA1);

    dataResp = getByte();
    dataResp &= 0xF;        // Masking

    if (dataResp == DATA_RESP_CRC_ERROR) {
        DBG("Write failed with CRC error\n");
        goto error;
    } 

    if (dataResp == DATA_RESP_WRITE_ERROR) {
        DBG("Write failed with Write error\n");
        goto error;
    }

    if (dataResp == DATA_RESP_ACCEPTED) {
        DBG("Write complete\n");
    }

    cs_deassert();

    // We should wait while card is busy
    for (int i = 0; i < 20; i++){
        uint8_t r = spi_read_byte();
        if (r == 0xFF) {
            break;
        }
        if (i == 19) {
            DBG("Card failed to exit busy state\n");
            goto error;
        }
    }

    while(hSPI->SR & SPI_SR_BSY);
    delay(50);
    cs_deassert();
    return 1;

    error:
    for (int i = 0; i < 10; i++){
        spi_read_byte();
    }
    delay(50);
    cs_deassert();
    return 0;
}


uint8_t DL_SDCARD_Read(uint32_t addr, uint8_t *buffer){
    uint8_t r1;
    uint8_t dataTokenFound = 0;
    uint8_t tryCount = 30;
    cs_assert();

    while(getByte() != 0xFF) {
        // DBG("Card is busy");
        if(!--tryCount) {
            DBG("Card locked in busy state");
            return 0;
        }
        delay(10000);
    }
    tryCount = 30;

    send_command(READ_SINGLE_BLOCK, addr);

    // while(tryCount--) {
    //     r1 = getByte();
    //     if (r1 != 0xFF) {
    //         break;
    //     }
    //     delay(100);
    // }
    // tryCount = 20;

    r1 = getByte();
    if (r1) {
        DBGF("SDCard Read error: %x\n", r1);
        return 0;
    }
    
    //Getting Data token
    do{
        r1 = getByte();
        if (r1 == 0xFE) {
            dataTokenFound = 1;
            break;
        }
    } while (tryCount--);
    if (!dataTokenFound) {
        DBG("SDCard failed to get data token\n");
        return 0;
    }

    for(int i = 0; i < 512; i++){
        buffer[i] = getByte();
    }

    getByte();  // This 2 bytes are CRC
    getByte();
    delay(50);
    cs_deassert();
    return 1;
}


//#####################################//
// *** Static function definition *** //
//###################################//

//Sets SD card to SPI mode, and then we can communicate

static void send_command(uint8_t command, uint32_t args){
    //TODO: This function really can return R1 value, so it could be implemented just to check 'in place' what happened
    command |= 1 << 6;
    uint8_t *arg = (uint8_t *) &args;
    uint8_t crcdata[5] = { command, arg[3], arg[2], arg[1], arg[0]};
    uint8_t crc = crc7_gen(crcdata, 5);
    // cs_assert();

    spi_send_byte(command);
    spi_send_byte(arg[3]);
    spi_send_byte(arg[2]);
    spi_send_byte(arg[1]);
    spi_send_byte(arg[0]);
    spi_send_byte(crc + 1);
    spi_send_byte(0xFF); //Dummy byte
    while(hSPI->SR & SPI_SR_BSY);
    // delay(50);
    // cs_deassert();
}

static void send_command_ACMD(uint8_t acmd, uint32_t args){
    // cs_assert();
    send_command(0x77, 0x0);
    // uint8_t r1 = getByte(); Well, actually r1 doesn't need here
    getByte();
    send_command(acmd, args);

    // delay(50);
    // cs_deassert();
}

static void set_mode_spi(){
    cs_deassert();
    for (int i = 0; i < 15; i++){
        spi_send_byte(0xFF);
    }
    while (hSPI->SR & SPI_SR_BSY);

}

static void spi_send_byte(uint8_t byte){
    while(!(hSPI->SR & SPI_SR_TXE));
    hSPI->DR = byte;
    while(!(hSPI->SR & SPI_SR_RXNE));
    hSPI->DR;
}

static uint8_t spi_read_byte(){
    while(!(hSPI->SR & SPI_SR_TXE));
    hSPI->DR = 0xFF;
    while(!(hSPI->SR & SPI_SR_RXNE));
    return hSPI->DR;
}

static void cs_assert(){
    csPort->BSRR |= 1 << (csPin + 16);
}

static void cs_deassert(){
    csPort->BSRR |= 1 << csPin;
}

static uint8_t crc7_gen(uint8_t *data, uint8_t len){
    uint8_t crc = 0;
	uint8_t poly = 0x89;
	for(int i = 0; i < len; i++){
		crc ^= data[i];
		for(int j = 0; j < 8; j++){
			crc = (crc & 0x80u) ? ((crc << 1) ^ (poly << 1)) : (crc << 1);
		}
	}
	return crc;
}

static void delay(uint32_t t){
    while(t--);
}

static uint8_t getByte(){
    //cs_assert();
    uint8_t ret = spi_read_byte();
    while(hSPI->SR & SPI_SR_BSY);
    // delay(50);
    // cs_deassert();
    return ret;
}
static uint32_t getInt(){
    uint32_t ret = 0;
    // cs_assert();
    for (int i = 3; i > -1; i--){
        uint8_t b = spi_read_byte();
        ret |= b << (8 * i);
    }
    // delay(50);
    // cs_deassert();
    while(hSPI->SR & SPI_SR_BSY);
    return ret;
}
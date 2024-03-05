#include "sdcard.h"
#include "stm32f411xe.h"
#include "DL_Debug.h"

#define CMD0 64
#define CMD1 65
#define CMD2 66
#define CMD8 72
#define ACMD41 105
#define CMD58 122

#define READ_SINGLE     81 
#define READ_MULTIPLE   82
#define WRITE_SINGLE    88
#define WRITE_MULTIPLE  89
#define STOP_TRANS      76

#define DATA_RESP_ACCEPTED      0x05
#define DATA_RESP_CRC_ERROR     0x0B
#define DATA_RESP_WRITE_ERROR   0x0D

#define SD_R1_InIdleState 0x01
#define SD_R1_EraseReset  0x02
#define SD_R1_IllegalCommand 0x04
#define SD_R1_CommandCRCError 0x08
#define SD_R1_EraseSequenceError 0x10 
#define SD_R1_AddressError  0x20
#define SD_R1_ParameterError 0x40


// static SPI_TypeDef *hSPI;
// static GPIO_TypeDef *csGPIO;
// static uint8_t csPIN;

// At this time it only works with one sdcard. 
// If it planned to have multiple card, should be rewritten
static SDCard_t *hSDCard;

static void     init_spi(void);
static void     set_mode_spi(void);
static void     spi_send_byte(uint8_t byte);
static uint8_t  spi_read_byte(void);
static void     cs_assert(void);
static void     cs_deassert(void);
static uint8_t  crc7_gen(uint8_t *data, uint8_t len);
static uint8_t  send_command(uint8_t command, uint32_t args);
static uint8_t  send_command_ACMD(uint8_t command, uint32_t args);
static void     delay(uint32_t t);
static inline void wait_not_bsy(void) { while(hSDCard->SPI->SR & SPI_SR_BSY); }
static uint8_t  getByte(void); 
static uint32_t getInt(void);


uint8_t SDCard_Init(SDCard_t *sdcard) {
  DBG("SDCard: Init started...");
  // hSPI = sdcard->SPI;
  // csGPIO = sdcard->CS_Port;
  // csPIN = sdcard->CS_Pin;
  hSDCard = sdcard;

  set_mode_spi();
  delay(100);

  cs_assert();
  send_command(CMD0, 0x0);
  
  uint8_t r1 = getByte(); 
  if (!(r1 & SD_R1_InIdleState)) {
    DBGF("SDCard: CMD0 unexpected resonse %x", r1);
    DBG("SDCard: If response is 0x0 or 0xFF - check hardware");
    return 0;
  }
  DBG("SDCard: in Idle state");

  send_command(CMD8, 0x000001AA);
  r1 = getByte(); 
  if (r1 != SD_R1_InIdleState) {
    DBG("SDCard: failed to ack CMD8");
    return 0;
  }
  uint32_t r3 = getInt();
  if (r3 != 0x000001AA) {
    DBG("SDCard: failed to compare CMD8 value");
    DBGF("SDCard: CMD8 value %x", r3);
    return 0;
  }
  DBG("SDCard: CMD8 acknowledged. Switching to operation state");
 
  uint16_t tryCount = 0x1FF;
  uint8_t sof = 0;
  do {
    send_command_ACMD(ACMD41, 0x40000000);
    r1 = getByte();
    if (r1 == 0xFF) {
      DBG("SDCard: ACMD41 recieved 0xFF. Aborting...");
      return 0;
    }
    if (!(r1 & SD_R1_InIdleState)) {
      sof = 1;
      break;
    }
    tryCount--;
    delay(5000);
  } while(tryCount);

  if(!sof) {
    DBG("SDCard: failed to bring card to operation mode. Aborting");
  }
  DBGF("SDCard: in operation after %i cycles", 0x1FF - tryCount);

  send_command(CMD58, 0x0);
  r1 = getByte();
  
  if (r1 != 0x0) {
    DBG("SDCard: CMD58 failed to recieve");
  }
  r3 = getInt();
  if (!(r3 & (1 << 30))) {
    DBG("SDCard: CSS bit not set");
  }

  DBG("SDCard: fully initialized. Clear to proceed!\r\n");

  wait_not_bsy();
  delay(50);
  cs_deassert();

  return 1;
}

uint8_t SDCard_Write(uint32_t addr, uint8_t *data, uint32_t len) {
  uint8_t r1;
  uint8_t dataResp;
  cs_assert();
  send_command(WRITE_SINGLE, addr);
  r1 = getByte();

  if(r1) {
    DBGF("SDCard ERR: Write command failed: %x", r1);
    goto error;
  }
  r1 = getByte();
  if(r1 != 0xFF) {
    DBG("SDCard ERR: Card is busy after Write command");
    goto error;
  }

  spi_send_byte(0xFE); // Send tocken Data start 

  for(int i = 0; i < 512; i++) {
    spi_send_byte(data[i]);
  }

  spi_send_byte(0xAA);    // Two CRC dummy bytes
  spi_send_byte(0xA1);  

  dataResp = getByte();
  dataResp &= 0b11111;

  if(dataResp != DATA_RESP_ACCEPTED) {
    DBGF("SDCard ERR: Failed to accept data %x", dataResp);
    goto error;
  }
  DBG("SDCard: Write complete");

  // Wait till card is out of busy state
  for(int i = 0; i < 30; i++) {
    uint8_t byte = getByte();
    if (byte == 0xFF) break;
    delay(50); 
    if(i == 28) {
      DBG("SDCard WARN: Card in busy state after Write");
    }
  }

  wait_not_bsy();
  cs_deassert();
  delay(50);
  return 1;
error:
  wait_not_bsy();
  cs_deassert();
  delay(50);
  return 0;
}

uint8_t SDCard_Read(uint32_t addr, uint8_t *buffer, uint32_t pgCnt) {
  uint8_t r1 = 0;
  uint8_t dataTokenFound = 0;
  uint8_t tryCount = 60;
  uint8_t restart = 0;
  start: 

  cs_assert();

  while(getByte() != 0xFF) {
    if (!--tryCount) {
      DBG("SDCard: locked in busy state. Aborting");
      return 0;
    }
    delay(3000);
  }
  tryCount = 30;
  
  if (pgCnt == 1) {
    send_command(READ_SINGLE, addr);
  } else {
    send_command(READ_MULTIPLE, addr);
  }
  r1 = getByte();
  if(r1) {
    DBGF("SDCard: Read error: %x", r1);
    if (!restart) {           // Will try to execute init sequence
      restart =  1;
      SDCard_Init(hSDCard);
      DBG("Reboot completed");
      goto start;
    } else {
      return 0;
    }
  }

 for (uint32_t p = 0; p < pgCnt; p++) {
    // Trying to receive Data tocken 
    tryCount = 30;
    do {
      delay(200);
      uint8_t resp = getByte();
      if (resp == 0xFE) {       // 0xFE - data start tocken
        dataTokenFound = 1;
        break;
      }
    } while(tryCount--);
  
    if(!dataTokenFound) {
      DBG("SDCard: failed to get data token");
      return 0;
    }
    // Recieving actual data  
    for(int s = 0; s < 512; s++) {
      *buffer = getByte();
      buffer++;
      //buffer[s] = getByte();
    }
    getByte();  // 2 bytes of CRC
    getByte();
  }
  // If multiple pages read, then we stop transaction
  if(pgCnt >= 2) {
    send_command(STOP_TRANS, 0);
    getByte();    // Discard stuff byte 
    uint8_t b = 8; 
    uint8_t resp = getByte();
    while(resp != 0xFF) {
      if (!b--) {
        DBG("SDCard: Failed to get cmd resp after Multiple Read");
        return 0;
      }
    }
    if(resp) {
      DBGF("SDCard: Transmission response error %x", resp);
      return 0;
    }
    uint8_t t = 10;
    while(getByte() != 0xFF) {
      if(!t--) {
        DBG("SDCard WARN: Card busy after Transmission stop");
      }
    }
  }

  wait_not_bsy();
  delay(50);
  
  cs_deassert();
  return 1;
}


//#####################################// 
// *** Static function definition *** // 
//###################################//

static void set_mode_spi(void) {
  cs_assert();
  delay(10000);
  cs_deassert();

  for (uint8_t i = 0; i < 15; i++) {
    spi_send_byte(0xFF);
  }
  wait_not_bsy();
  delay(50);
}


static uint8_t send_command(uint8_t command, uint32_t args) {
  uint8_t *arg = (uint8_t *) &args;
  uint8_t crcdata[5] = { command, arg[3], arg[2], arg[1], arg[0]};
  uint8_t crc = crc7_gen(crcdata, 5);

  spi_send_byte(command);
  spi_send_byte(arg[3]);
  spi_send_byte(arg[2]);
  spi_send_byte(arg[1]);
  spi_send_byte(arg[0]);
  spi_send_byte(crc + 1);
  spi_send_byte(0xFF); // Dummy byte 
  while(hSDCard->SPI->SR & SPI_SR_BSY);
  delay(50);

  return 1;
}


static uint8_t  send_command_ACMD(uint8_t command, uint32_t args) {
  if (!send_command(0x77, 0x0)) return 0;
  getByte();
  if (!send_command(command, args)) return 0;
  return 1;
}


static void spi_send_byte(uint8_t byte) {
  while(!(hSDCard->SPI->SR & SPI_SR_TXE));
  hSDCard->SPI->DR = byte;
  while(!(hSDCard->SPI->SR & SPI_SR_RXNE));
  hSDCard->SPI->DR;
}


static uint8_t spi_read_byte(void) {
  while(!(hSDCard->SPI->SR & SPI_SR_TXE));
  hSDCard->SPI->DR = 0xFF; 
  while(!(hSDCard->SPI->SR & SPI_SR_RXNE));
  return hSDCard->SPI->DR;
}


static uint8_t getByte(void) {
  uint8_t ret = spi_read_byte();
  wait_not_bsy();
  delay(50);
  return ret;
}


static uint32_t getInt(void) {
  uint32_t ret = 0;
  for(int i = 3; i > -1; i--) {
    uint8_t b = spi_read_byte();
    ret |= b << (8 * i);
  }
  return ret;
}


static void cs_assert(void) {
  hSDCard->CS_Port->BSRR |= 1 << (hSDCard->CS_Pin + 16);
  // csGPIO->BSRR |= 1 << (csPIN + 16);
}


static void cs_deassert(void) {
  // csGPIO->BSRR |= 1 << csPIN;
  hSDCard->CS_Port->BSRR |= 1 << (hSDCard->CS_Pin);
}


static uint8_t crc7_gen(uint8_t *data, uint8_t len) {
  uint8_t crc = 0;
  uint8_t poly = 0x89;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      crc = (crc & 0x80u) ? ((crc << 1) ^ (poly << 1)) : (crc << 1);
    }
  }
  return crc;
}


static void delay(uint32_t t) {
  while(t--);
}


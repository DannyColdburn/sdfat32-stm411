#ifndef DL_SDCARD_H
#define DL_SDCARD_H
#include "stm32f411xe.h"
// Mne voobshe pohuy.
// A ti soglasno licensii ne mojesh menyat' v etom file nichego
// TODO: Should rename SDCardInfo to Filesystem info
// TODO: Should split apart SD Card code and File system code

#define MBR_1PART_ENTRY     0x1BE
#define FILE_WRITE  0x1
#define FILE_READ   0x2
#define SDCARD_MAX_FILENAME 32
#define DL_FILENAME_MAX 32

typedef struct{
    uint32_t rootClusterPos;        // Root in which entry placed
    uint16_t EntryPos;              // Position at page
    uint32_t fileSize;              // File size in bytes
    uint32_t dataClusterStart;      // Start cluster of file data
    uint32_t bytesLeftToRead;       // Used in read operation to continiously read file
    uint32_t readPosition;          // We'll read from this position and increase it after    
    uint32_t writePosition;
    uint32_t LFNcount;
}SDCardFile_t;

typedef struct{
    uint32_t PartitionLBA;
    uint8_t  sectorsPerCluster;
    uint16_t reservedSectorCount;
    uint8_t  numberOfFatCopies;
    uint32_t numberOfSectors;
    uint32_t sectorsPerFat;
    uint32_t rootStartClusterNumber;
    uint32_t rootLBA;
    uint16_t bytesPerSector;    
    uint32_t clusterSize;
    uint32_t clusterMapLBA;
}SDCardInfo_t;


/// @brief Init system and switches SD Card to SPI mode
/// @param spi_instance Selects configured SPI with SD Card connected
/// @param CS_Port GPIO port with chip select pin
/// @param CS_Pin Configured pin connected to CS(chip select) pin of SD Card
/// @return Operation status
/// @warning SPI Should be alredy configured before this function
uint8_t DL_SDCARD_Init(SPI_TypeDef *spi_instance, GPIO_TypeDef *CS_Port, uint8_t CS_Pin);

/// @brief Read Filesystem info, after this function we can get access to read/write
/// @param SDCardInfo SDCardInfo_t struct which holds all necessary data
/// @return Operation status
uint8_t DL_SDCARD_Mount(SDCardInfo_t *SDCardInfo);

/// @brief Create and/or open file on SD Card
/// @param SDCard SD Card to read from
/// @param fileName Name of file to be created or opened
/// @param attrib if attrib FILE_READ, file won't be created if none found, if FILE_WRITE - file will be created if none found
/// @return Pointer to SDCardFile_t struct.
/// @warning After work is done, you need to close file to prevent memory leak
SDCardFile_t *DL_SDCARD_Open(SDCardInfo_t *SDCard, const char *fileName, uint8_t attrib);


uint8_t DL_SDCard_WriteString(SDCardInfo_t *SDCard, SDCardFile_t *file, uint8_t *string);

uint8_t DL_SDCard_FileRead(SDCardInfo_t *SDCard, SDCardFile_t *file, uint8_t *buffer, uint32_t bytesToRead);

/// @brief Manual data read
/// @param addr LBA address
/// @param buffer Where to write
/// @return Status
uint8_t DL_SDCARD_Read(uint32_t addr, uint8_t *buffer);

/// @brief Writes page at specific address
/// @param addr LBA page address
/// @param data Buffer to write
/// @return Operation status
uint8_t DL_SDCARD_WritePage(uint32_t addr, uint8_t *data);

/// @brief DO NOT USE
void DL_SDCARD_TestFunc();

uint8_t *DL_SDCard_getLashError();


#endif
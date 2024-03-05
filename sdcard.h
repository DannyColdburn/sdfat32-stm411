#ifndef SDCARD_H
#define SDCARD_H

#include "stm32f411xe.h"

#define FS_FILE_READ    0x0  // Open for read, do not create file if none found
#define FS_FILE_WRITE   0x1  // If not found, file will be created
#define FS_FILE_TRUNC   0x2  // Write data at file beggining, prev data will be lost 
#define FS_FILE_APPEND  0x4  // Write data at the end of file
#define FS_FCACHE_PG    1    // Number of page to be cached 
/// Struct with SDCard configuration 
typedef struct {
  SPI_TypeDef   *SPI;     // SPI should be configured as master with frequency less than 400kHz 
  GPIO_TypeDef  *CS_Port; // Chip Select should be configured  
  uint8_t       CS_Pin;   // Pin should be configured 
} SDCard_t;

// Cluter map structure for holding info
typedef struct {
  uint32_t last_free_clust_pos; 
  uint32_t last_cached_page;
  uint8_t changed;   // Was map changed?
  uint8_t *map;
} clustmap_info_t;

// FileSystem info
typedef struct {
  SDCard_t *sdcard;
  clustmap_info_t *clust_info;
  uint32_t partitionLBA;
  uint32_t rootLBA;
  uint32_t clusterMapLBA;
  uint8_t  sectorsPerCluster;
  uint16_t reservedSectorCount;
  uint8_t  numOfFatCopies;
  uint32_t numOfSectors;
  uint32_t sectorsPerFat;
  uint32_t rootStartClusterNumber;
  uint16_t bytesPerSector;
  uint32_t clusterSize;
} FS_t;


typedef struct {
  FS_t *fs;
  uint32_t en_pos;  // Entry position at root 
  uint32_t r_pos;
  uint32_t w_pos;
  uint32_t cluster; 
  uint32_t size;
  uint32_t cached_pg;   // Last cached page 
  uint8_t *cache;
} FS_File_t;


/// @brief Inits SDCard and brings it to life
/// @param *sdcard SDCard_t struct with configuration
/// @warning SPI should be initialized externally
uint8_t SDCard_Init(SDCard_t *sdcard);

/// @brief Writes given data with given length to selected address of SDCard 
/// @param addr Page address 
/// @param *data Pointer to array with data to write
/// @param len Length of data array to write
uint8_t SDCard_Write(uint32_t addr, uint8_t *data, uint32_t len);

/// @brief Reads data from SDCard
/// @param addr Page address to read from 
/// @param *buffer Pointer to buffer where to store data from SDCard 
/// @param pgCnt Page count for read  
uint8_t SDCard_Read(uint32_t addr, uint8_t *buffer, uint32_t pgCnt);

/// @brief Reads filesystem info from given SD Card 
/// @param *sdcard Initialized SD Card 
/// @return Returns pointer to FS_t or 0 in case of an error
FS_t *FS_Init(SDCard_t *sdcard);

/// @brief Will open file in selected Filesystem 
/// @param *fs pointer to filesystem where to search file 
/// @param *fn Filename 
/// @param mode If selected FS_FILE_READ file won't be created if none found. FS_FILE_WRITE will create file. Also FS_FILE_TRUNC and FS_FILE_APPEND can be added using | 
/// @return Return pointer to file if created or found. Will return 0 if none found or failed to create new 
FS_File_t* FS_FileOpen(FS_t *fs, const char *fn, uint8_t mode);


/// @brief Read data from file by given lenght or until end of file 
/// @param *f FS_File_t pointer to file 
/// @param *buffer in which data will be written 
/// @param len Size of data to read in bytes 
uint32_t FS_FileRead(FS_File_t *f, uint8_t *buffer, uint32_t len);


uint32_t FS_FileWrite(FS_File_t *f, uint8_t *data, uint32_t len);


uint8_t FS_FileSync(FS_File_t *f);

uint8_t FS_FileClose(FS_File_t *f);


#endif 

#include "DL_SDCARD.h"
#include "malloc.h"
#include "memory.h"
#include "string.h"
#include "DL_Debug.h"

#define uint32_cast(x) (*(uint32_t *)(x))
#define uint16_cast(x) (*(uint16_t *)(x))
#define SD_BUFFER_MAX 512+256
#define MAX_LFN_COUNT 5     // Maximum count of LFNs
#define ENTRY_IS_FILE 1
#define ENTRY_IS_ARCH 3
#define ENTRY_ZERO 0
#define ENTRY_NOT_FILE 2

typedef struct{
    uint8_t found;
    uint32_t page;
    uint32_t pos;
}fileinfo_t;

//static uint8_t buffer[512];
//static uint8_t *buffer;
static uint8_t  sd_buffer[SD_BUFFER_MAX];
static uint8_t  getPartitionInfo(SDCardInfo_t *cardInfo);
static uint8_t  findFile(const char *name, SDCardFile_t *fileinfo, SDCardInfo_t *card);
static uint8_t  ucs2nameConvertion(char *ucsName, char *normalName);
static uint8_t  name2ucsConvention(const char *name, uint16_t *ucsName);
static uint8_t  removeTrailingBytes(char *data, uint8_t len, uint8_t byte);
static uint8_t  unpackSFN(char *dest, char *entry);
static uint8_t  getFileAttrib(SDCardInfo_t *card, SDCardFile_t *file);
static uint32_t getNextCluster(SDCardInfo_t *card, uint32_t sector);
static uint32_t getClusterChained(SDCardInfo_t *card, uint32_t startSector, uint32_t steps);
static uint32_t readPtrToAddr(SDCardInfo_t *card, SDCardFile_t);
static uint32_t createFile(SDCardInfo_t *card, SDCardFile_t *file, const char *name);
static uint8_t  readRootPage(SDCardInfo_t *SDCard, uint8_t *buffer, uint32_t pageOffset);
static uint8_t  readEntry(SDCardInfo_t *card, uint8_t *entry, uint32_t number);
static inline uint8_t  isFile(uint8_t *entry);
static inline uint8_t getEntryLFNCount(SDCardInfo_t *card, uint32_t entryPos);


uint8_t DL_SDCARD_Mount(SDCardInfo_t *SDCardInfo){
    // uint8_t *buffer = 0;
    // buffer = (uint8_t *) malloc(512);
    // if (buffer == 0){
    //     DBG("SDCard malloc() failed to allocate memory\n");
    //     return 0;
    // }
    // DBG("SDCard memory allocated\n");

    if (DL_SDCARD_Read(0, sd_buffer) == 0){
        DBG("SDCard Failed to read Master Boot\n");
        return 0;
    }
    DBG("SDCard checking MBR...\n");

    //Checking MBR signature
    if (uint16_cast(&sd_buffer[0x1FE]) != 0xAA55) {
        DBGF("SDCard SIGNATURE MISMATCH: %x\n", uint16_cast(&sd_buffer[0x1FE]));
        return 0;
    }
    DBG("SDCard Signature matched 0x55AA\n");
    
    // 1st partition entry read and go
    SDCardInfo->PartitionLBA = uint32_cast(&sd_buffer[MBR_1PART_ENTRY + 0x8]);
    DBGF("SDCard Partition block Address: %x\n", SDCardInfo->PartitionLBA);


    DL_SDCARD_Read(SDCardInfo->PartitionLBA, sd_buffer); 
    if (uint16_cast(&sd_buffer[0x1FE]) != 0xAA55){
        DBGF("SDCard Partition Signature mismatch: %x!!!\n", uint16_cast(&sd_buffer[0x1FE]));
        return 0;
    }    
    getPartitionInfo(SDCardInfo);  
    
    memset(sd_buffer, 0, SD_BUFFER_MAX);    // Cleaning buffa
    // free(buffer);
    // buffer = 0;
    DBG("SDCard Mounted\n");
    return 1;
}

SDCardFile_t *DL_SDCARD_Open(SDCardInfo_t *SDCard, const char *fileName, uint8_t attrib){
    if(strlen(fileName) > SDCARD_MAX_FILENAME) {
        DBG("SDCard Filename is too long\n");
        return 0;
    }
    SDCardFile_t *file = (SDCardFile_t *)malloc(sizeof(SDCardFile_t));
    if (!file) {
        DBG("Failed to allocate memory inside SDCARD_OPEN()\nPossible memory leak\n");
        return 0;
    }
    memset(file, 0, sizeof(SDCardFile_t));          // Bring all zeroes

    if (!findFile(fileName, file, SDCard)){         // If none found
        if (attrib == FILE_READ) {                  // If we only need to read file, then we go out returning nothing
            DBGF("No such file: \"%s\"\n", fileName);
            free(file);                             //? What if we create a sturcture inside stack, and only before we found file we call malloc
            return 0;
        } else {
            // Here is file created
            DBG("Creating file...\n");
            createFile(SDCard, file, fileName);
            free(file);
            return 0;
        }
    }

    getFileAttrib(SDCard, file);   

    return file;
}

uint8_t DL_SDCard_FileRead(SDCardInfo_t *SDCard, SDCardFile_t *file, uint8_t *buffer, uint32_t bytesToRead){
    uint32_t lastPosition = file->readPosition + bytesToRead;

    if (file->readPosition >= file->fileSize){
        return 0;
    }

    if (file->fileSize < lastPosition) {
        lastPosition = file->fileSize;
    }

    uint32_t writePointer = 0;
    // uint32_t bytesWritten = 0; // Not used?
    uint8_t temp[512] = {0};
    DBG("Reading file\n");

    
    
    while(1){
        uint32_t clust = file->readPosition / (512 * SDCard->sectorsPerCluster);
        uint32_t page = file->readPosition / 512  - (SDCard->sectorsPerCluster * clust);
        uint32_t byte = file->readPosition - (512 * SDCard->sectorsPerCluster * clust + page * 512);
        uint32_t toRead = (bytesToRead + byte - 1) / 512 + 1;
        uint32_t clustChain = getClusterChained(SDCard, file->dataClusterStart, clust);
        DBGF("Cluster offset: %u\n", clust);
        DBGF("Page offset: %u\n", page);
        DBGF("Byte offset: %u\n", byte);
        DBGF("Pages to read: %u\n", toRead);
        DBGF("Cluster to read: %u\n", clustChain);

        uint32_t addr = SDCard->PartitionLBA + SDCard->rootLBA + ((clustChain - 2) * 64 + page);
        //DBGF("Reading LBA: %u\n", addr);

        DL_SDCARD_Read(addr, temp);

        uint32_t toWrite = 0;
        if (byte) {
            toWrite = bytesToRead > (512 - byte) ? 512 - byte : bytesToRead;
            memcpy(&buffer[writePointer], &temp[byte], toWrite);
        } else {
            toWrite = lastPosition - file->readPosition > 512 ? 512 : lastPosition - file->readPosition;
            memcpy(&buffer[writePointer], temp, toWrite);
        }

        writePointer += toWrite;
        file->readPosition += toWrite;

        if (file->readPosition >= lastPosition) {
            break;
        } 

    }
    return 1;       
}



static uint8_t findFile (const char *fileName, SDCardFile_t *file, SDCardInfo_t *card){
    DBGF("Searching for %s", fileName);
    uint8_t entry[32];
    uint32_t pos = 0;
    uint32_t lfnCount = 0;
    uint8_t *name = 0;
    while(readEntry(card, entry, ++pos)){ 
	// DBGF("Pos is: %u", pos);
	// DBGH((char *)entry, 32);
        uint8_t res = isFile(entry);
        if (res == ENTRY_ZERO)      return 0;
        if (res == ENTRY_IS_ARCH)   continue;
        if (res == ENTRY_NOT_FILE)   continue;
      	// DBGF("Found file at pos: %u", pos);
	// DBGC((char *)entry, 32);
        lfnCount = getEntryLFNCount(card, pos);
		// DBGF("LFN count = %u", lfnCount);
        if (lfnCount) {
            name = (uint8_t *) calloc(14, lfnCount);
            if (!name) {
                DBG("Mem alloc failed at findFile()");
                return 0;
            }
            
            for (int i = 1; i < lfnCount + 1; i++){ // TODO pack it to inline func
                uint8_t lfnEntry[32];
                readEntry(card, lfnEntry, pos - i);
                uint8_t ucs2name[26] = {0};
                uint8_t normalName[14] = {0};
                memcpy(ucs2name, &lfnEntry[0x01], 10);
                memcpy(ucs2name + 10, &lfnEntry[0x0E], 12);
                memcpy(ucs2name + 22, &lfnEntry[0x1C], 4);
                ucs2nameConvertion((char *) ucs2name, (char *)normalName);
                strcat((char *) name, (char *)normalName);
            }
            
        } else {
            name = (uint8_t *) calloc(14, sizeof(uint8_t));
            if (!name) {
                DBG("Mem alloc failed at findFile()");
                return 0;
	    }
            unpackSFN((char *)name, (char *)entry);            
        }

        DBGF("Cheking: %s", name);
        if (strcmp((char *)name, fileName) == 0) {
            DBG("Found File\n");
	    file->EntryPos = pos;
            free(name);
            return 1;
        }        
        free(name);
    }
    return 0;
}

static uint32_t readPtrToAddr(SDCardInfo_t *card, SDCardFile_t file){
    // NOP
    return 0;
}

static uint32_t createFile(SDCardInfo_t *card, SDCardFile_t *file, const char *name){
    // Processing file name to define SFN and LFN
    uint32_t len = strlen(name);
    if (len > DL_FILENAME_MAX) {
        DBG("Filename too big\n");
        return 0;
    }

    // Getting count of LFN entries
    uint32_t lfnCount = (len * 2 - 1 ) / 25 + 1 ;
    DBGF("File will be created with %u LFNs\n", lfnCount);

    // Create UCS2 string and fill it with name
    uint16_t *ucs2 = (uint16_t *) calloc(len, sizeof(uint16_t));
    if (!ucs2) {
        return 0;
    }
    name2ucsConvention(name, ucs2);
    DBGH((char *) ucs2, len * 2);

    // Now we find clear space to write our structures
    
    uint32_t entryNum = 0;
    uint8_t entry[32];
    while(1) {
        readEntry(card, entry, entryNum);
        
        if (entry[0x0] == 0xE5 || entry[0x0] == 0x0) {  // If we found one empty place, we should check next
            readEntry(card, entry, entryNum + 1);       // Checking next
            if (entry[0x0] == 0xE5 || entry[0x0] == 0x0) {   // TODO: it should consider lfn count and then check all them, for now we assume there is no more than 1 LFN
                break; 
            } else {
                entryNum++;     // We increase because we already checked next one, and it's not suitable
            }
        }    

        entryNum++; // Proceed to next one 
    }
    
    if (!entryNum) {
        DBG("Failed to find empty place to write file entry\n");
        return 0;
    }
    // So here we have to write our entry
    


    

    return 1;
}

static uint8_t  isFile(uint8_t *entry){
    if (entry[0x0] == 0) {
        return ENTRY_ZERO;
    }
    if (entry[0x0] == 0xE5) {
        return ENTRY_IS_ARCH;
    }
    if (entry[0x0B] != 0x20) {
        return ENTRY_NOT_FILE;
    }

    return ENTRY_IS_FILE;
}

static uint8_t  readEntry(SDCardInfo_t *card, uint8_t *entry, uint32_t number){
    uint32_t cluster = number * 32 / card->clusterSize;  
    uint32_t page = number * 32 / 512 - (card->sectorsPerCluster * cluster);
    uint32_t pos = number * 32 - (card->clusterSize * cluster + page * 512);
    uint32_t clusterOffset = getClusterChained(card, card->rootStartClusterNumber, cluster);
    if (clusterOffset == 0){
        return 0; // No clusters next
    }

    uint32_t addr = card->PartitionLBA + card->rootLBA + ((clusterOffset - 2) * 64 + page);
    // DBGF("Reading addr: %u", addr);
    uint8_t buff[512];
    DL_SDCARD_Read(addr, buff);
    memcpy(entry, &buff[pos], 32);
    return 1;
}

static uint8_t readRootPage(SDCardInfo_t *SDCard, uint8_t *buffer, uint32_t pageOffset){
    uint32_t clusterOffset = 64 / pageOffset;
    uint32_t cluster = getClusterChained(SDCard, SDCard->rootStartClusterNumber, clusterOffset);
    if(!cluster) return 0;
    
    uint32_t addr = SDCard->PartitionLBA + SDCard->rootLBA + (cluster - 2) * 64;
    if (!DL_SDCARD_Read(addr, buffer)) return 0;
    
    return 1;
}


// This function sends cluster 
// 
static uint32_t getClusterChained(SDCardInfo_t *card, uint32_t startSector, uint32_t steps){
    uint32_t temp = startSector;
    while (steps--){
        temp = getNextCluster(card, temp);
        if (!temp) return 0;
    }
    return temp;     // In case steps is zero
}


// This function gets a number of next cluster
// You pass current cluster, function looks at it, and if number cluster written, then returns written cluster
// If there is mark CLUSTER END, then return 0, because there is no next cluster
static uint32_t getNextCluster(SDCardInfo_t *card, uint32_t cluster){
    uint32_t clusterNext = 0;       // Declaration
    uint8_t clusterMap[512];
    uint32_t clusterPage = cluster / 128;       // Cluster page offset, one cluster entry is 4 byte, max number at page is 128
    uint32_t clusterPos = cluster - ( 128 * clusterPage );      // Cluster entry pos offset at the page
    DL_SDCARD_Read(card->PartitionLBA + card->reservedSectorCount + clusterPage, clusterMap); // Cluster maps starts from partition addr + reserved sectors and goes on

    clusterNext = uint32_cast(&clusterMap[clusterPos * 4]);
    if (clusterNext == 0x0FFFFFFF) {
        return 0;
    }

    return clusterNext;
}

static uint8_t  getFileAttrib(SDCardInfo_t *card, SDCardFile_t *file){
    uint8_t *entry = &sd_buffer[file->EntryPos * 32 + 256];
    file->fileSize = uint32_cast(&entry[0x1C]);
    file->bytesLeftToRead = file->fileSize;
    // DBGF("File size: %u bytes\n", file->fileSize);
    uint32_t clusterNumber = 0;
    clusterNumber |= (uint16_cast(&entry[0x14]) << 16) | uint16_cast(&entry[0x1A]);
    file->dataClusterStart = clusterNumber;
    // DBGF("File data cluster: %u\n", file->dataClusterStart);
    return 1; 
}

static uint8_t  unpackSFN(char *dest, char *entry){
    memcpy(dest, entry, 8);                 // Copy Short file name
    removeTrailingBytes(dest, 8, ' ');      // Remove trailing spaces if present (if filename is smaller that 8 bytes)
    strcat(dest, ".");                      // Adding '.' to split name and extension
    char ext[4] = {0};                   // Extension holder            
    memcpy(ext, &entry[0x08], 3);           // Getting extension from entry
    for (int i = 0; i < 3; i++) { ext[i] |= 0x60; } // Making extension lowercase
    strcat(dest, ext);                      // Placing all together 
    return 1;
}

static uint8_t ucs2nameConvertion(char *ucsName, char *normalName){
    for (int i = 0; i < 13; i++) {
        normalName[i] = ucsName[i * 2];
    }
    normalName[13] = '\0';
    return 1;
}

static uint8_t  name2ucsConvention(const char *name, uint16_t *ucsName){
    uint32_t len = strlen(name);
    for (int i = 0; i < len; i++){
        ucsName[i] = (uint16_t)name[i];
    }

    return 1;
}

static inline uint8_t getEntryLFNCount(SDCardInfo_t *card, uint32_t entryPos){
    uint8_t entry[32];
    uint8_t lfnCount = 0;

    // First check if entry has LFN
    readEntry(card, entry, entryPos--);     // Read and decrement for later
	// DBGF("Searh for LFN at %u", enryPos);
    // DBGC((char *) entry, 32);
	if (!(entry[0x06] == '~' && entry[0x07] == '1')) {
        return 0; 
    }
    
    while (1){
        readEntry(card, entry, entryPos--);  
        if ( ((entry[0x0] & 0x40) == 0x40) && (entry[0x0B] == 0x0F)) {
            lfnCount = entry[0x0] & 0x0F;
            break;
        }
        

        if(!entryPos) {
            return 0;   // At this point we are in the beggining of root folder
        }
    }

    return lfnCount;
}

static uint8_t  removeTrailingBytes(char *data, uint8_t len, uint8_t byte){
    char *buff = 0;
    buff = (char *) malloc(len);
    if (!buff) return 0;
    strcpy(buff, data);
    for (int i = len - 1; i > -1; i--){
        if (buff[i] == byte) {
            buff[i] = '\0';
        } else {
            break;
        }
    }

    strcpy(data, buff);
    free(buff);
    buff = 0;
    return 1;
}

static uint8_t  
getPartitionInfo(SDCardInfo_t *cardInfo){
    cardInfo->sectorsPerCluster = sd_buffer[0x0D];
    cardInfo->reservedSectorCount = uint16_cast(&sd_buffer[0x0E]);
    cardInfo->numberOfFatCopies = sd_buffer[0x10];
    cardInfo->numberOfSectors = uint32_cast(&sd_buffer[0x20]);
    cardInfo->sectorsPerFat = uint32_cast(&sd_buffer[0x24]);
    cardInfo->rootStartClusterNumber = uint32_cast(&sd_buffer[0x2C]);
    cardInfo->rootLBA = cardInfo->reservedSectorCount + (cardInfo->numberOfFatCopies * cardInfo->sectorsPerFat);
    cardInfo->clusterSize = cardInfo->sectorsPerCluster * cardInfo->bytesPerSector;
    cardInfo->clusterMapLBA = cardInfo->PartitionLBA + cardInfo->reservedSectorCount;
    #ifdef DEBUG_D
        DBG( "SDCard Partition info:\n");
        DBGF("  Bytes per Sector: %u\n", uint16_cast(&sd_buffer[0x0B]));
        DBGF("  Sectors per Cluster: %u\n", cardInfo->sectorsPerCluster);
        DBGF("  Reserved Sector Count: %u\n", cardInfo->reservedSectorCount);
        DBGF("  Number of FAT copies: %u\n", cardInfo->numberOfFatCopies);
        DBGF("  Number of Sector in Partition: %u\n", cardInfo->numberOfSectors);
        DBGF("  Sectors per FAT: %u\n", cardInfo->sectorsPerFat);
        DBGF("  Cluster number of root: %u\n", cardInfo->rootStartClusterNumber);
        DBGF("  Calculated root LBA: %u\n", cardInfo->rootLBA);
        DBGF("  Cluster size in bytes: %u\n", cardInfo->clusterSize);
        DBGF("  Cluster map LBA: %u\n", cardInfo->clusterMapLBA);
    #endif

    return 1;
}



// static uint32_t getInt(uint32_t pos){
//     uint32_t ret;
//     ret |= buffer[pos]   << 24;
//     ret |= buffer[pos+1] << 16;
//     ret |= buffer[pos+2] << 8;
//     ret |= buffer[pos+3];

//     return ret;
// }

// static uint16_t getShort(uint32_t pos){
//     uint16_t ret = 0;
//     ret |= buffer[pos] << 8;
//     ret |= buffer[pos + 1];
//     return ret;
// }
// 
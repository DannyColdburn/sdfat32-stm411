#include "DL_SDCARD.h"
#include "malloc.h"
#include "memory.h"
#include "string.h"
#include "DL_Debug.h"
#include "fat_fnutils.h"

#define uint32_cast(x) (*(uint32_t *)(x))
#define uint16_cast(x) (*(uint16_t *)(x))
#define SD_BUFFER_MAX 512+256
#define MAX_LFN_COUNT 5     // Maximum count of LFNs
#define ENTRY_IS_FILE 1
#define ENTRY_IS_ARCH 3
#define ENTRY_ZERO 0
#define ENTRY_NOT_FILE 2
#define NO_ENTRY_AVAIL 0xFFFFFFFF

typedef struct{
    uint8_t found;
    uint32_t page;
    uint32_t pos;
}fileinfo_t;

typedef struct {
    uint32_t clust_offset;
    uint32_t page_offset;
    uint32_t pos_offset;
}offset_t;

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
static inline uint32_t readClusterValue(SDCardInfo_t *card, uint32_t clusterPos);
static inline uint32_t getAvailableEntryPos(SDCardInfo_t *card, uint32_t entryCount);
static inline uint32_t expandCluster(SDCardInfo_t *card, uint32_t sector);
static inline uint8_t  writeEntry(SDCardInfo_t *card, uint32_t pos, uint8_t *data);
static uint8_t         writeCluster(SDCardInfo_t *card, uint32_t pos, uint32_t value);
static uint32_t        getLastClusterPos(SDCardInfo_t *card, uint32_t cluster);
static inline offset_t  calculateOffsets(SDCardInfo_t *card, uint32_t position, uint32_t sizeInBytes);
static uint8_t readPageCached(SDCardInfo_t *card, uint32_t Addr, uint8_t *buffer); // Use it if you need to read one page multiple time
static uint32_t expandFileCluster(SDCardInfo_t *SDCard, uint32_t cluster);


uint8_t DL_SDCARD_Mount(SDCardInfo_t *SDCardInfo){
    
    if (DL_SDCARD_Read(0, sd_buffer) == 0){
        DBG("SDCard Failed to read Master Boot");
        return 0;
    }
    DBG("SDCard checking MBR...");

    //Checking MBR signature
    if (uint16_cast(&sd_buffer[0x1FE]) != 0xAA55) {
        DBGF("SDCard SIGNATURE MISMATCH: %x", uint16_cast(&sd_buffer[0x1FE]));
        return 0;
    }
    DBG("SDCard Signature matched 0x55AA");

    
    // 1st partition entry read and go
    SDCardInfo->PartitionLBA = uint32_cast(&sd_buffer[MBR_1PART_ENTRY + 0x8]);
    DBGF("SDCard Partition block Address: %x", SDCardInfo->PartitionLBA);

    uint8_t tryCount = 4; 
    uint8_t res = 0;
    while (tryCount--) {        // Sometimes we get an error while getting data read responce, try few times
        res = DL_SDCARD_Read(SDCardInfo->PartitionLBA, sd_buffer);
        if (!res) continue;
        else break;
    }

    if (!res) {
        DBG("Partition failed to read after few attempts");
        DBG("Mount Failed");
        return 0;
    }

    if (uint16_cast(&sd_buffer[0x1FE]) != 0xAA55){
        DBGF("SDCard Partition Signature mismatch: %x!!!", uint16_cast(&sd_buffer[0x1FE]));
        return 0;
    }    
    getPartitionInfo(SDCardInfo);  
    
    memset(sd_buffer, 0, SD_BUFFER_MAX);    // Cleaning buffa
    // free(buffer);
    // buffer = 0;
    DBG("SDCard Mounted");
    return 1;
}

SDCardFile_t *DL_SDCARD_Open(SDCardInfo_t *SDCard, const char *fileName, uint8_t attrib){
    if(strlen(fileName) > SDCARD_MAX_FILENAME) {
        DBG("SDCard Filename is too long");
        return 0;
    }
    SDCardFile_t *file = (SDCardFile_t *)malloc(sizeof(SDCardFile_t));
    if (!file) {
        DBG("Failed to allocate memory inside SDCARD_OPEN()\r\nPossible memory leak");
        return 0;
    }
    memset(file, 0, sizeof(SDCardFile_t));          // Bring all zeroes

    if (!findFile(fileName, file, SDCard)){         // If none found
        if (attrib == FILE_READ) {                  // If we only need to read file, then we go out returning nothing
            DBGF("No such file: \"%s\"", fileName);
            free(file);                             //? What if we create a sturcture inside stack, and only before we found file we call malloc
            return 0;
        } else {
            // Here is file created
            DBG("Creating file...");
            if (!createFile(SDCard, file, fileName)) {
                DBG("Failed to create file!");
                free(file);
                return 0;
            }
        }
    }

    getFileAttrib(SDCard, file);   

    return file;
}

uint8_t DL_SDCard_FileRead(SDCardInfo_t *SDCard, SDCardFile_t *file, uint8_t *buffer, uint32_t bytesToRead){
    uint32_t lastPosition = file->readPosition + bytesToRead;

    if (file->readPosition >= file->fileSize){
        DBG("Read position is bigger than file size");
        return 0;
    }

    if (file->fileSize < lastPosition) {
        lastPosition = file->fileSize;
    }

    uint32_t writePointer = 0;
    // uint32_t bytesWritten = 0; // Not used?
    uint8_t temp[512] = {0};
    DBG("Reading file");

    
    
    while(1){
        offset_t read_offset = calculateOffsets(SDCard, file->readPosition, 1);

        uint32_t clust = read_offset.clust_offset;
        uint32_t page = read_offset.page_offset;
        uint32_t byte = read_offset.pos_offset;
        // uint32_t clust = file->readPosition / (512 * SDCard->sectorsPerCluster);
        // uint32_t page = file->readPosition / 512  - (SDCard->sectorsPerCluster * clust);
        // uint32_t byte = file->readPosition - (512 * SDCard->sectorsPerCluster * clust + page * 512);
        uint32_t toRead = (bytesToRead + byte - 1) / 512 + 1;
        uint32_t clustChain = getClusterChained(SDCard, file->dataClusterStart, clust);
        DBGF("Cluster offset: %u", clust);
        DBGF("Page offset: %u", page);
        DBGF("Byte offset: %u", byte);
        DBGF("Pages to read: %u", toRead);
        DBGF("Cluster to read: %u", clustChain);

        if (!clustChain) {
            // DL_delay_ticks(0xFFFFFFFF);
            DBG("ERR: Failed to get next cluster");
            DBG("ERR: Check cluster chain");
        }

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

uint8_t DL_SDCard_WriteString(SDCardInfo_t *SDCard, SDCardFile_t *file, uint8_t *string){
    uint32_t len = strlen((char *) string);
    uint32_t wPos = 0;
    uint32_t bytesLeft = len;
    uint8_t sector[512] = {0};
    uint8_t expandedOnce = 0;

    while(bytesLeft) {
        DBGF("Bytes left %u", bytesLeft);

        offset_t offset = calculateOffsets(SDCard, file->writePosition, 1);
        uint32_t cluster = getClusterChained(SDCard, file->dataClusterStart, offset.clust_offset);
        DBG("Writing in:");
        DBGF("  Write Position: %u", file->writePosition);
        DBGF("  Cluster offset: %u", offset.clust_offset);
        DBGF("  Page offset: %u", offset.page_offset);
        DBGF("  Pos offset: %u", offset.pos_offset);
        DBGF("  Cluster chained: %u", cluster);

        if (!cluster) {
            if(!expandedOnce) {
                DBG("Unreachable cluster found. Expanding...");
                if (!expandFileCluster(SDCard, file->dataClusterStart)) {
                    DBG("Cluster expanding failed. Aborting");
                    return 0;
                }
            } else {
                DBG("Second try to expand cluster detected. Aborting");
                return 0;
            }
        }

        uint32_t addr = SDCard->PartitionLBA + SDCard->rootLBA + (cluster - 2) * 64 + offset.page_offset;
        if (!DL_SDCARD_Read(addr, sector)) {
            DBG("Read failed in WriteString. Aborting");
            return 0;
        }
        // DBGH((char *)sector, 512);
        
        uint32_t toWrite = (512 - offset.pos_offset) < bytesLeft ? 512 - offset.pos_offset : bytesLeft;
        // DBGF("toWrite %u", toWrite);
        memcpy(&sector[offset.pos_offset], &string[wPos], toWrite);
        // DBGH((char *)sector, 512);

        if (!DL_SDCARD_WritePage(addr, sector)) {
            DBG("Write failed in WriteString");
            return 0;
        }

        wPos += toWrite;
        bytesLeft -= toWrite;
        file->writePosition += toWrite;
        file->fileSize += toWrite;
        // DBGF("Bytes left %u", bytesLeft);
    }

    // Update file attrib;
    DBG("Updating entry record");

    uint8_t entry[32] = {0};
    if (!readEntry(SDCard, entry, file->EntryPos)) {
        DBG("Failed to read entry in WriteString()");
        return 0;
    }
    uint32_t *pSz = (uint32_t *)&entry[0x1C];
    *pSz = file->fileSize;
    if (!writeEntry(SDCard, file->EntryPos, entry)) {
        DBG("Failed to update entry");
        return 0;
    }

    return 1;
}

// Will try to find entry position where is allowed to place
// multiple entry depending on entryCount. WILL COUNT DELETED ENTRIES AS ALLOWED PLACE
// Returns enry position on success, on fail - 0
static inline uint32_t getAvailableEntryPos(SDCardInfo_t *card, uint32_t entryCount) {
    uint8_t entry[32];
    uint32_t entryPos = 0;
    uint8_t didExpand = 0;
    uint32_t freeFound = 0;
    while (1) {
        uint32_t res = readEntry(card, entry, entryPos + freeFound);
        DBGF("Getting entry info at %u", entryPos + freeFound);
        DBGH((char *)entry, 32);
        if (!res) {     // If we got zero then maybe we approached end of cluster
            if (didExpand) {
                DBG("Cluster expand executes second time. Stopping");
                return NO_ENTRY_AVAIL;
            }
            offset_t offset = calculateOffsets(card, entryPos + freeFound - 1, 32); // Calculate on what offset we got entry read error, previous one should be good
            uint32_t entryCluster = getClusterChained(card, card->rootStartClusterNumber, offset.clust_offset); // Get last good cluster
            if (!expandCluster(card, entryCluster)) {
                DBG("Failed to expand cluster");
                return NO_ENTRY_AVAIL;
            }
            didExpand = 1;
            continue;       //And then we try again 
        }

        if (entry[0x0] == 0xE5 || entry[0x0] == 0) {
            freeFound++;
        } else {
            entryPos += freeFound;
            entryPos ++;
        }

        if (freeFound == entryCount) {
            return entryPos;
        }       
    }
}

// Will try to expand cluster
// On success return number of next cluster created, on fail - 0
// Cluster number should be last cluster in chain
// Otherwise zero will be returned      // TODO: maybe it should be check cluster chain and then expand from last valid value
static inline uint32_t expandCluster(SDCardInfo_t *card, uint32_t cluster) {
   // For expanding cluster we need to find an empty cluster
   // And write in given cluster, which should be Cluster End Value, a number of next free cluster
    uint32_t nextFreeClusterPos = 0;
    uint32_t clusterPos = cluster;
    if (readClusterValue(card, cluster) != 0x0FFFFFFF) {
        return 0;
    }
    
    while(readClusterValue(card, ++clusterPos)); // We read until we get 0 value
    nextFreeClusterPos = clusterPos;
    writeCluster(card, cluster, nextFreeClusterPos);
    writeCluster(card, nextFreeClusterPos, 0x0FFFFFFF);
    return nextFreeClusterPos;
}

static uint32_t expandFileCluster(SDCardInfo_t *card, uint32_t startCluster) {
    // For now we need to find EOC
    uint32_t nextFreeCluster = 1;
    uint32_t lastPos = startCluster;
    uint32_t lastValue = 0;
    DBGF("Expanding cluster from: %u", startCluster);
    while(1) {
        lastValue = readClusterValue(card, lastPos);
        if (lastValue == 0x0FFFFFFF) {
            DBG("   Found EOC");
            break; // We at the end of cluster, proceed to expanding
        } else if (!lastValue) {
            DBG("ERR: Something went wrong at expandFileCluster");
            DBG("ERR: Recieved 0 as last value");
        } else {
            // In this case we found a next cluster number in clusteUnreUnrer map
            if (lastPos == lastValue) {
                DBG("ERR: Cluster connected to itself. Aborting");
                return 0;
            }
            lastPos = lastValue;
            DBGF("Last pos: %u\r\nLast Value: %u", lastPos, lastValue);
            continue;
        }
    }



    while(readClusterValue(card, ++nextFreeCluster));
    DBGF("Next free cluster: %u", nextFreeCluster);

    if (!writeCluster(card, lastPos, nextFreeCluster)) {
        DBG("ERR: Failed to expand file cluster 1. Aborting");
        return 0;
    }

    if (!writeCluster(card, nextFreeCluster, 0x0FFFFFFF)) {
        DBG("ERR: failed to expand file cluster 2. Aborting");
        return 0;
    }

    return 1;
}

static inline uint8_t  writeEntry(SDCardInfo_t *card, uint32_t pos, uint8_t *data) {
    uint8_t buffer[512];
    offset_t offset = calculateOffsets(card, pos, 32);
    uint32_t clusterOffset = getClusterChained(card, card->rootStartClusterNumber, offset.clust_offset);
    DBG("Write entry page hex");

    uint32_t addr = card->PartitionLBA + card->rootLBA + ((clusterOffset - 2) * card->sectorsPerCluster + offset.page_offset);
    if (!readPageCached(card, addr, buffer)) {
        return 0;
    }
    // DBGH((char *)buffer, 512);

    memcpy(&buffer[offset.pos_offset], data, 32);
    DBG("\r\nAfter Modification:");
    // DBGH((char *)buffer, 512);
    DL_SDCARD_WritePage(addr, buffer);

    DL_SDCARD_Read(addr, buffer);
    DBG("After write on card");
    // DBGH((char*) buffer, 512);
    return 1;
}


// Return stuct holding offset for given position and size of element
static inline offset_t calculateOffsets(SDCardInfo_t *card, uint32_t position, uint32_t sizeInBytes){
    offset_t offset;
    offset.clust_offset = position * sizeInBytes / card->clusterSize;
    offset.page_offset = position * sizeInBytes / 512 - (card->sectorsPerCluster * offset.clust_offset);
    offset.pos_offset = position * sizeInBytes - (card->clusterSize * offset.clust_offset + offset.page_offset * 512);
    return offset;
}

// Will return cluster value written in cluster map
static inline uint32_t readClusterValue(SDCardInfo_t *card, uint32_t clusterPos){
    uint8_t buffer[512];
    uint32_t ret = 0;
    offset_t offset = calculateOffsets(card, clusterPos, 4);
    uint32_t addr = card->PartitionLBA + card->reservedSectorCount + offset.page_offset + (offset.clust_offset * card->sectorsPerCluster);
    readPageCached(card, addr, buffer);
    // DBG("Cluster page dump");
    // DBGH(buffer, 512);
    // DL_delay_ticks(1000);
    ret = uint32_cast(&buffer[offset.pos_offset]);
    return ret;
}

static uint8_t writeCluster(SDCardInfo_t *card, uint32_t pos, uint32_t value){
    uint8_t page[512];
    offset_t offset = calculateOffsets(card, pos, 4);
    uint32_t addr = card->PartitionLBA + card->reservedSectorCount + offset.page_offset + offset.clust_offset * card->sectorsPerCluster;
    readPageCached(card, addr, page);
    memcpy(&page[offset.pos_offset], (uint8_t*)&value, 4);
    DL_SDCARD_WritePage(addr, page);
    return 1;
}

static uint32_t getLastClusterPos(SDCardInfo_t *card, uint32_t cluster){
    uint32_t res = 0;
    while(1){
        res = getNextCluster(card, cluster);
        if (!res) {         // Res 0 means that current cluster is last cluster
            return cluster;
        }
        cluster = res;
    }
}

static uint8_t readPageCached(SDCardInfo_t *card, uint32_t Addr, uint8_t *buffer){
    static uint32_t lastAddr = 0;
    static uint8_t  lastBuffer[512] = {0};

    if ( lastAddr != Addr) {            // If new address is new address we read
        if (!DL_SDCARD_Read(Addr, lastBuffer)) {    // If read failed we return 0
            return 0;
        }
        // If read successfull
        //lastAddr = Addr;
    }

    memcpy(buffer, lastBuffer, 512);
    return 1;
}

static uint8_t findFile (const char *fileName, SDCardFile_t *file, SDCardInfo_t *card){
    DBGF("Searching for %s...", fileName);
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
            DBG("Found File");
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
    if (len > 12) {
        DBG("Filename too big");
        return 0;
    }
    
    uint32_t entryNum = 0;
    uint32_t clusterNum = 0;
    uint8_t  entry[32] = {0};
    
    uint8_t strName[8] = {0};
    memset(strName, ' ', 8);


    char *delim = strchr(name, '.');
    memcpy(strName, name, delim - name);

    memcpy(entry, strName, 8);
    memcpy(&entry[0x08], "TXT", 3);
    entry[0x0B] = 0x20;
    

    while(readClusterValue(card, ++clusterNum));
    
    uint16_t *lowClust = (uint16_t *) &entry[0x1A];
    uint16_t *highClust = (uint16_t *) &entry[0x14];

    *lowClust = clusterNum & 0xFFFF;
    *highClust = (clusterNum & 0xFFFF0000) >> 16;

    // DBG("Will write entry:");

    entryNum = getAvailableEntryPos(card, 1); // +1 for SFN entry    
    if (entryNum == NO_ENTRY_AVAIL) {
        DBG("Failed to find empty place to write file entry");
        return 0;
    }

    DBGF("Found suitable position: %u", entryNum);
    if (!writeEntry(card, entryNum, entry)) {
        DBG("Failed to write entry");
        return 0;
    }

    if (!writeCluster(card, clusterNum, 0x0FFFFFFF)) {
        DBG("Cluster write failed");
        return 0;
    }

    file->EntryPos = entryNum;
    file->dataClusterStart = clusterNum;

    return 1;
}

static uint8_t isFile(uint8_t *entry){
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

    uint32_t addr = card->PartitionLBA + card->rootLBA + ((clusterOffset - 2) * card->sectorsPerCluster + page);
    DBGF("Reading addr: %u", addr);
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
    // DBG("Cluster chained: ");
    // DBGF("  Start Cluster: %u", temp);
    // DBGF("  Steps: %u", steps);

    if (!steps) {
        return startSector;
    }

    for(int i = 0; i < steps; i++){
        temp = readClusterValue(card, temp);
        // DBGF(" Next cluster value: %u", temp);
        if (temp == 0x0FFFFFFF) return 0;
    }
    // DBGF("  Chain: %u", temp);
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
    // uint8_t *entry = &sd_buffer[file->EntryPos * 32 + 256];
    uint8_t entry[32] = {0};
    readEntry(card, entry, file->EntryPos);
    file->fileSize = uint32_cast(&entry[0x1C]);
    DBGF("File size: %u", file->fileSize);
    file->bytesLeftToRead = file->fileSize;
    // DBGF("File size: %u bytes\n", file->fileSize);
    if (file->fileSize) file->writePosition = file->fileSize - 1;
    //file->writePosition = file->fileSize; // TODO is this really good?
    uint32_t clusterNumber = 0;
    clusterNumber |= (uint16_cast(&entry[0x14]) << 16) | uint16_cast(&entry[0x1A]);
    file->dataClusterStart = clusterNumber;
    DBGF("File data cluster: %u", file->dataClusterStart);
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
    cardInfo->clusterSize = 512 * cardInfo->sectorsPerCluster;
    cardInfo->clusterMapLBA = cardInfo->PartitionLBA + cardInfo->reservedSectorCount;
    #ifdef DEBUG_D
        DBG( "SDCard Partition info:");
        DBGF("  Bytes per Sector: %u", uint16_cast(&sd_buffer[0x0B]));
        DBGF("  Sectors per Cluster: %u", cardInfo->sectorsPerCluster);
        DBGF("  Reserved Sector Count: %u", cardInfo->reservedSectorCount);
        DBGF("  Number of FAT copies: %u", cardInfo->numberOfFatCopies);
        DBGF("  Number of Sector in Partition: %u", cardInfo->numberOfSectors);
        DBGF("  Sectors per FAT: %u", cardInfo->sectorsPerFat);
        DBGF("  Cluster number of root: %u", cardInfo->rootStartClusterNumber);
        DBGF("  Calculated root LBA: %u", cardInfo->rootLBA);
        DBGF("  Cluster size in bytes: %u", cardInfo->clusterSize);
        DBGF("  Cluster map LBA: %u", cardInfo->clusterMapLBA);
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

#include <string.h>
#include <ctype.h>
#include "fat_fnutils.h"


uint8_t SFNchecksum(uint8_t *shortname){
    uint8_t checksum = 0;
    int i = 0;

    for(int i = 0; i < 11; i++){
        checksum = ((checksum & 1) ? 0x80 : 0) + (checksum >> 1) + shortname[i];
    }

    return checksum;
}


uint8_t mkSFN(const uint8_t *name, uint8_t *sfn){
    /*
    Short file name is 11 bytes max, therefore sfn field must be the same
    Function strips null terminator, sfn field size is guaranteed by FAT specification
    Conversion strips bytes over max - 10 bytes, appends ~1 and 6 bytes of extension
    Returns 0 on success
    */
    uint8_t *buffer = malloc(strlen(name));
    uint8_t lossy = 0;
    

    strcpy(buffer, name);

    uint8_t *ext = strrchr(buffer, '.');
    uint8_t *end = strchr(buffer, '\0');
    uint8_t *s = 0;

    if(!(ext == 0)){
        //Cut excess bytes from name body
        if(ext - 1 - buffer > 8){
            memset(buffer + 7, 0x7f, ext - 1 - buffer - 7);     //0x7f is ASCII DEL
            lossy |= 1;
        }

        //Remove all dots but the dot of extension
        s = strchr(buffer, '.');
        if(!(ext == 0)){
            while(s != ext){
                *s = 0x7f;
                s = strchr(buffer, '.');
            }
        }
        
        //Cut excess bytes from extension
        if(buffer - ext + 1 > 3){
            memset(ext + 1 + 3, '\0', end - ext - 1);
        }
    }
    else{
        //Same excess bytes cutting from body
        if(strlen(buffer) > 8){
            memset(buffer + 7, 0x7f, strlen(name) - 7);
            lossy |= 1;
        }
    }

    //Convert lowercase characters to uppercase
    for(size_t i = 0; i < strlen(name); i++){
        toupper(buffer + i);
    }
    
    //Remove spaces
    s = strchr(buffer, ' ');
    while(s != 0){
        *s = 0x7f;
        lossy |= 0x1;
        s = strchr(buffer, ' ');
    }

    //Replace any non-ASCII + extended characters with '_'
    for(uint8_t i = 0; i < srtlen(name); i++){
        if((*(buffer + i) < 33 || *(buffer + i) > 126) &&
            *(buffer + i) < 0x80 || *(buffer + i) > 0xFF &&
            *(buffer + i) != 0x7f){
                *(buffer + i) = '_';
                lossy |= 0x1;
        }
    }

    s = buffer;
    for(size_t i = 0; i < strlen(sfn); i++){
        while(  *s == '\0' ||
                *s == 0x7f ||
                *s == '.'){
            if(s >= end) return 1;
            s++;
        }

        *(sfn + i) = s;
    }
    
    free(buffer);

    return 0;
}


uint16_t fillFAT(FAT_eLFN **ppeLFN, uint8_t const lfnCount, FAT_eSFN *eSFN, const char *ucs_name, const char name){
    /*
    Fills structs for lfn and sfn FAT32 filename
    Writes into accepted structs
    Returns 0 on success
    */
    const uint8_t *shortname = malloc(11);
    mkSFN(name, shortname);

    //setting up short filename entry
    memcpy(eSFN->DIR_Name, shortname, 8);
    memcpy(eSFN->DIR_Name_ext, shortname + 9, 3);
    eSFN->DIR_Attr              = 0x0;
    eSFN->DIR_NTRes             = 0x0;
    eSFN->DIR_CrtTimeHundrth    = 0x0;
    eSFN->DIR_CrtTime           = 0x0;
    eSFN->DIR_CrtDate           = 0x0;
    eSFN->DIR_LstAccDate        = 0x0;
    eSFN->DIR_FstClusHI         = 0x0;
    eSFN->DIR_FstClusLO         = 0x0;
    eSFN->DIR_FileSize          = 0x0;
    eSFN->DIR_WrtDate           = 0x0;
    eSFN->DIR_WrtTime           = 0x0;


    //setting up long filename entry
    uint8_t checksum = SFNchecksum(shortname);
    free(shortname);

    {
    FAT_eLFN *peLFN = ppeLFN;

    //fill in everything except the name
    for(uint8_t order = 1; order <= lfnCount; order++){
        peLFN->LDIR_Ord = order == lfnCount ? LDIR_NAME_END : order;
        peLFN->LDIR_Attrs       =  DIR_ENTRY_ATTR_LONG_NAME;
        peLFN->LDIR_Type        =  LDIR_TYPE;
        peLFN->LDIR_FstClustLO  =  0x0;
        peLFN->LDIR_Chksum      =  checksum;
        peLFN = ppeLFN + 1;
    }

    //lfn name processing
    const uint16_t *lfn = malloc(lfnCount * 13 * sizeof(uint16_t));
    memset(lfn, 0, lfnCount * 13 * sizeof(uint16_t));

    memcpy(lfn, ucs_name, wcslen(ucs_name));
    memset(lfn + wcslen(ucs_name), 0xFF, wcschr(lfn, 0x00) - lfn);

    peLFN = ppeLFN;
    uint16_t plfn = lfn;
    for(size_t i = 0; i < lfnCount; i++){
        memcpy(peLFN->LDIR_Name1, lfn, 10);
        memcpy(peLFN->LDIR_Name2, lfn + 10, 12);
        memcpy(peLFN->LDIR_Name1, lfn + 22, 4);
        plfn += 26;
        peLFN = ppeLFN + 1;
    }

    free(lfn);
    }


    return 0;
}
#include <string.h>
#include <ctype.h>
#include <malloc.h>
#include <wchar.h>
#include "fat_fnutils.h"
#include "DL_Debug.h"


uint8_t SFNchecksum(uint8_t *shortname){
    uint8_t checksum = 0;
    // uint32_t i = 0;

    for(int i = 0; i < 11; i++){
        checksum = ((checksum & 1) ? 0x80 : 0) + (checksum >> 1) + shortname[i];
    }

    return checksum;
}


uint8_t mkSFN(const char *name, uint8_t *sfn){
    /*
    Short file name is 11 bytes max, therefore sfn field must be the same
    Function strips null terminator, sfn field size is guaranteed by FAT specification
    Conversion strips bytes over max - 10 bytes, appends ~1 and 6 bytes of extension
    Returns 1 on success
    */
    char *buffer = (char *)calloc(strlen(name) + 1, sizeof(char));
    if (!buffer) {
        DBG("Failed to mallo at mkSFN()");
        return 0;
    }
    uint8_t lossy = 0;
    

    strcpy(buffer, name);

    char *ext = strrchr(buffer, '.');
    char *end = strchr(buffer, '\0');
    char *s = 0;

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
        toupper(buffer[i]);
    }
    
    //Remove spaces
    s = strchr(buffer, ' ');
    while(s != 0){
        *s = 0x7f;
        lossy |= 0x1;
        s = strchr(buffer, ' ');
    }

    //Replace any non-ASCII + extended characters with '_'
    for(uint8_t i = 0; i < strlen(name); i++){
        if(((*(buffer + i) < 33 || *(buffer + i) > 126)) &&
            (*(buffer + i) < 0x80 || *(buffer + i) > 0xFF) &&
            (*(buffer + i) != 0x7f)){
                *(buffer + i) = '_';
                lossy |= 0x1;
        }
    }

    s = buffer;
    for(size_t i = 0; i < strlen((char *)sfn); i++){
        while(  *s == '\0' ||
                *s == 0x7f ||
                *s == '.'){
            if(s >= end) return 1;
            s++;
        }

        *(sfn + i) = *s;
    }

    if (lossy == 1) memcpy(sfn + 6, "~1", 2);
    
    free(buffer);

    return 1;
}


uint8_t fillFAT(FAT_eLFN **ppeLFN, const uint8_t lfnCount, FAT_eSFN *eSFN, const char *ucs_name, const char *name){
    /*
    Fills structs for lfn and sfn FAT32 filename
    Writes into accepted structs
    Returns 1 on success
    */
    uint8_t *shortname = calloc(11, sizeof(uint8_t));
    mkSFN((char *)name, (uint8_t *) shortname);

    uint32_t ucs2_len = (strlen(name) + 1);
    uint16_t *ucs2_name = calloc(ucs2_len, sizeof(uint16_t));
    if (!ucs2_name) {
        DBG("Malloc failed fillFAT()");
        return 0;
    }

    // Naming convertion
    for (int i = 0; i < strlen(name); i ++){
        ucs2_name[i] = (uint16_t) (name[i]);
    }

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
    FAT_eLFN *peLFN = 0; // ppeLFN[0];

    //fill in everything except the name
    for(uint8_t order = 0; order < lfnCount; order++){
        peLFN = ppeLFN[order];
        peLFN->LDIR_Ord = order + 1 == lfnCount ? LDIR_NAME_END : order + 1;
        peLFN->LDIR_Attrs       =  DIR_ENTRY_ATTR_LONG_NAME;
        peLFN->LDIR_Type        =  LDIR_TYPE;
        peLFN->LDIR_FstClustLO  =  0x0;
        peLFN->LDIR_Chksum      =  checksum;
        // peLFN = ppeLFN[order];
    }

    //lfn name processing
    uint16_t *lfn = calloc((lfnCount * 13), sizeof(uint16_t));
    memcpy(lfn, ucs2_name, ucs2_len * sizeof(uint16_t));

    memset(lfn + ucs2_len, 0xFF, (lfnCount * 13) - ucs2_len);


    //memset(lfn + wcslen(ucs_name), 0xFF, (wcschr(lfn, L"\0") - lfn));

    peLFN = ppeLFN;
    uint16_t plfn = lfn;
    for(size_t i = 0; i < lfnCount; i++){
        peLFN = ppeLFN[i];


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
#pragma once

#include <stdint.h>

#define DIR_ENTRY_ATTR_READ_ONLY        0x01
#define DIR_ENTRY_ATTR_HIDDEN           0x02
#define DIR_ENTRY_ATTR_SYSTEM           0x04
#define DIR_ENTRY_ATTR_VOLUME_ID        0x08
#define DIR_ENTRY_ATTR_DIRECTORY        0x10
#define DIR_ENTRY_ATTR_ARCHIVE          0x20
#define DIR_ENTRY_ATTR_LONG_NAME        0x0F

#define DIR_NTRES_BODY_LOW_CASE         0x08
#define DIR_NTRES_EXTENSION_LOW_CASE    0x10

#define LDIR_NAME_END                   0x40
#define LDIR_ENTRY_UNUSED               0xE5
#define LDIR_EOD                        0x0
#define LDIR_TYPE                       0x0

typedef struct{
    uint8_t     DIR_Name[8];
    uint8_t     DIR_Name_ext[3];
    uint8_t     DIR_Attr;
    uint8_t     DIR_NTRes;              //RESERVED
    uint8_t     DIR_CrtTimeHundrth;     //valid 0-199 hundreths of a second (this is used to fine-tune for crtTime secs)
    uint16_t    DIR_CrtTime;            //bits 15-11 valid 0-23 hours, bits 10-5 valid 0-59 mins, bits 4-0 valid 0-29 2-sec count
    uint16_t    DIR_CrtDate;            //relative to 1/1/1980, bits 15-9 valid 0-127 years, bits 8-5 valid 1-12 months, bits 4-0 valid 1-31 days
    uint16_t    DIR_LstAccDate;
    uint16_t    DIR_FstClusHI;
    uint16_t    DIR_WrtTime;
    uint16_t    DIR_WrtDate;
    uint16_t    DIR_FstClusLO;
    uint32_t    DIR_FileSize;
}FAT_eSFN;


typedef struct{
    uint8_t     LDIR_Ord;
    uint16_t    LDIR_Name1[5];      //5 UNICODE chars
    uint8_t     LDIR_Attrs;
    uint8_t     LDIR_Type;
    uint8_t     LDIR_Chksum;
    uint16_t    LDIR_Name2[6];      //6 UNICODE chars
    uint16_t    LDIR_FstClustLO;    //RESERVED
    uint16_t    LDIR_Name3[2];      //3 UNICODE chars
}FAT_eLFN;


uint8_t SFNchecksum (uint8_t sfn[]);
uint8_t fillFAT(FAT_eLFN **ppeLFN, const uint8_t lfnCount, FAT_eSFN *eSFN, const char *ucs_name, const char *name);
uint8_t mkSFN(const char *name, uint8_t *sfn);
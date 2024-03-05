#ifndef FSUTILS_H
#define FSUTILS_H 

#include "stm32f411xe.h"
#include "sdcard.h"

typedef struct{
  uint8_t sfn[8];
  uint8_t ext[3];
  uint8_t attr;
  uint8_t mark;       // reserved 
  uint8_t crt_ms;     // Creation time in 10ms portions
  uint16_t crt_time;  // Creation time 
  uint16_t crt_date;  // Creation date 
  uint16_t lst_acc;   // Last access date 
  uint16_t clustHI;   // Data cluster high bytes 
  uint16_t wrt_time;
  uint16_t wrt_date;
  uint16_t clustLO; 
  uint32_t size; 
} SFN_t;

typedef struct {
  uint8_t seq_number;
  uint8_t name1[5 * 2];
  uint8_t atrb;       // Always 0x0F
  uint8_t type;       // Always 0x00
  uint8_t checksum;
  uint8_t name2[6 *2 ];
  uint16_t zero;
  uint8_t name3[2 * 2];
} LFN_t;

typedef struct {
  uint32_t cl;    // Cluster offset 
  uint32_t pg;    // Page offset 
  uint32_t ps;    // Position offset 
  uint32_t pg_abs; // Page absolute offset value
} offset_t;


uint8_t *construct_names(const char *fn, uint8_t *lfn_count);

// Calculate offset of element with size in bytes (l) at given pos
offset_t calculate_offset(FS_t *fs, uint32_t pos, uint32_t l);


#endif 

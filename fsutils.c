#include "fsutils.h"
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include "DL_Debug.h"
#include "sdcard.h"
#include "stdlib.h"

uint8_t calculate_checksum(const uint8_t *sfn);

uint8_t *construct_names(const char *fn, uint8_t *lfn_count) {
  uint8_t *result = 0; // Return value
  size_t n_len  = 0;  // Name len
  size_t e_len  = 0; // Extension len
  size_t full_len = strlen(fn);
  char *ext     = strrchr(fn, '.'); 
  // Calculating lengths
  if (ext) {
    ext += 1;
    e_len = strlen(ext);
    n_len = ext - fn - 1;
  } else {
    n_len = strlen(fn);
  }
  // Check for lowercase 
  uint8_t has_lowercase = 0;
  for (int i = 0; i < n_len; i++) {
    if (islower( (int) fn[i])) {has_lowercase++; break;}
  }
  // Check if LFNs are needed 
  *lfn_count = 0;
  if (n_len > 8 || has_lowercase) {
    *lfn_count = (full_len - 1) / 13 + 1; 
  }
  // Allocate needed memory
  result = (uint8_t *) calloc(32 + (*lfn_count * 32), sizeof(uint8_t));
  if(!result) {
    DBG("FS ERR: Failed to allocate memory in constuct_names");
    return 0;
  }
  // Fill SFN
  SFN_t *sfn = (SFN_t *) result;
  // Copy filename and extension
  memcpy(sfn->sfn, fn, n_len > 8 ? 8 : n_len);
  memcpy(sfn->ext, ext, e_len);
  // Bring em to uppercase 
  for (int i = 0; i < 11; i++) result[i] = toupper(result[i]);
  // Add paddings to name and extension or add '~1'
  if (n_len > 8) { sfn->sfn[6] = '~'; sfn->sfn[7] = '1'; }
  for (int i = n_len; i < 8; i++) sfn->sfn[i] = ' ';
  for (int i = e_len; i < 3; i++) sfn->ext[i] = ' ';
  // Attribue
  sfn->attr = 0x20;   // Mark as a file
  // RTC REQUIRED AT THIS POINT
  sfn->crt_ms = 0;
  sfn->lst_acc = 0;
  sfn->crt_date = 0;
  sfn->wrt_date = 0;
  sfn->crt_time = 0;
  sfn->wrt_time = 0; 
  // Time for LFN 
  if(!(*lfn_count)) {
    return result;
  }
  // Allocate memory for ucs letters
  uint16_t *ucs = calloc(*lfn_count * 13, sizeof(uint16_t));
  if (!ucs) {
    DBG("FS WARN: Failed to allocate memory for UCS-2");
    *lfn_count = 0;
    return result;
  }
  // Fill UCS-2 
  for(int i = 0; i < full_len; i++) {
    ucs[i] = (uint16_t)fn[i];
  }
  // Fill UCS-2 with paddings 
  for(int i = full_len + 1; i < *lfn_count * 13; i++) {
    ucs[i] = UINT16_MAX;
  }
  // Checksums are really needed 
  uint8_t sum = calculate_checksum((uint8_t *)sfn);
  // Now we will structure
  uint8_t *lfn = &result[32];
  for (int i = 0; i < *lfn_count; i++) {
    LFN_t *p = (LFN_t *) &lfn[i * 32];
    uint16_t *u = &ucs[i * 13];             // Easier to understand
    p->seq_number = (i + 1);
    if ((i + 1) == *lfn_count) p->seq_number |= 0x40; // Place End of Sequence
    p->atrb = 0x0F;
    p->checksum = sum;
    memcpy(p->name1, u, 5 * sizeof(uint16_t));
    memcpy(p->name2, u + 5, 6 * sizeof(uint16_t));
    memcpy(p->name3, u + 11, 2 * sizeof(uint16_t));
  }
  free(ucs);
  return result;
}


uint8_t calculate_checksum(const uint8_t *sfn) {
  uint8_t sum = 0;

  for(uint8_t i = 11; i; i--) {
    sum = ((sum & 1) << 7) + (sum >> 1) + *sfn++;
  }
  return sum;
}


offset_t calculate_offset(FS_t *fs, uint32_t pos, uint32_t l) {
  offset_t ret = {0};
  ret.cl = pos * l / fs->clusterSize;
  ret.pg = pos * l / 512 - (fs->sectorsPerCluster * ret.cl);
  ret.ps = pos * l - (fs->clusterSize * ret.cl + ret.pg * 512);
  ret.pg_abs = pos * l / 512;
  return ret;
}





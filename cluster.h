#ifndef CLUSTER_H
#define CLUSTER_H 
#include "sdcard.h"

#define CLUST_NOT_VALID 0xFFFFFFFF
#define CLUST_LAST      0x8FFFFFFF

// Read cluster map page and writes to FileSystem cluster map buffer
uint8_t read_cluster_map(FS_t *fs, uint32_t pg_abs);
// Sync cluster map - i mean write to sdcard 
uint8_t sync_cluster_map(FS_t *fs);

// Returns cluster value from given position
uint32_t read_cluster_value(FS_t *fs, uint32_t pos);
// Writes value to cluster map cache
uint8_t write_cluster_value(FS_t *fs, uint32_t pos, uint32_t val); 
// Returst first found 0x00000 cluster position
uint32_t find_empty_cluster(FS_t *fs);
//  OBSOLETE Returs next cluster position, check for CLUST_LAST. 
// Will Return 0 if failed 
uint32_t get_next_cluster(FS_t *fs, uint32_t clust);
// Expand cluster and returns next cluster position 
uint32_t expand_cluster(FS_t *fs, uint32_t clust);

#endif 

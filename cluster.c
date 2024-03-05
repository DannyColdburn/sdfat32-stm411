#include "cluster.h"
#include "DL_Debug.h"
#include "sdcard.h"
#include <stdint.h>
#include "fsutils.h"
#include <memory.h>
#include <stdlib.h>



uint8_t read_cluster_map(FS_t *fs, uint32_t pg_abs) {
  // Check if already cached
  if (fs->clust_info->last_cached_page == pg_abs) {
    return 1;
  }
  // If not cached - before we read another page we should check for changes
  if (fs->clust_info->changed) {
    if (!sync_cluster_map(fs)) {
      return 0; 
    }
  }
  // Calculate address 
  uint32_t addr = fs->clusterMapLBA + pg_abs;
  if (!SDCard_Read(addr, fs->clust_info->map, 1)) {
    DBG("FS ERR: Failed to read cluster map");
    return 0;
  }
  // If success - change last cached page 
  fs->clust_info->last_cached_page = pg_abs;
  return 1;
}


uint8_t sync_cluster_map(FS_t *fs) {
  uint32_t addr = fs->clusterMapLBA + fs->clust_info->last_cached_page;
  if (!SDCard_Write(addr, fs->clust_info->map, 1)) {
    DBG("FS ERR: Failed to sync cluster map");
    return 0;
  }
  fs->clust_info->changed = 0;
  fs->clust_info->last_cached_page = UINT32_MAX;
  return 1;
}


uint32_t read_cluster_value(FS_t *fs, uint32_t pos) {
  offset_t of = calculate_offset(fs, pos, 4);
  if (!read_cluster_map(fs, of.pg_abs)) return CLUST_NOT_VALID;
  return *(uint32_t *) &fs->clust_info->map[of.ps]; 
}


uint8_t write_cluster_value(FS_t *fs, uint32_t pos, uint32_t val) {
  offset_t of = calculate_offset(fs, pos, 4); 
  if (!read_cluster_map(fs, of.pg_abs)) return 0;
  memcpy(&fs->clust_info->map[of.ps], &val, 4);
  // Mark cluster page as changed
  fs->clust_info->changed = 1;
  DBGH(fs->clust_info->map, 512);
  return 1;
}


uint32_t find_empty_cluster(FS_t *fs) {
  uint32_t pos = fs->clust_info->last_free_clust_pos;

  while(1) {
    uint32_t res = read_cluster_value(fs, pos);
    if (!res) break;
    if (res == CLUST_NOT_VALID ) {
      DBG("FS ERR: Failed to find empty cluster");
      return CLUST_NOT_VALID;
    }
    pos++;
  }
  fs->clust_info->last_free_clust_pos = pos;
  return pos;
}


uint32_t expand_cluster(FS_t *fs, uint32_t clust) {
  uint32_t val = read_cluster_value(fs, clust);
  if (val != CLUST_LAST) {
    DBG("FS ERR: Expand_cluster - not a last cluster");
    return 0;
  }
  uint32_t free = find_empty_cluster(fs);
  if (free == CLUST_NOT_VALID) {
    DBG("FS ERR: Expand_cluster - failed to find next free cluster");
    return 0;
  }
  // All found, now we write in existing cluster position of next cluster
  // And Write CLUST_LAST into that next cluster 
  if (!write_cluster_value(fs, clust, free)) return 0;
  if (!write_cluster_value(fs, free, CLUST_LAST)) return 0;
  return free;
}




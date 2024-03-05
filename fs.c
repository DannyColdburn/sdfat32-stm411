#include "DL_Debug.h"
#include "sdcard.h"
#include <malloc.h>
#include <stdint.h>
#include <string.h>
#include "fsutils.h"
#include "cluster.h"


#define uint16_cast(x) *(uint16_t *)(x)
#define uint32_cast(x) *(uint32_t *)(x)
#define szPage 512
#define CLUSTER_MAP_CACHED_PAGES 1
#define ROOT_DIR_CACHED_PAGES 1 
#define FILE_CACHE_SZ 1


#define ENTRY_NOENTRY 0x0 
#define ENTRY_ARCH    0x1 
#define ENTRY_NOTFILE 0x2 
#define ENTRY_FILE    0x3 

uint32_t rootDirCachedPage = 0xFFFFFFFF;
uint8_t rootDirCache[ROOT_DIR_CACHED_PAGES * 512];
uint8_t rootDirChanged = 0;


static uint32_t calculate_page_address(FS_t *fs, uint32_t startCluster, uint32_t page);
static inline uint8_t check_signature(uint8_t *m) { return uint16_cast(&m[0x1FE]) == 0xAA55 ? 1 : 0; }
static void get_partition_info(FS_t *fs, uint8_t *p);
static uint8_t find_file(FS_t *fs, FS_File_t *f, const char *fn);
static uint8_t read_root_dir(FS_t *fs, uint32_t page);
static uint8_t sync_rood_dir(FS_t *fs);
static uint8_t *read_entry(FS_t *fs, uint32_t en);
static uint8_t get_entry_info(uint8_t *en);
static char   *get_entry_name(FS_t *fs, uint32_t pos);
static char   *extract_sfn(uint8_t *e);
static uint32_t get_lfn_count(FS_t *fs, uint32_t ep);
static char   *extract_lfn(FS_t *fs, uint32_t epos);
static uint8_t get_file_info(FS_t *fs, FS_File_t *f);
static uint8_t read_file_cache(FS_File_t *f, uint32_t pg_abs);
static uint8_t create_file(FS_t *fs, FS_File_t *f, const char *n);
static uint32_t find_empty_entry_pos(FS_t *fs, uint32_t count);
static uint32_t write_entry(FS_t *fs, uint8_t *e, uint32_t pos);



// ####################################//
// *** PUBLIC FUNCTION DEFINITION *** //
// ##################################//
FS_t *FS_Init(SDCard_t *sdcard) {
  FS_t *fs = (FS_t *) calloc(1, sizeof(FS_t)); 
  if (!fs) {
    DBG("FS ERR: failed to allocate memory in FS_Init");
    return 0;
  }
  // Reading Master Boot Record  
  uint8_t MBR[szPage] = { 0 };
  if (!SDCard_Read(0, MBR, 1)) {
    DBG("FS ERR: failed to get MBR page");
    goto error;
  }
  // Check MBR signatures 
  if (!check_signature(MBR)) {
    DBG("FS ERR: MBR signature mismatch");
    goto error;
  }

  fs->partitionLBA = uint32_cast(&MBR[0x1BE] + 0x8); // Write down partition LBA  
  DBGF("FS: partition LBA: %x", fs->partitionLBA);

  uint8_t tryCount = 4;
  uint8_t res = 0;
  while(tryCount--) {
    res = SDCard_Read(fs->partitionLBA, MBR, 1);
    if (!res) continue;
    else break;
  }

  if(!res) {
    DBG("FS: failed to read partition after few attempts");
    DBG("FS ERR: Mount failed");
    goto error;
  }

  // Check Partition signature 
  if (!check_signature(MBR)) {
    DBG("FS WARN: Partition signature mismatch");
    // goto error;
  }

  get_partition_info(fs, MBR); 
  fs->sdcard = sdcard;

  // Allocate cluster map structure
  fs->clust_info = (clustmap_info_t *)calloc(1, sizeof(clustmap_info_t)); 
  if (!fs->clust_info) {
    DBG("FS ERR: Failed to allocate memory for cluter info struct");
    goto error;
  }

  fs->clust_info->map = (uint8_t *)calloc(CLUSTER_MAP_CACHED_PAGES * 512, sizeof(uint8_t));
  if (!fs->clust_info->map) {
    DBG("FS ERR: Failed to allocate memory for cluster map cache");
  }
  fs->clust_info->last_cached_page = UINT32_MAX;

  return fs;

error:
  free(fs);
  return 0;
}


FS_File_t* FS_FileOpen(FS_t *fs, const char *fn, uint8_t param) {
  // First off - create return variable 
  FS_File_t *file = calloc(1, sizeof(FS_File_t)); 
  if (!file) {
    DBG("FS ERR: failed to allocate memory in FS_FileOpen");
    goto error;
  } 
  file->fs = fs;  

  // Find file inside root folder 
  DBGF("FS: starting search for %s", fn);
  if (find_file(fs, file, fn)) {
    // If we found file, then what?
    get_file_info(fs, file);
    DBGF("FS: File found \"%s\"", fn);
    goto filefound;
  }
  // If file wasn't found 
  if (param & FS_FILE_WRITE) {
    // If we going to write, we should create new file
    DBGF("FS: Creating file \'%s\'", fn);
    if (!create_file(fs, file, fn)) goto error;
    goto filefound;
  }
  
  DBG("FS: File not found");
  // Creating cache 
filefound:
  file->cached_pg = UINT32_MAX;
  file->cache = (uint8_t *) calloc(512 * FS_FCACHE_PG, sizeof(uint8_t));
  if (!file->cache) {
    DBG("FS ERR: Malloc failed for file cache");
    goto error;
  }
  // If none specified assume it's append mode
  if (param & FS_FILE_TRUNC) {
    file->w_pos = 0;
    file->size  = 0;
  } 
  return file;
error:
  free(file); 
  return 0;
}


uint32_t FS_FileRead(FS_File_t *f, uint8_t *buffer, uint32_t len) {
  if (!f) goto error;
  // Check if cache is present, and create one 
  if (!f->cache) {
    f->cache = (uint8_t *) calloc(512 * FS_FCACHE_PG, sizeof(uint8_t)); 
    if (!f->cache) {
      DBG("FS ERR: Failed to allocate memory page cache");
      return 0;
    }
  }

  uint32_t diff = f->size - f->r_pos;
  uint32_t left = diff > len ? len : diff;
  uint32_t counter = 0;
  do {
    uint32_t cur = f->r_pos;
    offset_t cur_of = calculate_offset(f->fs, cur, 1);
    if (!read_file_cache(f, cur_of.pg_abs)) goto error;
    uint32_t to_read = left > 512 ? 512 - cur_of.ps : left;
    memcpy(buffer, f->cache + cur_of.ps, to_read);
    left -= to_read;
    buffer+= to_read;
    f->r_pos += to_read;
    counter += to_read;
  } while(left);

  return counter;
error:
  DBG("FS ERR: Failed to read file");
  return 0;
}


uint32_t FS_FileWrite(FS_File_t *f, uint8_t *data, uint32_t len) {
  if(!f->cache) {
    DBG("FS ERR: No cache to write to");
    return 0;
  } 
  if (!len) {
    DBG("FS ERR: No data to write");
    return 0;
  }
  uint32_t w_pos_before = f->w_pos; // Used to revert write position in case of fail
  // Left to write 
  uint32_t left = len;
  uint32_t counter = 0;
  // Write to end of file 
  f->w_pos = f->size;
  do {
    // At first we need to read page it which text will be written
    offset_t of = calculate_offset(f->fs, f->w_pos, 1);
    if (!read_file_cache(f, of.pg_abs)) goto error; 
    uint32_t to_write = left > 512 ? 512 - of.ps : left;
    memcpy(f->cache + of.ps, data, to_write);
    data += to_write;
    f->w_pos += to_write;
    counter += to_write;
    left -= to_write;
    w_pos_before = f->w_pos;
  } while (left);

  return 1;
error:
  DBG("FS ERR: Write failed");
  f->w_pos = w_pos_before;
  return 0;
}



//#####################################//
// *** Static function definition *** //
//###################################//

static uint8_t create_file(FS_t *fs, FS_File_t *f, const char *n) {
  if (!strlen(n)) {
    DBG("FS ERR: How you think you will create file with E M P T Y name dumbass?");
    return 0;
  }
  uint8_t lfn_count = 0;
  uint8_t *sfn = 0;
  uint8_t *lfn = 0;
  uint8_t *data = construct_names(n, &lfn_count);
  if (!data) {
    DBG("FS ERR: Failed to create file");
    return 0;
  }
  sfn = data;
  if (lfn_count) lfn = &data[32];
  // Next one is to calculate free cluster and free position to fill info
  uint32_t entry_pos = find_empty_entry_pos(fs, 1 + lfn_count);
  // if (!entry_pos) return 0;
  uint32_t cluster_pos = find_empty_cluster(fs);
  if (!cluster_pos) return 0;
  DBGF("FS: Suitable pos for %u LFNs is %u", lfn_count + 1, entry_pos);
  DBGF("FS: Suitable cluster %u", cluster_pos);
  // Now just add cluster positions to SFN and we clear to go)
  ((SFN_t *)sfn)->clustLO = cluster_pos & 0xFFFF;
  ((SFN_t *)sfn)->clustHI = (cluster_pos & 0xFFFF0000) >> 16;
  // Finally - write entry to current cache
  write_entry(fs, sfn, entry_pos + lfn_count);
  for(int i = 1; i < lfn_count + 1; i++) {
    write_entry(fs, &lfn[(i - 1) * 32], (entry_pos + lfn_count) - i);
  }
  DBGC(rootDirCache, 512);
  // And then just sync in, it's so easy you know) 
  sync_rood_dir(fs);
  // As we got mark a data cluster, we should also write 
  write_cluster_value(fs, cluster_pos, 0x0FFFFFFF);
  sync_cluster_map(fs);
  // Write data to sturct
  f->fs = fs;
  f->cluster = cluster_pos;
  f->en_pos = entry_pos + lfn_count;
  // Never spaghetti
  free(data);
  return 0;
}


// Tries to find a file inside root folder, returns non zero if success, and writes entry position 
// in FS_File_t *f->en_pos
static uint8_t find_file(FS_t *fs, FS_File_t *f, const char *fn) {
  uint8_t *entry;
  uint32_t en = 0;

  while (1) {
    entry = read_entry(fs, en);
    if (!entry) {
      DBG("FS ERR: find_file failed");
      return 0;
    }
    // Not we check entry if it is a file entry
    uint8_t res = get_entry_info(entry);
    if (res == ENTRY_NOENTRY) {
      DBG("FS: No more entries in root");
      return 0;
    } 
    if (res != ENTRY_FILE) {
      en++;
      continue;
    }
    // At this point we had an entry that looks like a file 
    // Next we need to get it's name
    char *name = get_entry_name(fs, en);
    if (!name ) return 0;
    DBGF("FS: checking file - %s", name);
    if (!strcmp(name, fn)) {
      f->en_pos = en; 
      free(name);
      return 1;
    } 

    free(name);
    en++;
  }
  return 0;
}


// Read file info from entry and fills *f file struct with:
// cluster, size
static uint8_t get_file_info(FS_t *fs, FS_File_t *f) {
  uint8_t *e = read_entry(fs, f->en_pos);
  if(!e) return 0;
  f->cluster = (uint16_cast(&e[0x14]) << 16) |uint16_cast(&e[0x1A]);
  f->size = uint32_cast(&e[0x1C]); 
  DBG("FS: --File info-- ");
  DBGF("    File data cluster: %u", f->cluster);
  DBGF("    File size in bytes: %u", f->size);
  // Let's check if cluster is valid 
  uint32_t clusterValue = read_cluster_value(fs, f->cluster);
  if (!clusterValue) {
    DBG("    Warning: Cluter value of file is zero");
    DBG("    Fixing...");
    write_cluster_value(fs, f->cluster, 0x8FFFFFFF);
    sync_cluster_map(fs);
  }
  return 1;
}


// Reads file entry in root dir at *fs(filesystem) at given position (en)
static uint8_t *read_entry(FS_t *fs, uint32_t en) {
  offset_t of = calculate_offset(fs, en, 32);   // Calculate offsets for 32byte entry;  
  if (!read_root_dir(fs, of.pg_abs)) goto error;    // Make sure that cache is relevant 
  uint8_t *entry = &rootDirCache[of.ps];     
  return entry;
error: 
  DBG("FS ERR: Read enry error");
  return 0;
}


// Writes entry info into root directory cache. Use sync_root_dir to save changes
static uint32_t write_entry(FS_t *fs, uint8_t *e, uint32_t pos) {
  offset_t of = calculate_offset(fs, pos, 32);
  if(!read_root_dir(fs, of.pg_abs)) return 0;
  memcpy(rootDirCache + of.ps, e, 32); // Copy info to cache 
  rootDirChanged = 1;
  return 1;
}


// Reads root dir at (page) of filesystem(fs)
// uint32_t page should be absolute value
static uint8_t read_root_dir(FS_t *fs, uint32_t pg_abs) {
  if (pg_abs == rootDirCachedPage) return 1;    // If it cached, no need to read 

  // Check if root cache was changed before moving on
  if (rootDirChanged) {
    if(!sync_rood_dir(fs)) {
      DBG("FS ERR: Failed to read dir - failed to save previous changes");
      return 0;
    }
  }
  uint32_t addr = calculate_page_address(fs, fs->rootStartClusterNumber, pg_abs);
  if (!SDCard_Read(addr, rootDirCache, 1)) {
    DBG("FS ERR: failed to read root dir");
    return 0;
  }
  // DBGH(rootDirCache, 512);
  rootDirCachedPage = pg_abs;  
  return 1;
}


// Syncs root dir cache with sdcard 
static uint8_t sync_rood_dir(FS_t *fs) {
  uint32_t addr = calculate_page_address(fs, fs->rootStartClusterNumber, rootDirCachedPage);
  if (!SDCard_Write(addr, rootDirCache, 1)) {
    DBG("FS ERR: Failed to write cache");
    return 0;
  }
  rootDirChanged = 0;
  DBG("FS: Root dir synced");
  return 1;
}


static uint8_t get_entry_info(uint8_t *en) {
  if (en[0x0] == 0x0) return ENTRY_NOENTRY;
  if (en[0x0] == 0xE5) return ENTRY_ARCH;
  if (en[0x0B] != 0x20) return ENTRY_NOTFILE;

  return ENTRY_FILE;
}


// Will try to find a sequence of empty entries for writing depending of required count 
static uint32_t find_empty_entry_pos(FS_t *fs, uint32_t count) {
  uint32_t pos = 0;
  uint32_t empty_found = 0;
  while(1){
    uint8_t *e = read_entry(fs, pos);
    if(!e) return 0;
    uint8_t et = get_entry_info(e);       // Entry type
    if (et == ENTRY_ARCH || et == ENTRY_NOENTRY) {
      empty_found++;      // Increase if suitable found  
    } else {
      empty_found = 0;    // This means that sequence is over
    }
    if(empty_found == count) {
      return pos - empty_found + 1;  // Return first position of suitable sequence
    }
    pos ++;
  } 
  return 0;   // Not really needed
}


static uint8_t read_file_cache(FS_File_t *f, uint32_t pg_abs) {
  if(pg_abs >= f->cached_pg && pg_abs < (f->cached_pg + FILE_CACHE_SZ)) {
    // DBG("FS: Page already cached");
    return 1;
  }
  memset(f->cache, 0, 512 * FILE_CACHE_SZ);   // Clear cache if we at new point 
  // Calculating offset_t for reading 
  uint32_t addr = calculate_page_address(f->fs, f->cluster, pg_abs);
  if (!addr) {
    DBG("FS ERR: Failed to calculate page address");
    return 0;
  }

  if (!SDCard_Read(addr, f->cache, 1)) {
    return 0;
  }
  f->cached_pg = pg_abs;
  return 1;
}

// Will extract name and extension "name.ext"  
// Looks for SFN and LFN. Will return pointer to allocated char array 
// BE SURE TO CALL FREE() AFTER 
static char *get_entry_name(FS_t *fs, uint32_t pos) {
  uint8_t *e = read_entry(fs, pos);
  char *name = 0;
  if (!e) {
    DBG("FS ERR: failed to read_entry at get_entry_name");
    return 0;
  }
  // First of all we should check if name has LFN
  // It appears that ~1 in sfn at 0x6 & 0x7 shows us that it has LFN's 
  // TODO: And it all comes down to a single function extract_lfn
  uint16_t c = uint16_cast(&e[0x6]);
  if (c != 0x317e) {
    // If not equal - only SFN
    name = extract_lfn(fs, pos);
    if (!name) {
      name = extract_sfn(e);
    }
  } else {
    // If equal to "~1" - Has LFN
    name = extract_lfn(fs, pos);
    if(!name) {   
      name = extract_sfn(e); // If failed, try SFN 
    }
  }
  return name;
}


// (*out) should be array with no less that 12 bytes 
static char *extract_sfn(uint8_t *e) {
  char *n = (char *) calloc(12, sizeof(char));
  if (!n) {
    DBG("FS ERR: mem alloc failed in extract_sfn");
    return 0;
  }
  uint8_t sfn[9] = {0}; // Additional zeroes at the end for safety
  uint8_t ext[4] = {0};
  
  memcpy(sfn, e, 8);
  // remove trailing spaces 
  for(int i = 7; i > -1; i--) {
    if(sfn[i] == ' ') sfn[i] = 0;
    else break;
  }
  strcat(n, (char *)sfn);
  strcat(n, ".");
  
  memcpy(ext, &e[0x8], 3);
  for(int i = 0; i < 3; i++) ext[i] += 32;
  strcat(n, (char *)ext);
  return n;
}


// Will return lfn count based on first lfn entry before sfn
static uint32_t get_lfn_count(FS_t *fs, uint32_t ep) {
  uint32_t step = 1;
  uint8_t try = 10;
  while(try--) {
    uint8_t *e = read_entry(fs, ep-step);
    if (e[0xB] != 0x0F) break;
    if (e[0xC] != 0x00) break;
    if ((e[0x0] & 0xF0) == 0x40) return (e[0x0] & 0x0F);
    if ((e[0x0] & 0x0F) != step) break;
    step++;  
  }

  return 0;
}



// Returns pointer to a string containing LFN name
static char *extract_lfn(FS_t *fs, uint32_t epos) {
  uint32_t lfn_count = get_lfn_count(fs, epos);
  // DBGF("FS: Found entry with lfn count %u", lfn_count);
  if (!lfn_count) {
    DBG("FS ERR: LFN count = 0");
    return 0;
  }
  char *n = calloc(13 * lfn_count, sizeof(char)); // Return value 
  char *l = calloc(26 * lfn_count, sizeof(char)); // Holds LFN UCS 2 characters
  if (!n || !l) {
    DBG("FS ERR: calloc failed in extract_lfn");
    return 0;
  }
  // Copy LFNs to array 
  for(int i = 0; i < lfn_count; i++) {
    uint8_t *e = read_entry(fs, epos - (i + 1));
    memcpy(l + (26 * i),       &e[0x01], 10);
    memcpy(l + (26 * i) + 10,  &e[0x0E], 12);
    memcpy(l + (26 * i) + 22,  &e[0x1C], 4);
  }
 // Now we convert UCS2 to ASCII
  for (int i = 0; i < 13 * lfn_count; i++) {
    n[i] = l[i * 2];
  }
  free(l);
  return n;
}


// Calculates address of a page. Tries to find linked clusters from startCluster 
static uint32_t calculate_page_address(FS_t *fs, uint32_t startCluster, uint32_t page) {
  uint32_t addr = 0;
  offset_t of = calculate_offset(fs, page, 512);
  if (of.cl) {
    uint32_t prev = startCluster;
    
    for (uint32_t i = 0; i < of.cl; i++) {
      uint32_t value = read_cluster_value(fs, prev);
      if (value == 0) {
        DBG("FS ERR: Cluter with zero value");
        // DBGF("FS ERR: At position: %u", prev);
        return 0;
      }
      if ((value & 0x0FFFFFFF) == 0x0FFFFFFF) break;
      prev = value;      
    }
    startCluster = prev;
  }

  addr = fs->rootLBA + (startCluster - 2) * fs->sectorsPerCluster + of.pg;
  return addr;
}


/////////////////
/// Aux and helpers

static void get_partition_info(FS_t *fs, uint8_t *p) {
  fs->sectorsPerCluster = p[0x0D];
  fs->reservedSectorCount = uint16_cast(&p[0x0E]);
  fs->numOfFatCopies = p[0x10];
  fs->numOfSectors = uint32_cast(&p[0x20]);
  fs->sectorsPerFat = uint32_cast(&p[0x024]);
  fs->rootStartClusterNumber = uint32_cast(&p[0x2C]);
  // fs->rootLBA = fs->reservedSectorCount + (fs->sectorsPerFat * fs->numOfFatCopies);
  fs->rootLBA = fs->partitionLBA + fs->reservedSectorCount + (fs->sectorsPerFat * fs->numOfFatCopies);
  fs->clusterSize = 512 * fs->sectorsPerCluster;
  fs->clusterMapLBA = fs->partitionLBA + fs->reservedSectorCount;

  #ifdef DEBUG_D 
    DBG("FS: Partition info:");
    DBGF("  Bytes per sector: %u", uint16_cast(&p[0x0B]));
    DBGF("  Sectors per Cluster: %u", fs->sectorsPerCluster);
    DBGF("  Reserved sector Count: %u", fs->reservedSectorCount);
    DBGF("  Number of FAT copies: %u", fs->numOfFatCopies);
    DBGF("  Number of Sector in Partition: %u", fs->numOfSectors);
    DBGF("  Sectors per FAT: %u", fs->sectorsPerFat);
    DBGF("  Cluster number of root: %u", fs->rootStartClusterNumber);
    DBGF("  Calculated root LBA: %u", fs->rootLBA);
    DBGF("  Cluster size in bytes: %u", fs->clusterSize);
    DBGF("  Cluster map LBA: %u", fs->clusterMapLBA);
  #endif 
}










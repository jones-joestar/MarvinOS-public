#pragma once
#include "../common.h"

typedef struct {
    uint32_t first_cluster;
    uint32_t cur_cluster;
    uint32_t size;
    uint32_t cur_offset;
    uint8_t  valid;
} fat32_file_t;

// Returns 0 on success, -1 on failure.
int fat32_init(uint32_t partition_lba);


fat32_file_t fat32_open(const char *path);


uint32_t fat32_read(fat32_file_t *f, void *buf, uint32_t nbytes);


int fat32_seek(fat32_file_t *f, uint32_t offset);

typedef struct {
    char    name[13]; 
    uint8_t is_dir;
} fat_dirent_t;


int fat32_readdir(const char *path, fat_dirent_t *buf, uint32_t max);

#include "fat32.h"
#include "ata.h"
#include "../common.h"
#include "../console/console_helper.h"

// on-disk structures
typedef struct {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;   // non-zero for FAT12/16, 0 for FAT32
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;        // non-zero for FAT12/16, 0 for FAT32
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    // FAT32 extended BPB (only valid when fat_size_16 == 0)
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
} __attribute__((packed)) bpb_t;

typedef struct {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  ntres;
    uint8_t  crt_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t acc_date;
    uint16_t cluster_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t cluster_lo;
    uint32_t size;
} __attribute__((packed)) dirent_t;

#define ATTR_DIR           0x10
#define ATTR_LFN           0x0F
#define EOC                0x0FFFFFF8u
#define FAT16_ROOT_CLUSTER 0u          // sentinel: root dir is fixed, not in a chain
#define DIRENTS_PER_SECTOR 16          // 512 / 32

// internal state 

static union {
    uint8_t  b[512];
    uint16_t w[256];
    uint32_t d[128];
} __attribute__((aligned(16))) sec;

static union {
    uint8_t  b[512];
    uint16_t w[256];
    uint32_t d[128];
} __attribute__((aligned(16))) fat_sec;
static uint32_t fat_sec_lba = 0xFFFFFFFF;

typedef struct {
    uint32_t fat_lba;
    uint32_t data_lba;
    uint32_t root_cluster;       
    uint32_t sectors_per_cluster;
    uint32_t cluster_size;
    uint32_t root_dir_lba;       // FAT16 only
    uint32_t root_dir_sectors;   // FAT16 only
    int      fat_bits;           // 16 or 32
} fs_t;

static fs_t g_fs;
static int  g_ready = 0;

// low-level helpers 

static int read_sec(uint32_t lba) {
    return ata_read_sectors(lba, 1, sec.w);
}

static uint32_t clust_lba(uint32_t cluster) {
    if (cluster < 2) return g_fs.data_lba; // should not happen for valid data clusters
    return g_fs.data_lba + (cluster - 2) * g_fs.sectors_per_cluster;
}

//cache one FAT because of contiguous clusters
static uint32_t fat_next(uint32_t cluster) {
    uint32_t lba, idx;

    if (g_fs.fat_bits == 16) {
        lba = g_fs.fat_lba + cluster / 256;
        idx = cluster % 256;
    } else {
        lba = g_fs.fat_lba + cluster / 128;
        idx = cluster % 128;
    }

    // only read if we don't already have this FAT sector cached
    if (lba != fat_sec_lba) {
        if (ata_read_sectors(lba, 1, fat_sec.w) < 0) return EOC;
        fat_sec_lba = lba;
    }

    uint32_t next;
    if (g_fs.fat_bits == 16) {
        uint16_t val = fat_sec.w[idx];
        next = (val >= 0xFFF8u) ? EOC : val;
    } else {
        next = fat_sec.d[idx] & 0x0FFFFFFFu;
    }

    return next;
}

// name helpers

static char to_upper(char c) {
    return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

static void to_83(const char *name, char out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';
    int i = 0, j = 0;
    while (name[i] && name[i] != '.' && j < 8)
        out[j++] = to_upper(name[i++]);
    if (name[i] == '.') {
        i++; j = 8;
        while (name[i] && j < 11)
            out[j++] = to_upper(name[i++]);
    }
}

// directory search 
static int scan_sector_for(const char name83[11], uint32_t *out_cluster,
                            uint32_t *out_size, uint8_t *out_attr) {
    dirent_t *ents = (dirent_t *)sec.b;
    for (int e = 0; e < DIRENTS_PER_SECTOR; e++) {
        uint8_t first = (uint8_t)ents[e].name[0];
        if (first == 0x00) return -2;        // end of directory
        if (first == 0xE5) continue;         // deleted
        if (ents[e].attr == ATTR_LFN) continue;

        const char *raw = ents[e].name;
        int match = 1;
        for (int k = 0; k < 11; k++) {
            if (to_upper(raw[k]) != name83[k]) {
                match = 0;
                break;
            }
        }

        if (match) {
            uint32_t c_hi = ents[e].cluster_hi;
            uint32_t c_lo = ents[e].cluster_lo;
            uint32_t c = c_lo;
            if (g_fs.fat_bits == 32)
                c |= (c_hi << 16);

            *out_cluster = c;
            *out_size    = ents[e].size;
            *out_attr    = ents[e].attr;
            return 0;
        }
    }
    return -1; // not in this sector, keep searching
}

static int find_in_dir(uint32_t cluster, const char name83[11],
                       uint32_t *out_cluster, uint32_t *out_size, uint8_t *out_attr) {
    // FAT16 root directory: fixed sectors
    if (g_fs.fat_bits == 16 && cluster == FAT16_ROOT_CLUSTER) {
        for (uint32_t s = 0; s < g_fs.root_dir_sectors; s++) {
            if (read_sec(g_fs.root_dir_lba + s) < 0) return -1;
            int r = scan_sector_for(name83, out_cluster, out_size, out_attr);
            if (r == 0)  return 0;   // found
            if (r == -2) return -1;  // end of directory
        }
        return -1;
    }

    // Cluster chain
    while (cluster < EOC) {
        uint32_t lba = clust_lba(cluster);
        for (uint32_t s = 0; s < g_fs.sectors_per_cluster; s++) {
            if (read_sec(lba + s) < 0) return -1;
            int r = scan_sector_for(name83, out_cluster, out_size, out_attr);
            if (r == 0)  return 0;
            if (r == -2) return -1;
        }
        cluster = fat_next(cluster); // safe: done with sec contents
    }
    return -1;
}

// directory listing helpers

static void fmt_83_name(const char base[8], const char ext[3], char out[13]) {
    int i = 0;
    int base_len = 8;
    while (base_len > 0 && base[base_len - 1] == ' ') base_len--;
    for (int j = 0; j < base_len; j++) out[i++] = base[j];
    int ext_len = 3;
    while (ext_len > 0 && ext[ext_len - 1] == ' ') ext_len--;
    if (ext_len > 0) {
        out[i++] = '.';
        for (int j = 0; j < ext_len; j++) out[i++] = ext[j];
    }
    out[i] = '\0';
}

static int scan_dir_all(uint32_t cluster, fat_dirent_t *buf, uint32_t max) {
    uint32_t count = 0;

    if (g_fs.fat_bits == 16 && cluster == FAT16_ROOT_CLUSTER) {
        for (uint32_t s = 0; s < g_fs.root_dir_sectors && count < max; s++) {
            if (read_sec(g_fs.root_dir_lba + s) < 0) return (int)count;
            dirent_t *ents = (dirent_t *)sec.b;
            for (int e = 0; e < DIRENTS_PER_SECTOR && count < max; e++) {
                uint8_t first = (uint8_t)ents[e].name[0];
                if (first == 0x00) return (int)count;
                if (first == 0xE5) continue;
                if (ents[e].attr == ATTR_LFN) continue;
                if (ents[e].name[0] == '.') continue;
                fmt_83_name(ents[e].name, ents[e].ext, buf[count].name);
                buf[count].is_dir = (ents[e].attr & ATTR_DIR) ? 1 : 0;
                count++;
            }
        }
        return (int)count;
    }

    while (cluster < EOC && count < max) {
        uint32_t lba = clust_lba(cluster);
        for (uint32_t s = 0; s < g_fs.sectors_per_cluster && count < max; s++) {
            if (read_sec(lba + s) < 0) return (int)count;
            dirent_t *ents = (dirent_t *)sec.b;
            for (int e = 0; e < DIRENTS_PER_SECTOR && count < max; e++) {
                uint8_t first = (uint8_t)ents[e].name[0];
                if (first == 0x00) return (int)count;
                if (first == 0xE5) continue;
                if (ents[e].attr == ATTR_LFN) continue;
                if (ents[e].name[0] == '.') continue;
                fmt_83_name(ents[e].name, ents[e].ext, buf[count].name);
                buf[count].is_dir = (ents[e].attr & ATTR_DIR) ? 1 : 0;
                count++;
            }
        }
        cluster = fat_next(cluster);
    }
    return (int)count;
}

// public API

int fat32_init(uint32_t partition_lba) {
    if (read_sec(partition_lba) < 0) return -1;

    bpb_t *bpb = (bpb_t *)sec.b;
    if (bpb->bytes_per_sector != 512) return -1;
    if (bpb->sectors_per_cluster == 0) return -1;

    g_fs.sectors_per_cluster = bpb->sectors_per_cluster;
    g_fs.cluster_size        = bpb->sectors_per_cluster * 512u;
    g_fs.fat_lba             = partition_lba + bpb->reserved_sectors;

    if (bpb->fat_size_16 != 0) {
        g_fs.fat_bits         = 16;
        g_fs.root_dir_lba     = g_fs.fat_lba + (uint32_t)bpb->num_fats * bpb->fat_size_16;
        g_fs.root_dir_sectors = ((uint32_t)bpb->root_entry_count * 32 + 511) / 512;
        g_fs.data_lba         = g_fs.root_dir_lba + g_fs.root_dir_sectors;
        g_fs.root_cluster     = FAT16_ROOT_CLUSTER;
    } else {
        g_fs.fat_bits     = 32;
        g_fs.data_lba     = g_fs.fat_lba + (uint32_t)bpb->num_fats * bpb->fat_size_32;
        g_fs.root_cluster = bpb->root_cluster;
    }

    g_ready = 1;

    return 0;
}

fat32_file_t fat32_open(const char *path) {
    fat32_file_t f = {0};
    if (!g_ready) return f;
    if (*path == '/') path++;

    uint32_t cur_cluster = g_fs.root_cluster; // 0 for FAT16 root

    while (*path) {
        char comp[13] = {0};
        int  len = 0;
        while (path[len] && path[len] != '/' && len < 12) {
            comp[len] = path[len];
            len++;
        }
        path += len;
        if (*path == '/') path++;

        if (comp[0] == '.' && comp[1] == '\0') continue;

        char name83[11];
        to_83(comp, name83);

        uint32_t next_cluster, fsize;
        uint8_t  attr;
        if (find_in_dir(cur_cluster, name83, &next_cluster, &fsize, &attr) < 0)
            return f;

        cur_cluster = next_cluster;

        if (*path == '\0') {
            if (attr & ATTR_DIR) return f;
            f.first_cluster = next_cluster;
            f.cur_cluster   = next_cluster;
            f.size          = fsize;
            f.cur_offset    = 0;
            f.valid         = 1;
            
            return f;
        }
        if (!(attr & ATTR_DIR)) return f;
    }
    return f;
}

int fat32_seek(fat32_file_t *f, uint32_t offset) {
    if (!g_ready || !f->valid || offset > f->size) return -1;

    uint32_t cluster   = f->first_cluster;
    uint32_t remaining = offset;

    while (remaining >= g_fs.cluster_size) {
        uint32_t next = fat_next(cluster);
        if (next < 2 || next >= EOC) break;
        cluster    = next;
        remaining -= g_fs.cluster_size;
    }

    f->cur_cluster = cluster;
    f->cur_offset  = offset;

    return 0;
}

int fat32_readdir(const char *path, fat_dirent_t *buf, uint32_t max) {
    if (!g_ready || !buf || max == 0) return -1;
    if (*path == '/') path++;

    uint32_t cluster = g_fs.root_cluster;

    while (*path) {
        char comp[13] = {0};
        int len = 0;
        while (path[len] && path[len] != '/' && len < 12) {
            comp[len] = path[len];
            len++;
        }
        path += len;
        if (*path == '/') path++;
        if (comp[0] == '.' && comp[1] == '\0') continue;

        char name83[11];
        to_83(comp, name83);
        uint32_t next_cluster, fsize;
        uint8_t attr;
        if (find_in_dir(cluster, name83, &next_cluster, &fsize, &attr) < 0) return -1;
        if (!(attr & ATTR_DIR)) return -1;
        cluster = next_cluster;
    }

    return scan_dir_all(cluster, buf, max);
}

uint32_t fat32_read(fat32_file_t *f, void *buf, uint32_t nbytes) {
    if (!g_ready || !f->valid) return 0;

    uint32_t remaining = f->size - f->cur_offset;
    if (nbytes > remaining) nbytes = remaining;

    uint8_t  *dst  = (uint8_t *)buf;
    uint32_t  done = 0;

    while (done < nbytes) {
        if (f->cur_cluster < 2) {
            Merror("F32R: invalid cluster < 2\n");
            break;
        }

        uint32_t in_cluster = f->cur_offset % g_fs.cluster_size;
        uint32_t sec_idx    = in_cluster / 512;
        uint32_t in_sec     = in_cluster % 512;

        uint32_t lba = clust_lba(f->cur_cluster) + sec_idx;
        if (read_sec(lba) < 0) break;



        uint32_t can = 512 - in_sec;
        if (can > nbytes - done) can = nbytes - done;

        memcpy(dst + done, sec.b + in_sec, can);
        done          += can;
        f->cur_offset += can;

        // in case we crossed or reached a cluster boundary
        if (f->cur_offset < f->size) {
            uint32_t current_clust_offset = f->cur_offset % g_fs.cluster_size;
            if (current_clust_offset == 0) {
                f->cur_cluster = fat_next(f->cur_cluster);
                if (f->cur_cluster < 2 || f->cur_cluster >= EOC) break;
            }
        }
    }
    return done;
}

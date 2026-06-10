#include "mbr.h"
#include "gpt.h"
#include "ata.h"

#define MBR_SIGNATURE 0xAA55

static int is_fat_type(uint8_t t) {
    return t == 0x01 || t == 0x04 || t == 0x06 ||  // FAT12/FAT16
           t == 0x0B || t == 0x0C || t == 0x0E ||  // FAT32 / FAT16 LBA
           t == 0x0EF;                             // EFI System Partition
}

typedef struct {
    uint8_t  status;
    uint8_t  chs_first[3];
    uint8_t  type;
    uint8_t  chs_last[3];
    uint32_t lba_start;
    uint32_t lba_size;
} __attribute__((packed)) mbr_entry_t;

typedef struct {
    uint8_t     boot_code[446];
    mbr_entry_t partitions[4];
    uint16_t    signature;
} __attribute__((packed)) mbr_t;

int mbr_find_all_fat32_lba(uint32_t *out, int max) {
    static uint16_t buf[256];
    if (ata_read_sectors(0, 1, buf) < 0) return 0;

    mbr_t *mbr = (mbr_t *)buf;
    if (mbr->signature != MBR_SIGNATURE) return 0;

    for (int i = 0; i < 4; i++) {
        if (mbr->partitions[i].type == 0xEE)
            return gpt_find_all_fat32_lba(out, max);
    }

    int found = 0;
    for (int i = 0; i < 4 && found < max; i++) {
        if (is_fat_type(mbr->partitions[i].type) && mbr->partitions[i].lba_start != 0)
            out[found++] = mbr->partitions[i].lba_start;
    }
    return found;
}

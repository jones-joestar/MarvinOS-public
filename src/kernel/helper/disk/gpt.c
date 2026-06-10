#include "gpt.h"
#include "ata.h"


static const uint8_t GUID_ESP[16]   = { 0x28,0x73,0x2A,0xC1, 0x1F,0xF8, 0xD2,0x11,
                                        0xBA,0x4B,0x00,0xA0,0xC9,0x3E,0xC9,0x3B };
static const uint8_t GUID_FAT32[16] = { 0xA2,0xA0,0xD0,0xEB, 0xE5,0xB9, 0x33,0x44,
                                        0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7 };

typedef struct {
    char     sig[8];          // EFI
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t my_lba;
    uint64_t alt_lba;
    uint64_t first_usable;
    uint64_t last_usable;
    uint8_t  disk_guid[16];
    uint64_t part_entry_lba;  // LBA partition entries start
    uint32_t num_entries;     // total entry slots
    uint32_t entry_size;     
    uint32_t entries_crc32;
} __attribute__((packed)) gpt_header_t;

typedef struct {
    uint8_t  type_guid[16];
    uint8_t  unique_guid[16];
    uint64_t lba_start;
    uint64_t lba_end;
    uint64_t attributes;
    uint16_t name[36];        
} __attribute__((packed)) gpt_entry_t;

static int guid_eq(const uint8_t *a, const uint8_t *b) {
    for (int i = 0; i < 16; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

int gpt_find_all_fat32_lba(uint32_t *out, int max) {
    static uint16_t buf[256];
    int found = 0;

    if (ata_read_sectors(1, 1, buf) < 0) return 0;

    gpt_header_t *hdr = (gpt_header_t *)buf;
    if (hdr->sig[0] != 'E' || hdr->sig[1] != 'F' || hdr->sig[2] != 'I' ||
        hdr->sig[3] != ' ' || hdr->sig[4] != 'P' || hdr->sig[5] != 'A' ||
        hdr->sig[6] != 'R' || hdr->sig[7] != 'T') return 0;

    if (hdr->entry_size != 128) return 0;

    uint32_t num     = hdr->num_entries;
    uint32_t lba     = (uint32_t)hdr->part_entry_lba;
    uint32_t sectors = (num + 3) / 4;

    for (uint32_t s = 0; s < sectors && found < max; s++) {
        if (ata_read_sectors(lba + s, 1, buf) < 0) break;

        gpt_entry_t *e = (gpt_entry_t *)buf;
        uint32_t in_sector = (s == sectors - 1) ? (num - s * 4) : 4;

        for (uint32_t i = 0; i < in_sector && found < max; i++) {
            if ((guid_eq(e[i].type_guid, GUID_ESP) ||
                 guid_eq(e[i].type_guid, GUID_FAT32)) &&
                e[i].lba_start != 0) {
                out[found++] = (uint32_t)e[i].lba_start;
            }
        }
    }
    return found;
}

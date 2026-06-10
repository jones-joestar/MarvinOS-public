#pragma once
#include "../common.h"

int  ata_init(void);
int  ata_read_sectors(uint32_t lba, uint8_t count, uint16_t *buf);
int  ata_write_sectors(uint32_t lba, uint8_t count, const uint16_t *buf);

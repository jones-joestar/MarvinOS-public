#pragma once
#include "../common.h"

int  ahci_init(void);
int  ahci_read_sectors(uint32_t lba, uint8_t count, uint16_t *buf);
int  ahci_write_sectors(uint32_t lba, uint8_t count, const uint16_t *buf);

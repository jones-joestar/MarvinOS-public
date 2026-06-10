#pragma once
#include "../common.h"

//return the number of partitions found
int gpt_find_all_fat32_lba(uint32_t *out, int max);

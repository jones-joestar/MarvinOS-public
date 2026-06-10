#pragma once
#include "../common.h"
#include "process.h"

void init_process_map();
void add_process(process_t *proc);
process_t* get_process(uint64_t pid);
process_t* remove_process(uint64_t pid);
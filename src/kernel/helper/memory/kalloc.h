#pragma once

#include "../common.h"

extern uint64_t allocated_pages;
extern uint64_t total_pages;

void* kalloc();
void kfree(void* frame);
void init_kalloc();
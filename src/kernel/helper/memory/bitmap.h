#pragma once
#include "../common.h"

typedef struct {
    uint64_t* data;
    uint64_t total_words;
    uint64_t last_word_index;
} bitmap_allocator_t;

void bitmap_init(bitmap_allocator_t* bmp, uint64_t* buffer, uint64_t total_words);
uint64_t bitmap_allocate(bitmap_allocator_t* bmp);
void bitmap_free(bitmap_allocator_t* bmp, uint64_t index);
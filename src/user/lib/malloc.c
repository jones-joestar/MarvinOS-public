#include "malloc.h"
#include "stdbool.h"
#include "stdint.h"
#include "syscall.h"

#define IS_FREE_FLAG 1ULL

#define ALIGN(size) ((size + 0xf) & ~0xf)
#define HEADER_SIZE (2 * sizeof(uint64_t))

typedef struct chunk {
    size_t prev_size; // size of previous chunk + header (16 bytes)
    size_t size; // chunk size + header, always 16-byte aligned. bit 0: is_free flag
    struct chunk* next_free; // next_free free chunk, only used if this chunk is free, is part of user data if allocated
    struct chunk* prev_free; // same but for previous chunk
} chunk_t;

#define SIZE(chunk) ((chunk)->size & ~0xfULL)
#define PREV_CHUNK(chunk) ((chunk_t*)((uint64_t)(chunk) - (uint64_t)((chunk)->prev_size)))
#define NEXT_CHUNK(chunk) ((chunk_t*)((uint64_t)(chunk) + (uint64_t)(SIZE(chunk))))
#define IS_FREE(chunk) ((uint64_t)chunk->size & IS_FREE_FLAG)

inline void set_alloc(chunk_t* chunk) { chunk->size &= ~IS_FREE_FLAG; }
inline void set_free(chunk_t* chunk) { chunk->size |= IS_FREE_FLAG; }

bool enlarge_heap();
void remove_from_list(chunk_t* chunk);

static void* heap_base = 0;
static void* heap_top = 0;
static chunk_t* free_list = 0;
static chunk_t* last_chunk = 0;

void* malloc(size_t size) {
    chunk_t* current = free_list;
    size = ALIGN(size + HEADER_SIZE);
    
    if (size < sizeof(chunk_t)) {
        size = sizeof(chunk_t);
    }

    while (true) {
        // if no suitable chunk is found, enlarge the heap
        if (!current) {
            if (!enlarge_heap()) {
                return 0;
            }
            current = free_list;

            if (SIZE(current) < size) {
                current = 0;
            }

            continue;
        }
        
        if (SIZE(current) >= size) {
            remove_from_list(current);

            // zero the is_free flag
            set_alloc(current);

            // split chunk if necessary
            if (SIZE(current) >= size + sizeof(chunk_t)) {
                chunk_t* split = (chunk_t*)((uint64_t)current + size);
                split->size = SIZE(current) - size;
                split->prev_size = size;
                
                if (current == last_chunk) {
                    last_chunk = split;
                } else {
                    NEXT_CHUNK(current)->prev_size = split->size;
                }

                current->size = size;
                free((void*)((uint64_t)split + HEADER_SIZE));
            }

            return (void*)((uint64_t)current + HEADER_SIZE);
        }
        
        current = current->next_free;
    }
}

void *calloc(size_t nmemb, size_t size) {
    if (nmemb > 0 && size > 0 && nmemb > (size_t)-1 / size) {
        return 0;
    }
    size_t total = nmemb * size;
    char *p = malloc(total);
    if (p) for (size_t i = 0; i < total; i++) p[i] = 0;
    return p;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return 0;
    }

    void *newp = malloc(size);
    if (newp) {
        chunk_t* old_chunk = (chunk_t*)((uint64_t)ptr - HEADER_SIZE);
        size_t old_data_size = SIZE(old_chunk) - HEADER_SIZE;
        size_t copy_size = (size < old_data_size) ? size : old_data_size;
        
        char *src = (char *)ptr, *dst = (char *)newp;
        for (size_t i = 0; i < copy_size; i++) dst[i] = src[i];
        free(ptr);
    }
    return newp;
}

void free(void *ptr) {
    if (!ptr) {
        return;
    }
    chunk_t* chunk = (chunk_t*)((uint64_t)ptr - HEADER_SIZE);

    // merge with next
    if (chunk != last_chunk && IS_FREE(NEXT_CHUNK(chunk))) {
        chunk_t* next = NEXT_CHUNK(chunk);
        chunk->size = SIZE(chunk) + SIZE(next);
        remove_from_list(next);

        if (next == last_chunk) {
            last_chunk = chunk;
        } else {
            NEXT_CHUNK(chunk)->prev_size = SIZE(chunk);
        }
    }

    // merge with previous
    if (chunk != (chunk_t*)heap_base && IS_FREE(PREV_CHUNK(chunk))) {
        chunk_t* new = PREV_CHUNK(chunk);
        remove_from_list(new);
        new->size = SIZE(new) + SIZE(chunk);

        if (chunk == last_chunk) {
            last_chunk = new;
        } else {
            NEXT_CHUNK(new)->prev_size = SIZE(new);
        }

        chunk = new;
    }

    chunk->prev_free = 0;
    chunk->next_free = free_list;
    if (free_list) {
        free_list->prev_free = chunk;
    }
    free_list = chunk;
    set_free(chunk);
}

// Removes a chunk from the free list
void remove_from_list(chunk_t* chunk) {
    if (chunk == free_list) {
        free_list = chunk->next_free;
    }

    if (chunk->prev_free) {
        chunk->prev_free->next_free = chunk->next_free;
    }

    if (chunk->next_free) {
        chunk->next_free->prev_free = chunk->prev_free;
    }
}

// Permanently enlarges the heap by 4KB. 
bool enlarge_heap() {
    if (!heap_base) {
        heap_base = sys_heap_base();
        heap_top = heap_base;
    }

    void* new_top = sys_enlarge_heap();
    if (new_top == heap_top) {
        return false;
    }

    chunk_t* new_chunk = (chunk_t*)heap_top;
    new_chunk->size = new_top - heap_top;

    if (last_chunk) {
        new_chunk->prev_size = SIZE(last_chunk);
    } else {
        new_chunk->prev_size = 0;
    }
    
    last_chunk = new_chunk;
    free((void*)(heap_top + HEADER_SIZE));
    heap_top = new_top;
    return true;
}
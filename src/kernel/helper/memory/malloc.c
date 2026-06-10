#include "malloc.h"
#include "kalloc.h"
#include "paging_helper.h"
#include "../panic/panic_helper.h"

#define IS_FREE_FLAG 1ULL

#define ALIGN(size) ((size + 0xf) & ~0xf)
#define HEADER_SIZE (2 * sizeof(uint64_t))

typedef struct chunk {
    uint64_t prev_size; // size of previous chunk + header (16 bytes)
    uint64_t size; // chunk size + header, always 16-byte aligned. bit 0: is_free flag
    struct chunk* next_free; // next_free free chunk, only used if this chunk is free, is part of user data if allocated
    struct chunk* prev_free; // same but for previous chunk
} chunk_t;

#define SIZE(chunk) ((chunk)->size & ~0xfULL)
#define PREV_CHUNK(chunk) ((chunk_t*)((uint64_t)(chunk) - (uint64_t)((chunk)->prev_size)))
#define NEXT_CHUNK(chunk) ((chunk_t*)((uint64_t)(chunk) + (uint64_t)(SIZE(chunk))))
#define IS_FREE(chunk) ((uint64_t)chunk->size & IS_FREE_FLAG)

inline void set_alloc(chunk_t* chunk) { chunk->size &= ~IS_FREE_FLAG; }
inline void set_free(chunk_t* chunk) { chunk->size |= IS_FREE_FLAG; }

void enlarge_heap();
void remove_from_list(chunk_t* chunk);

static uint64_t heap_top = HEAP_VIRT_BASE;
static chunk_t* free_list = 0;
static chunk_t* last_chunk = 0;

// Returns a 16-byte aligned pointer to a chunk of memory of the given size.
void* malloc(uint64_t size) {
    uint64_t flags = save_flags_and_cli();
    chunk_t* current = free_list;
    size = ALIGN(size + HEADER_SIZE);
    
    if (size < sizeof(chunk_t)) {
        size = sizeof(chunk_t);
    }

    while (true) {
        // if no suitable chunk is found, enlarge the heap
        if (!current) {
            enlarge_heap();
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
                
                // We are still holding flags/cli here. free() will also try to cli/sti.
                // Our save_flags_and_cli/restore_flags handles nesting correctly!
                free((void*)((uint64_t)split + HEADER_SIZE));
            }

            restore_flags(flags);
            return (void*)((uint64_t)current + HEADER_SIZE);
        }
        
        current = current->next_free;
    }
}

// Frees a chunk of memory and merges it into the free list
void free(void* ptr) {
    uint64_t flags = save_flags_and_cli();
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
    if (chunk != (chunk_t*)HEAP_VIRT_BASE && IS_FREE(PREV_CHUNK(chunk))) {
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
    restore_flags(flags);
}

// Removes a chunk from the free list
void remove_from_list(chunk_t* chunk) {
    // This is always called with interrupts disabled from malloc/free
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
void enlarge_heap() {
    // This is always called with interrupts disabled from malloc
    void* frame = kalloc();
    if (frame == ADDRESS_INVALID) {
        Manic("enlarge_heap out of memory");
    }

    if (!map_address(frame, (void*)heap_top, PAGE_PRESENT | PAGE_WRITE, true)) {
        Manic("enlarge_heap mapping failed");
    }

    chunk_t* new_chunk = (chunk_t*)heap_top;
    new_chunk->size = PAGE_SIZE;

    if (last_chunk) {
        new_chunk->prev_size = SIZE(last_chunk);
    } else {
        new_chunk->prev_size = 0;
    }
    
    last_chunk = new_chunk;
    free((void*)(heap_top + HEADER_SIZE));
    heap_top += PAGE_SIZE;
}

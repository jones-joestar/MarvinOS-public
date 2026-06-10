#pragma once
#include "../common.h"

#define USER_LOAD_BASE   ((void*)0x10000)
#define USER_STACK_TOP   ((void*)0x800000)
#define USER_STACK_PAGES 4

typedef struct {
    void* entry;
    void* stack_top;
    void *heap_base;
    uint8_t  valid;
} user_program_t;


user_program_t load_embedded_shell(void);


user_program_t load_binary_from_disk(const char *path);


user_program_t load_elf_from_disk(const char *path);


void map_stack_pages(void);


extern void jump_to_user(void* entry, void* stack_top);

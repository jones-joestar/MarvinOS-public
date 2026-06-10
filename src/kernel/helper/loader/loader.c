#include "loader.h"
#include "shell_bin.h"
#include "../memory/paging_helper.h"
#include "../memory/kalloc.h"
#include "../disk/fat32.h"
#include "../common.h"


// size bytes
static void map_code_pages(uint32_t size) {
    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < pages; i++) {
        char *phys = kalloc();
        map_address(phys,
                    (char *)(USER_LOAD_BASE + (uint64_t)i * PAGE_SIZE),
                    PAGE_USER_MODE | PAGE_WRITE, true);
    }
}



void map_stack_pages(void) {
    for (int i = 0; i < USER_STACK_PAGES; i++) {
        char *phys = kalloc();
        map_address(phys,
                    (char *)(USER_STACK_TOP - (uint64_t)(i + 1) * PAGE_SIZE),
                    PAGE_USER_MODE | PAGE_WRITE | PAGE_EXECUTE_DISABLE, true);
    }
}

user_program_t load_embedded_shell(void) {
    map_code_pages(shell_bin_len);
    memcpy((void *)USER_LOAD_BASE, shell_bin, shell_bin_len);
    map_stack_pages();
    void *heap_base = (void *)PAGE_ROUNDUP((uint64_t)USER_LOAD_BASE + shell_bin_len);
    return (user_program_t){ .entry = (void *)USER_LOAD_BASE, .stack_top = (void *)USER_STACK_TOP, .heap_base = heap_base, .valid = 1 };
}

user_program_t load_binary_from_disk(const char *path) {
    fat32_file_t f = fat32_open(path);
    if (!f.valid)
        return (user_program_t){ .valid = 0 };

    map_code_pages(f.size);
    fat32_read(&f, (void *)USER_LOAD_BASE, f.size);
    map_stack_pages();
    void *heap_base = (void *)PAGE_ROUNDUP((uint64_t)USER_LOAD_BASE + f.size);
    return (user_program_t){ .entry = (void *)USER_LOAD_BASE, .stack_top = (void *)USER_STACK_TOP, .heap_base = heap_base, .valid = 1 };
}

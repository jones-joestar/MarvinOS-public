#include "kalloc.h"
#include "paging_helper.h"
#include "../console/console_helper.h"
#include "../panic/panic_helper.h"

struct node {
    struct node* next;
};

struct node* free_list = (struct node*)0xFFFFFFFFFFFFFFFF;
uint64_t total_pages = 0;
uint64_t allocated_pages = 0;

// Allocates a zero'd 4KB physical frame. No mapping is provided. Use kfree to free.
void* kalloc() {
    uint64_t flags = save_flags_and_cli();
    if ((uint64_t)free_list == 0xFFFFFFFFFFFFFFFF) {
        Merror("kalloc out of memory");
        restore_flags(flags);
        return (char*)free_list;
    }

    struct node* page = free_list;
    free_list = free_list->next;
    allocated_pages++;
    restore_flags(flags);

    memset(page, 0, PAGE_SIZE);
    void* frame = (char*)VIRT_TO_PHYS(page);
    return frame;
}

// Frees a 4KB physical frame
void kfree(void* frame) {
    if ((uint64_t)frame % PAGE_SIZE) {
        Manic("kfree unaligned page");
    }

    // convert to virtual address
    struct node* page = (struct node*)PHYS_TO_VIRT(frame);

    uint64_t flags = save_flags_and_cli();
    struct node* head = page;
    head->next = free_list;
    allocated_pages--;
    free_list = head;
    restore_flags(flags);
}

// Initializes the free list by adding all free frames from the UEFI memory map.
void init_kalloc() {
    void* frame = 0;

    while ((frame = bump_alloc()) != (void*)0xFFFFFFFFFFFFFFFF) {
        kfree(frame);
        total_pages++;
    }

    allocated_pages = 0;

    // Mprint("kalloc initialized: \n");
    // Mprint_int(total_pages);
    // Mprint(" pages available.\n");
}
#pragma once
#include "../common.h"
#include "../memory/paging_helper.h"

#define KERNEL_STACK_REGION ((void *)STACK_VIRT_BASE)
#define STACK_SIZE (3 * 4096)

// 8KB kernel stack + 4KB unmapped padding to catch overflow
typedef struct  {
    void* top;
    void* rsp; // stack pointer
    void* frame1; // physical frames to kfree later
    void* frame2;
} kernel_stack_t;

void allocate_kernel_stack(kernel_stack_t* stack);
void free_kernel_stack(kernel_stack_t* stack);
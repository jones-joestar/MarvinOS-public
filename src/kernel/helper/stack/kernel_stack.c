#include "kernel_stack.h"
#include "../memory/bitmap.h"
#include "../memory/kalloc.h"
#include "../memory/paging_helper.h"
#include "../panic/panic_helper.h"


#define BITMAP_SIZE 16

// Allows for a maximum of 1024 separate stacks
static uint64_t buffer[BITMAP_SIZE];
static bitmap_allocator_t bitmap;

void allocate_kernel_stack(kernel_stack_t* stack) {
    static bool init = false;

    if (!init) {
        bitmap_init(&bitmap, buffer, BITMAP_SIZE);
        init = true;
    }

    // allocate virtual space for the stack
    uint64_t index = bitmap_allocate(&bitmap);
    if (index == ADDRESS_INVALID) {
        stack->top = (void*)index;
        return;
    }

    stack->top = KERNEL_STACK_REGION + ((index + 1) * STACK_SIZE);
    stack->rsp = stack->top;

    // allocate physical frames
    if ((stack->frame1 = kalloc()) == ADDRESS_INVALID) {
        stack->top = ADDRESS_INVALID;
        bitmap_free(&bitmap, index);
        Manic("kalloc failed in allocate_kernel_stack\n");
        return;
    }

    if ((stack->frame2 = kalloc()) == ADDRESS_INVALID) {
        stack->top = ADDRESS_INVALID;
        bitmap_free(&bitmap, index);
        kfree((void*)stack->frame1);
        Manic("kalloc failed in allocate_kernel_stack\n");
        return;
    }

    // map the stack
    if (!map_address((void*)stack->frame1, (void*)stack->top - PAGE_SIZE, PAGE_WRITE | PAGE_EXECUTE_DISABLE, true)) {
        stack->top = ADDRESS_INVALID;
        bitmap_free(&bitmap, index);
        kfree((void*)stack->frame1);
        kfree((void*)stack->frame2);
        Manic("map_address1 failed in allocate_kernel_stack\n");
        return;
    }

    if (!map_address((void*)stack->frame2, (void*)stack->top - (2 * PAGE_SIZE), PAGE_WRITE | PAGE_EXECUTE_DISABLE, true)) {
        unmap_address((void*)stack->top - PAGE_SIZE);
        stack->top = ADDRESS_INVALID;
        bitmap_free(&bitmap, index);
        kfree((void*)stack->frame1);
        kfree((void*)stack->frame2);
        Manic("map_address2 failed in allocate_kernel_stack\n");
        return;
    }
}

void free_kernel_stack(kernel_stack_t* stack) {
    if (stack->top == ADDRESS_INVALID) {
        return;
    }

    unmap_address((void*)stack->top - PAGE_SIZE);
    unmap_address((void*)stack->top - (2 * PAGE_SIZE));

    bitmap_free(&bitmap, (stack->top - KERNEL_STACK_REGION) / STACK_SIZE - 1);
    kfree((void*)stack->frame1);
    kfree((void*)stack->frame2);
    stack->frame1 = ADDRESS_INVALID;
    stack->frame2 = ADDRESS_INVALID;
    stack->top = ADDRESS_INVALID;
    stack->rsp = ADDRESS_INVALID;
}

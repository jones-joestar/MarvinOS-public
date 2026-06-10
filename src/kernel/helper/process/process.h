#pragma once
#include "../common.h"
#include "../stack/kernel_stack.h"
#include "../memory/paging_helper.h"
#include "../disk/fat32.h"
#include "../keyboard/keyboard_helper.h"

#define MAX_FILE_DESCRIPTORS 8

enum file_type {
    FD_NONE,
    FD_FAT32,
    FD_PRELOADED
};

typedef struct {
    const uint8_t *data;
    uint32_t size;
    uint32_t offset;
} preloaded_fd_t;

typedef struct {
    enum file_type type;
    union {
        fat32_file_t    *file;
        preloaded_fd_t  *preloaded;
    };
} file_descriptor_t;

enum process_state {
    NEW,
    READY,
    RUNNING,
    WAITING,
    WAITING_CHILD,
    TERMINATED
};

typedef struct process {
    uint64_t PID;
    enum process_state state;
    pte_t* pml4; // page table
    
    kernel_stack_t kernel_stack;
    
    // Floating point registers
    __attribute__((aligned(16))) uint8_t fpu_state[512];

    //heap
    void* heap_base;
    void* heap_top;

    // file descriptors
    file_descriptor_t fd[MAX_FILE_DESCRIPTORS];
    ring_buffer_t* stdin;

    uint64_t wakeup_time;

    struct process* next;
    struct process* prev;

    struct process* parent;
} process_t;

// Struct for storing general purpose registers on the stack
typedef struct pt_regs {
    uint64_t rax;         // Syscall number
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;         // Arg 3
    uint64_t rsi;         // Arg 2
    uint64_t rdi;         // Arg 1
    uint64_t rbp;
    uint64_t r8;          // Arg 5
    uint64_t r9;          // Arg 6
    uint64_t r10;         // Arg 4
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} pt_regs_t;

extern process_t proc0;
extern process_t *running_process;
extern process_t *fg_process;

void init_processes();
process_t *create_process(char *program, bool fg);
void terminate_process(process_t *dying);
bool delete_process(process_t *proc);
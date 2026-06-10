#include "process.h"
#include "process_map.h"
#include "../scheduler/scheduler.h"
#include "../memory/malloc.h"
#include "../memory/bitmap.h"
#include "../stack/kernel_stack.h"
#include "../loader/loader.h"
#include "../console/console_helper.h"
#include "../memory/paging_helper.h"
#include "../scheduler/dispatcher.h"
#include "../panic/panic_helper.h"
#include "../gop/gop_helper.h"
#include "../keyboard/keyboard_helper.h"

#define PID_BITMAP_SIZE 16

extern void cpu_idle();
extern void* init_pt_regs(void* rsp0, void* rsp3, void* rip); // defined in init_pt_regs.asm
extern void* init_kernel_pt_regs(void* rsp0, void* rip); // defined in init_pt_regs.asm
extern char stack_top[]; // defined in kernel_entry.asm

void init_FPU();

static uint64_t buffer[PID_BITMAP_SIZE];
static bitmap_allocator_t PID_bitmap;

process_t proc0; // dummy process, is later used for halting the cpu
process_t *fg_process; // only the foreground process will receive keyboard input

uint8_t default_fpu_state[512] __attribute__((aligned(16)));

// Initializes the process module
void init_processes() {
    bitmap_init(&PID_bitmap, buffer, PID_BITMAP_SIZE);
    init_FPU();
    init_process_map();

    // start with a dummy process that we can context switch from
    proc0.PID = 0;
    proc0.state = RUNNING; // process 0 is used for idling
    proc0.pml4 = get_kernel_pml4();
    memcpy(proc0.fpu_state, default_fpu_state, 512);
    proc0.next = NULL;
    proc0.prev = NULL;
    proc0.parent = NULL;

    proc0.stdin = (ring_buffer_t*)malloc(sizeof(ring_buffer_t));
    memset(proc0.stdin, 0, sizeof(ring_buffer_t));

    proc0.kernel_stack.top = stack_top - 2048;
    proc0.kernel_stack.rsp = init_kernel_pt_regs(proc0.kernel_stack.top, cpu_idle);

    running_process = &proc0;
    fg_process = &proc0;
}

// Creates a new process.
process_t *create_process(char *program, bool fg) {
    process_t *new_process = (process_t*)malloc(sizeof(process_t));
    memset(new_process, 0, sizeof(process_t));

    new_process->PID = bitmap_allocate(&PID_bitmap) + 1; // PID starts from 1
    new_process->state = NEW;
    memcpy(new_process->fpu_state, default_fpu_state, 512);

    allocate_kernel_stack(&(new_process->kernel_stack));

    char path[256];
    int pi = 0;
    while (pi < 255 && program[pi]) { path[pi] = program[pi]; pi++; }
    path[pi] = '\0';

    new_process->pml4 = create_page_table();
    
    // Temporarily switch to the new process's page table.
    // We update running_process->pml4 so that the scheduler restores the correct
    // page table if it context switches away and back during load_elf_from_disk.
    void *old_pml4 = running_process->pml4;
    running_process->pml4 = new_process->pml4;
    load_page_table(new_process->pml4);

    user_program_t prog = load_elf_from_disk(path);

    // Restore the original page table.
    running_process->pml4 = old_pml4;
    load_page_table(old_pml4);
    
    if (!prog.valid) {
        Merror("failed to load program");
        destroy_page_table(new_process->pml4);
        free_kernel_stack(&(new_process->kernel_stack));
        bitmap_free(&PID_bitmap, new_process->PID - 1);
        free(new_process);
        return NULL;
    }

    new_process->stdin = (ring_buffer_t*)malloc(sizeof(ring_buffer_t));
    memset(new_process->stdin, 0, sizeof(ring_buffer_t));
    
    // the file descriptors are empty
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        new_process->fd[i].type = FD_NONE;
        new_process->fd[i].file = NULL;
    }
    
    new_process->wakeup_time = 0;

    new_process->heap_base = prog.heap_base;
    new_process->heap_top  = prog.heap_base;
    new_process->kernel_stack.rsp = init_pt_regs(new_process->kernel_stack.top, prog.stack_top, prog.entry);
    
    new_process->next = NULL;
    new_process->prev = NULL;

    add_process(new_process);
    new_process->parent = running_process;

    // only the fg process may pass on its status
    if (fg && fg_process == running_process) {
        fg_process = new_process;
        kb_event_flush();
    }

    new_process->state = READY;
    push(&ready_queue, new_process);
    return new_process;
}

// Terminates the dying process
void terminate_process(process_t *dying) {
    uint64_t flags = save_flags_and_cli();
    dying->state = TERMINATED;

    if (dying->prev) dying->prev->next = dying->next;
    if (dying->next) dying->next->prev = dying->prev;

    if (ready_queue.head == dying) ready_queue.head = dying->next;
    if (ready_queue.tail == dying) ready_queue.tail = dying->prev;

    if (sleep_queue.head == dying) sleep_queue.head = dying->next;
    if (sleep_queue.tail == dying) sleep_queue.tail = dying->prev;

    if (disk_wait_queue.head == dying) disk_wait_queue.head = dying->next;
    if (disk_wait_queue.tail == dying) disk_wait_queue.tail = dying->prev;

    if (kb_wait_queue.head == dying) kb_wait_queue.head = dying->next;
    if (kb_wait_queue.tail == dying) kb_wait_queue.tail = dying->prev;

    if (audio_wait_queue.head == dying) audio_wait_queue.head = dying->next;
    if (audio_wait_queue.tail == dying) audio_wait_queue.tail = dying->prev;

    push(&terminated_queue, dying);

    if (fg_process == dying) {
        fg_process = dying->parent;
    }

    if (dying->parent->state == WAITING_CHILD) {
        dying->parent->state = READY;
        push(&ready_queue, dying->parent);
    }

    restore_flags(flags);
    if (running_process == dying)
        schedule(dying->kernel_stack.rsp);
}

// Destroys a process and frees its resources. Must not be called on the currently running process.
bool delete_process(process_t *proc) {
    if (proc == running_process || proc->state != TERMINATED || running_process == &proc0) {
        return false;
    }

    remove_process(proc->PID);

    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (proc->fd[i].type == FD_FAT32) {
            free(proc->fd[i].file);
            proc->fd[i].file = NULL;
        }
    }

    if (proc->stdin) {
        free(proc->stdin);
        proc->stdin = NULL;
    }

    free_kernel_stack(&(proc->kernel_stack));
    unmap_user_gop(proc->pml4);
    destroy_page_table(proc->pml4);
    bitmap_free(&PID_bitmap, proc->PID - 1);
    free(proc);
    return true;
}

// Enables the FPU
// Generated by Gemini 3.1 on 2026-05-09
void init_FPU() {
    // Clear the CR0.EM (Emulation) bit, set CR0.MP (Monitor Co-processor) bit
    // Emulation bit would throw exception for FP instructions, MP bit would be needed for lazy FPU state saving
    uint64_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // Clear EM
    cr0 |= (1 << 1);  // Set MP
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));

    // Set the CR4.OSFXSR bit to enable fxsave/fxrstor
    // and CR4.OSXMMEXCPT to enable unmasked SSE exceptions TODO need to handle these
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // Set OSFXSR
    cr4 |= (1 << 10); // Set OSXMMEXCPT
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));

    // Initialize FPU
    __asm__ volatile("fninit");
    uint32_t mxcsr = 0x1F80;
    __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr));
    __asm__ volatile("fxsave (%0)" : : "r"(default_fpu_state) : "memory");
}

#include "dispatcher.h"
#include "../memory/paging_helper.h"
#include "../gdt/tss.h"
#include "../process/process.h"
#include "scheduler.h"
#include "../panic/panic_helper.h"
#include "../console/console_helper.h"

extern void jump_to_user(void* rsp0); // defined in jump_user.asm
extern void cpu_idle(); // defined in kernel.c
extern struct tss_entry tss; // defined in tss.c

static inline void fpu_save(uint8_t *fpu_state);
static inline void fpu_load(const uint8_t *fpu_state);

// Saves the current process's state and jumps to the new process
// May only be called if interrupts are disabled
void switch_context(process_t* process) {
    __asm__ volatile("cli");
    fpu_save(running_process->fpu_state); // save FPU state of current process

    running_process = process;   
    tss.rsp0 = (uint64_t)process->kernel_stack.top;
    fpu_load(process->fpu_state); // load FPU state of new process
    load_page_table(process->pml4);
    process->state = RUNNING;
    jump_to_user(process->kernel_stack.rsp);
}

// puts process 0 as the current process and halts the cpu
// there is no real context switch because we stay in the kernel
void proc0_idle() {
    __asm__ volatile("cli");
    fpu_save(running_process->fpu_state); // everything else is already preserved
    
    running_process = &proc0;
    load_page_table(proc0.pml4);
    proc0.state = RUNNING;
    tss.rsp0 = (uint64_t)proc0.kernel_stack.top;
    cpu_idle();
}

static inline void fpu_save(uint8_t *fpu_state) {
    // Save the current FPU/SSE state into the provided buffer
    __asm__ volatile("fxsave (%0)" 
                     : 
                     : "r"(fpu_state) 
                     : "memory");
}

static inline void fpu_load(const uint8_t *fpu_state) {
    // Restore the FPU/SSE state from the provided buffer
    __asm__ volatile("fxrstor (%0)" 
                     : 
                     : "r"(fpu_state) 
                     : "memory");
}

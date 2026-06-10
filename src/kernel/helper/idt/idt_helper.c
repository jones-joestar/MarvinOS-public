#include "idt_helper.h"
#include "../font/font_helper.h"
#include "../gop/gop_helper.h"
#include "../console/console_helper.h"
#include "../panic/panic_helper.h"
#include "../process/process.h"
#include "../scheduler/scheduler.h"

extern void irq5_stub(void);

static struct idt_entry idt[IDT_ENTRIES];
static struct idtr idtr;

extern void idt_flush(uint64_t idtr_ptr);

static const char *exception_names[] = {
    "Division By Zero",        // 0
    "Debug",                   // 1
    "Non-Maskable Interrupt",  // 2
    "Breakpoint",              // 3
    "Overflow",                // 4
    "Bound Range Exceeded",    // 5
    "Invalid Opcode",          // 6
    "Device Not Available",    // 7
    "Double Fault",            // 8
    "Unknown",                 // 9
    "Invalid TSS",             // 10
    "Segment Not Present",     // 11
    "Stack Fault",             // 12
    "General Protection Fault",// 13
    "Page Fault",              // 14
};

void isr_handler(uint64_t vector, uint64_t errcode){
    if (vector < 15) {
        Merror(exception_names[vector]);
    } else {
        Merror("Unknown Exception");
    }

    Mprint("  vector="); Mprint_hex(vector);
    Mprint("  errcode="); Mprint_hex(errcode);
    Mprint("\n");

    /* page fault: CR2 holds the faulting virtual address */
    if (vector == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        Mprint("  cr2="); Mprint_hex(cr2); Mprint("\n");

        Mprint("  fault: ");
        if (!(errcode & 1)) Mprint("[not-present] ");
        if   (errcode & 2)  Mprint("[write] ");
        else                Mprint("[read] ");
        if   (errcode & 4)  Mprint("[user] ");
        else                Mprint("[supervisor] ");
        if   (errcode & 8)  Mprint("[reserved-bit] ");
        if   (errcode & 16) Mprint("[instr-fetch] ");
        Mprint("\n");
    }

    Mprint("System halted.\n");
    while (1) {}
}

void page_fault_handler(pt_regs_t *regs, uint64_t errcode) {
    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

    Merror("Page Fault Exception");
    if (running_process) {
        Mprint("  PID="); Mprint_hex(running_process->PID);
    }
    Mprint("  RIP="); Mprint_hex(regs->rip);
    Mprint("  CR2="); Mprint_hex(cr2);
    Mprint("  errcode="); Mprint_hex(errcode);
    Mprint("\n");

    Mprint("  fault details: ");
    if (!(errcode & 1)) Mprint("[not-present] ");
    if   (errcode & 2)  Mprint("[write] ");
    else                Mprint("[read] ");
    if   (errcode & 4)  Mprint("[user] ");
    else                Mprint("[supervisor] ");
    if   (errcode & 8)  Mprint("[reserved-bit] ");
    if   (errcode & 16) Mprint("[instr-fetch] ");
    Mprint("\n");

    if ((errcode & 4) && running_process && running_process->PID > 0) {
        Mprint("Terminating current process and scheduling next...\n");
        running_process->kernel_stack.rsp = regs;
        terminate_process(running_process);
    } else {
        Manic("Kernel Page Fault. System halted.");
    }
}

void isr_default_handler(void){
    //ignore
    //TODO?
    Manic("irs_default_handler got called, idk why, it is in idt_helper.c. help please!!");
}

void idt_set_entry(uint8_t vector, void *handler){
    uint64_t addr = (uint64_t)handler;

    idt[vector].offset_1 = (uint16_t)(addr & 0xFFFF);
    idt[vector].selector = 0x08;
    idt[vector].ist = 0;
    idt[vector].type_attributes = IDT_INTERRUPT_GATE;
    idt[vector].offset_2 = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vector].offset_3 = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[vector].zero = 0;
}

void idt_init(void) {
    for (int i = 0; i < IDT_ENTRIES; i++){
        idt_set_entry(i, isr_default);
    }


    // Exceptions ohne Error Code
    idt_set_entry(0,  isr0);
    idt_set_entry(1,  isr1);
    idt_set_entry(2,  isr2);
    idt_set_entry(3,  isr3);
    idt_set_entry(4,  isr4);
    idt_set_entry(5,  isr5);
    idt_set_entry(6,  isr6);
    idt_set_entry(7,  isr7);

    // Exceptions mit Error Code
    idt_set_entry(8,  isr8);
    idt_set_entry(10, isr10);
    idt_set_entry(11, isr11);
    idt_set_entry(12, isr12);
    idt_set_entry(13, isr13);
    idt_set_entry(14, isr14);

    // Hardware IRQs (PIC master offset 0x20 = 32)
    idt_set_entry(37, irq5_stub);  // IRQ5 = SB16

    idtr.size = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;

    idt_flush((uint64_t)&idtr);
}
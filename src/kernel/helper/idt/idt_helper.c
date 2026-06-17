#include "idt_helper.h"
#include "../font/font_helper.h"
#include "../gop/gop_helper.h"
#include "../console/console_helper.h"
#include "../panic/panic_helper.h"
#include "../process/process.h"
#include "../scheduler/scheduler.h"
#include "../serial/serial.h"

static void serial_hex(uint64_t v) {
    char buf[19] = "0x0000000000000000";
    for (int i = 15; i >= 0; i--)
        buf[2 + (15 - i)] = "0123456789abcdef"[(v >> (i * 4)) & 0xf];
    serial_write(buf);
}

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

void isr_handler(uint64_t vector, uint64_t errcode, uint64_t rip, uint64_t rsp){
    const char *name = vector < 15 ? exception_names[vector] : "Unknown Exception";
    Merror(name);
    serial_write("[EXCEPTION] "); serial_write(name);
    serial_write("  vec="); serial_hex(vector);
    serial_write("  err="); serial_hex(errcode);
    serial_write("  RIP="); serial_hex(rip);
    serial_write("  RSP="); serial_hex(rsp);
    if (running_process) { serial_write("  PID="); serial_hex(running_process->PID); }
    serial_write("\n");

    Mprint("  vector="); Mprint_hex(vector);
    Mprint("  errcode="); Mprint_hex(errcode);
    Mprint("\n");

    /* page fault: CR2 holds the faulting virtual address */
    if (vector == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        Mprint("  cr2="); Mprint_hex(cr2); Mprint("\n");
        serial_write("  cr2="); serial_hex(cr2); serial_write("\n");

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

    serial_write("System halted.\n");
    Mprint("System halted.\n");
    while (1) {}
}

void double_fault_handler(uint64_t errcode, uint64_t rip, uint64_t rsp) {
    serial_write("[DOUBLE FAULT]");
    serial_write("  errcode="); serial_hex(errcode);
    serial_write("  RIP="); serial_hex(rip);
    serial_write("  RSP="); serial_hex(rsp);
    if (running_process) { serial_write("  PID="); serial_hex(running_process->PID); }
    serial_write("\n");
    while (1) __asm__ volatile("hlt");
}

void page_fault_handler(pt_regs_t *regs, uint64_t errcode) {
    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

    Merror("Page Fault Exception");
    serial_write("[PAGE FAULT]  RIP="); serial_hex(regs->rip);
    serial_write("  CR2="); serial_hex(cr2);
    serial_write("  errcode="); serial_hex(errcode);
    if (running_process) {
        serial_write("  PID="); serial_hex(running_process->PID);
    }
    serial_write("\n");
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

void isr_default_handler(uint64_t rip, uint64_t rsp){
    serial_write("[EXCEPTION] Unhandled interrupt (isr_default)");
    serial_write("  RIP="); serial_hex(rip);
    serial_write("  RSP="); serial_hex(rsp);
    if (running_process) { serial_write("  PID="); serial_hex(running_process->PID); }
    serial_write("\n");
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
    idt_set_entry(8,  isr8_df);
    idt[8].ist = 1;  // switch to IST1 so handler runs even if kernel stack is dead
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
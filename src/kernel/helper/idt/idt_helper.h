#ifndef IDT_HELPER_H
#define IDT_HELPER_H

#include "../font/font_helper.h"
#include "../gop/gop_helper.h"
#include "../console/console_helper.h"
#include "../common.h"

#define IDT_INTERRUPT_GATE 0x8E
#define IDT_TRAP_GATE 0xEF
#define IDT_ENTRIES 256

struct idt_entry {
    uint16_t offset_1; //offset bits 0..15
    uint16_t selector; // a code segment selector in GDT or LDT
    uint8_t ist; // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero
    uint8_t type_attributes; // gate type, dpl, and p fields
    uint16_t offset_2; //offset bits 16..31
    uint32_t offset_3; //offset bits 32..63
    uint32_t zero;
}__attribute__((packed));

struct idtr
{
    uint16_t size;
    uint64_t base;
}__attribute__((packed));

void idt_init(void);
void idt_set_entry(uint8_t vector, void *handler);


//Exceptions without Error Code
extern void isr0(void);   // #DE Division By Zero
extern void isr1(void);   // #DB Debug
extern void isr2(void);   // #NMI
extern void isr3(void);   // #BP Breakpoint
extern void isr4(void);   // #OF Overflow
extern void isr5(void);   // #BR Bound Range
extern void isr6(void);   // #UD Invalid Opcode
extern void isr7(void);   // #NM Device Not Available

//Exceptions with error Code

extern void isr8(void);   // #DF Double Fault
extern void isr10(void);  // #TS Invalid TSS
extern void isr11(void);  // #NP Segment Not Present
extern void isr12(void);  // #SS Stack Fault
extern void isr13(void);  // #GP General Protection
extern void isr14(void);  // #PF Page Fault

extern void isr_default(void);

#endif

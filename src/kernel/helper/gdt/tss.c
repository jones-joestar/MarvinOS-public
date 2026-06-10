#include "tss.h"

struct tss_entry tss;
extern char stack_top[]; // defined in kernel.asm

void init_tss() {
    tss.reserved0 = 0;
    tss.reserved1 = 0;
    tss.reserved2 = 0;
    tss.reserved3 = 0;

    tss.rsp0 = (uint64_t)stack_top;
    tss.rsp1 = 0; // not used
    tss.rsp2 = 0; // not used
    
    tss.ist1 = 0; // interrupt stack tables. We don't need these because we don't make mistakes.
    tss.ist2 = 0; 
    tss.ist3 = 0;
    tss.ist4 = 0;
    tss.ist5 = 0;
    tss.ist6 = 0;
    tss.ist7 = 0;

    tss.iopb_offset = sizeof(struct tss_entry); // no I/O permission bitmap
}

// Fills 2 GDT entries (128 bits) with the TSS descriptor for the tss struct
void put_tss_entry(struct gdt_entry* gdt) {
    uint64_t base = (uint64_t)&tss;
    uint32_t limit = sizeof(struct tss_entry) - 1;

    struct tss_descriptor* desc = (struct tss_descriptor*)gdt;

    desc->limit_low = limit & 0xFFFF;
    desc->base_low = base & 0xFFFF;
    desc->base_mid = (base >> 16) & 0xFF;
    
    desc->type = 0x9; 
    desc->s = 0;    
    desc->dpl = 0;  // Ring 0
    desc->p = 1;    // Present
    
    desc->limit_high = (limit >> 16) & 0xF;
    desc->avl = 0;
    desc->l = 0;
    desc->db = 0;
    desc->g = 0;    // Byte granularity
    
    desc->base_high = (base >> 24) & 0xFF;
    desc->base_upper = (base >> 32) & 0xFFFFFFFF;
    desc->reserved = 0;
}
#ifndef GDT_HELPER_H
#define GDT_HELPER_H
 
#include "../common.h"


struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;  
    uint8_t  base_high;
} __attribute__((packed));

struct gdtr {
    uint16_t size;    // size of GDT in bytes - 1
    uint64_t base;    // address of first GDT entry
} __attribute__((packed));

// Segment selectors (index * 8, because each entry is 8 bytes)
#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_DATA    0x18
#define GDT_USER_CODE    0x20
#define GDT_TSS          0x28

void gdt_init(void);


extern void gdt_flush(uint64_t gdtr_ptr);

extern void load_tss(uint16_t selector);

#endif
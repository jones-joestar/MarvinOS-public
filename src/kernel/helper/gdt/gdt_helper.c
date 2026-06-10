#include "gdt_helper.h"
#include "tss.h"

// Type field (bits 0-3 of access byte)
#define SEG_DATA_RD        0x00 // Read-Only
#define SEG_DATA_RDA       0x01 // Read-Only, accessed
#define SEG_DATA_RDWR      0x02 // Read/Write
#define SEG_DATA_RDWRA     0x03 // Read/Write, accessed
#define SEG_DATA_RDEXPD    0x04 // Read-Only, expand-down
#define SEG_DATA_RDEXPDA   0x05 // Read-Only, expand-down, accessed
#define SEG_DATA_RDWREXPD  0x06 // Read/Write, expand-down
#define SEG_DATA_RDWREXPDA 0x07 // Read/Write, expand-down, accessed
#define SEG_CODE_EX        0x08 // Execute-Only
#define SEG_CODE_EXA       0x09 // Execute-Only, accessed
#define SEG_CODE_EXRD      0x0A // Execute/Read
#define SEG_CODE_EXRDA     0x0B // Execute/Read, accessed
#define SEG_CODE_EXC       0x0C // Execute-Only, conforming
#define SEG_CODE_EXCA      0x0D // Execute-Only, conforming, accessed
#define SEG_CODE_EXRDC     0x0E // Execute/Read, conforming
#define SEG_CODE_EXRDCA    0x0F // Execute/Read, conforming, accessed

// Access byte upper bits (bits 4-7)
#define SEG_PRESENT        (1 << 7)  // Bit 7: segment is valid
#define SEG_DPL_RING0      (0 << 5)  // Bits 5-6: kernel privilege
#define SEG_DPL_RING3      (3 << 5)  // Bits 5-6: user privilege
#define SEG_CODE_DATA      (1 << 4)  // Bit 4: code/data (not system)

// Composed access bytes
#define KERNEL_CODE_ACCESS  (SEG_PRESENT | SEG_DPL_RING0 | SEG_CODE_DATA | SEG_CODE_EXRD)  // 0x9A
#define KERNEL_DATA_ACCESS  (SEG_PRESENT | SEG_DPL_RING0 | SEG_CODE_DATA | SEG_DATA_RDWR)  // 0x92
#define USER_CODE_ACCESS    (SEG_PRESENT | SEG_DPL_RING3 | SEG_CODE_DATA | SEG_CODE_EXRD)  // 0xFA
#define USER_DATA_ACCESS    (SEG_PRESENT | SEG_DPL_RING3 | SEG_CODE_DATA | SEG_DATA_RDWR)  // 0xF2

// Flags nibble (upper 4 bits of byte 6)
#define GDT_FLAG_LONG_MODE (1 << 5)  // L bit: 64-bit code segment

// Helper to build a GDT entry from its components
static struct gdt_entry make_entry(uint8_t access, uint8_t flags) {
    struct gdt_entry e;
    e.limit_low       = 0;
    e.base_low        = 0;
    e.base_mid        = 0;
    e.access          = access;
    e.flags_limit_high = flags & 0xF0;  
    e.base_high       = 0;
    return e;
}


static struct gdt_entry gdt[7];
static struct gdtr gdtr;

void gdt_init(void) {
    gdt[0] = make_entry(0x00, 0x00);                          // null
    gdt[1] = make_entry(KERNEL_CODE_ACCESS, GDT_FLAG_LONG_MODE);  // kernel code
    gdt[2] = make_entry(KERNEL_DATA_ACCESS, 0x00);                // kernel data
    gdt[3] = make_entry(USER_DATA_ACCESS,   0x00);                // user data
    gdt[4] = make_entry(USER_CODE_ACCESS,   GDT_FLAG_LONG_MODE);  // user code
    
    init_tss();
    put_tss_entry(&gdt[5]);

    gdtr.size = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt;

    gdt_flush((uint64_t)&gdtr);
    load_tss(GDT_TSS);
}
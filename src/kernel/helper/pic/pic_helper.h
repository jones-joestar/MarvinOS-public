#ifndef PIC_HELPER_H
#define PIC_HELPER_H

#include "../common.h"

#define MASTER_PIC_CMD_ADDR 0x20
#define MASTER_PIC_DATA_ADDR 0x21
#define SLAVE_PIC_CMD_ADDR 0xA0
#define SLAVE_PIC_DATA_ADDR 0xA1

#define PIC_IRQ_OFFSET_MASTER 32   // IRQ0..7  → vectors 32..39
#define PIC_IRQ_OFFSET_SLAVE  40   // IRQ8..15 → vectors 40..47

// Sent to the PIC at the end of every IRQ handler — tells it we're done.
// Without this, the PIC will never fire that IRQ line again.
#define PIC_EOI 0x20

// Remap both PICs and mask all IRQs. Call before sti.
void pic_init(void);

// Must be called at the end of every IRQ handler.
// For IRQ8-15 this notifies both the slave and master PIC.
void pic_send_eoi(uint8_t irq);

// Unmask an IRQ line so it can reach the CPU (0-15).
void pic_unmask_irq(uint8_t irq);

// Mask an IRQ line so it is silenced.
void pic_mask_irq(uint8_t irq);

#endif //PIC_HELPER_H
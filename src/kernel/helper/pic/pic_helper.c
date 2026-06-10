#include "pic_helper.h"

// Initialization Command Words
#define ICW1_INIT  0x10   // start initialization sequence
#define ICW1_ICW4  0x01
#define ICW4_8086  0x01 

void pic_init(void) {
    // ICW1 start initialization on both PICs
    outb(MASTER_PIC_CMD_ADDR,  ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(SLAVE_PIC_CMD_ADDR,   ICW1_INIT | ICW1_ICW4);
    io_wait();

    // ICW2 set vector offsets so IRQs land above the 32 CPU exceptions
    outb(MASTER_PIC_DATA_ADDR, PIC_IRQ_OFFSET_MASTER);
    io_wait();
    outb(SLAVE_PIC_DATA_ADDR,  PIC_IRQ_OFFSET_SLAVE);
    io_wait();

    // ICW3 wire the two chips together: slave is on master IRQ2
    outb(MASTER_PIC_DATA_ADDR, 0x04);  // master: slave attached on IRQ2
    io_wait();
    outb(SLAVE_PIC_DATA_ADDR,  0x02);  // slave #2
    io_wait();

    // ICW4 set 8086 mode on both
    outb(MASTER_PIC_DATA_ADDR, ICW4_8086);
    io_wait();
    outb(SLAVE_PIC_DATA_ADDR,  ICW4_8086);
    io_wait();

    // Mask every IRQ line, drivers unmask what they need
    outb(MASTER_PIC_DATA_ADDR, 0xFF);
    outb(SLAVE_PIC_DATA_ADDR,  0xFF);

    // Unmask the cascade IRQ (IRQ 2) on the master PIC so slave IRQs can pass through
    pic_unmask_irq(2);
}

void pic_send_eoi(uint8_t irq) {
    // IRQ8-15 come through the slave, needs EOI too
    if (irq >= 8)
        outb(SLAVE_PIC_CMD_ADDR, PIC_EOI);
    outb(MASTER_PIC_CMD_ADDR, PIC_EOI);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port = (irq < 8) ? MASTER_PIC_DATA_ADDR : SLAVE_PIC_DATA_ADDR;
    uint8_t bit   = irq % 8;
    outb(port, inb(port) & ~(1 << bit));
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port = (irq < 8) ? MASTER_PIC_DATA_ADDR : SLAVE_PIC_DATA_ADDR;
    uint8_t bit   = irq % 8;
    outb(port, inb(port) | (1 << bit));
}

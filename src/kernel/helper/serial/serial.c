#include "serial.h"
#include "../common.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00); // disable interrupts
    outb(COM1 + 3, 0x80); // enable DLAB to set baud rate divisor
    outb(COM1 + 0, 0x01); // divisor low  = 1 → 115200 baud
    outb(COM1 + 1, 0x00); // divisor high = 0
    outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit; clears DLAB
    outb(COM1 + 2, 0xC7); // enable FIFO, clear, 14-byte threshold
    outb(COM1 + 4, 0x0B); // RTS + DTR + OUT2
}

static void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)); // wait for transmit buffer empty
    outb(COM1, (uint8_t)c);
}

void serial_write(const char *s) {
    while (*s) {
        if (*s == '\n')
            serial_putc('\r');
        serial_putc(*s++);
    }
}

#include "ata.h"
#include "ahci.h"
#include "../scheduler/scheduler.h"
#include "../pic/pic_helper.h"
#include "../idt/idt_helper.h"
#include "../console/console_helper.h"

static int use_ahci = 0;

#define ATA_DATA       0x1F0
#define ATA_SECTOR_CNT 0x1F2
#define ATA_LBA_LO     0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HI     0x1F5
#define ATA_DRIVE_HEAD 0x1F6
#define ATA_CMD        0x1F7
#define ATA_STATUS     0x1F7
#define ATA_CTRL       0x3F6

#define STATUS_BSY (1 << 7)
#define STATUS_DRQ (1 << 3)
#define STATUS_ERR (1 << 0)

#define CMD_READ  0x20
#define CMD_WRITE 0x30
#define CMD_FLUSH 0xE7

static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

//reading alt status
static void ata_delay400ns(void) {
    inb(ATA_CTRL); inb(ATA_CTRL); inb(ATA_CTRL); inb(ATA_CTRL);
}

// Returns 0 when the controller is ready, -1 on timeout or absent controller.
static int wait_not_busy(void) {
    uint8_t s = inb(ATA_STATUS);
    if (s == 0xFF) {
        return -1;
    }
    if (s == 0x00) {
        return -1;
    }
    uint32_t timeout = 1000000;
    while (inb(ATA_STATUS) & STATUS_BSY) {
        if (inb(ATA_STATUS) == 0xFF) {
            return -1;
        }
        if (--timeout == 0) {
            s = inb(ATA_STATUS);
            return -1;
        }
    }
    s = inb(ATA_STATUS);
    return 0;
}

static int disk_lock = 0;

static void acquire_disk(void) {
    while (1) {
        __asm__ volatile("cli");
        if (!disk_lock) {
            disk_lock = 1;
            __asm__ volatile("sti");
            break;
        }
        if (running_process && running_process->PID != 0) {
            sleep_on(&disk_wait_queue);
        } else {
            __asm__ volatile("sti");
            __asm__ volatile("pause");
        }
    }
}

static void release_disk(void) {
    disk_lock = 0;
    wakeup(&disk_wait_queue);
}

extern void ata_interrupt_handler(void);

void wakeup_disk_queue(void) {
    inb(ATA_STATUS); // Acknowledge interrupt at the drive
    wakeup(&disk_wait_queue);
}

static void ata_setup_lba28(uint32_t lba, uint8_t count) {
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F)); 
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LO,     (uint8_t)(lba));
    outb(ATA_LBA_MID,    (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI,     (uint8_t)(lba >> 16));
}

int ata_read_sectors(uint32_t lba, uint8_t count, uint16_t *buf) {
    if (use_ahci) return ahci_read_sectors(lba, count, buf);
    acquire_disk();

    uint32_t bsy_timeout = 1000000;
    while (1) {
        __asm__ volatile("cli");
        uint8_t s = inb(ATA_STATUS);
        if (!(s & STATUS_BSY)) {
            __asm__ volatile("sti");
            break;
        }
        if (--bsy_timeout == 0) {
            __asm__ volatile("sti");
            Mprint("[ATA] read BSY timeout before cmd, status=0x"); Mprint_hex(inb(ATA_STATUS)); Mprint("\n");
            release_disk();
            return -1;
        }
        if (running_process && running_process->PID != 0) {
            sleep_on(&disk_wait_queue);
        } else {
            __asm__ volatile("sti");
            __asm__ volatile("pause");
        }
    }

    // Enable interrupts
    outb(ATA_CTRL, 0x00);

    ata_setup_lba28(lba, count);
    outb(ATA_CMD, CMD_READ);

    for (int s = 0; s < count; s++) {
        uint32_t drq_timeout = 1000000;
        while (1) {
            __asm__ volatile("cli");
            uint8_t status = inb(ATA_STATUS);
            if (!(status & STATUS_BSY) && (status & STATUS_DRQ)) {
                __asm__ volatile("sti");
                break;
            }
            if (status & STATUS_ERR) {
                uint8_t err = inb(0x1F1);
                __asm__ volatile("sti");
                release_disk();
                return -1;
            }
            if (--drq_timeout == 0) {
                __asm__ volatile("sti");
                release_disk();
                return -1;
            }
            if (running_process && running_process->PID != 0) {
                sleep_on(&disk_wait_queue);
            } else {
                __asm__ volatile("sti");
                __asm__ volatile("pause");
            }
        }
        for (int i = 0; i < 256; i++)
            buf[s * 256 + i] = inw(ATA_DATA);
    }

    release_disk();
    return 0;
}

int ata_write_sectors(uint32_t lba, uint8_t count, const uint16_t *buf) {
    if (use_ahci) return ahci_write_sectors(lba, count, buf);
    acquire_disk();

    uint32_t bsy_timeout = 1000000;
    while (1) {
        __asm__ volatile("cli");
        uint8_t s = inb(ATA_STATUS);
        if (!(s & STATUS_BSY)) {
            __asm__ volatile("sti");
            break;
        }
        if (--bsy_timeout == 0) {
            __asm__ volatile("sti");
            release_disk();
            return -1;
        }
        if (running_process && running_process->PID != 0) {
            sleep_on(&disk_wait_queue);
        } else {
            __asm__ volatile("sti");
            __asm__ volatile("pause");
        }
    }

    
    outb(ATA_CTRL, 0x00);

    ata_setup_lba28(lba, count);
    outb(ATA_CMD, CMD_WRITE);

    for (int s = 0; s < count; s++) {
        uint32_t drq_timeout = 1000000;
        while (1) {
            __asm__ volatile("cli");
            uint8_t status = inb(ATA_STATUS);
            if (!(status & STATUS_BSY) && (status & STATUS_DRQ)) {
                __asm__ volatile("sti");
                break;
            }
            if (status & STATUS_ERR) {
                __asm__ volatile("sti");
                release_disk();
                return -1;
            }
            if (--drq_timeout == 0) {
                __asm__ volatile("sti");
                release_disk();
                return -1;
            }
            if (running_process && running_process->PID != 0) {
                sleep_on(&disk_wait_queue);
            } else {
                __asm__ volatile("sti");
                __asm__ volatile("pause");
            }
        }
        for (int i = 0; i < 256; i++)
            outw(ATA_DATA, buf[s * 256 + i]);
    }

    // Flush drive write cache
    outb(ATA_CMD, CMD_FLUSH);
    while (1) {
        __asm__ volatile("cli");
        if (!(inb(ATA_STATUS) & STATUS_BSY)) {
            __asm__ volatile("sti");
            break;
        }
        if (running_process && running_process->PID != 0) {
            sleep_on(&disk_wait_queue);
        } else {
            __asm__ volatile("sti");
            __asm__ volatile("pause");
        }
    }

    release_disk();
    return 0;
}

int ata_init(void) {

    
    outb(ATA_CTRL, 0x04);
    ata_delay400ns();
    outb(ATA_CTRL, 0x00);
    ata_delay400ns();

    outb(ATA_DRIVE_HEAD, 0xA0);
    ata_delay400ns();

    uint8_t status_before = inb(ATA_STATUS);

    if (wait_not_busy() < 0) {
        if (ahci_init() == 0) {
            use_ahci = 1;
            return 0;
        }
        return -1;
    }

    uint8_t status_after = inb(ATA_STATUS);

    
    uint8_t err = inb(0x1F1);

    
    uint8_t cyl_lo = inb(ATA_LBA_MID);
    uint8_t cyl_hi = inb(ATA_LBA_HI);
    if (cyl_lo == 0x14 && cyl_hi == 0xEB) {
    } else if (cyl_lo == 0x00 && cyl_hi == 0x00) {
    } else {
    }

    idt_set_entry(PIC_IRQ_OFFSET_SLAVE + 6, ata_interrupt_handler); 
    pic_unmask_irq(14);

    // Drain the drive's data buffer in case there's leftover data from UEFI
    if (inb(ATA_STATUS) & STATUS_DRQ) {
        for (int i = 0; i < 256; i++) inw(ATA_DATA);
    }

    return 0;
}

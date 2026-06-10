#include "ahci.h"
#include "../console/console_helper.h"
#include "../memory/paging_helper.h"
#include "../memory/kalloc.h"
#include "../timer/tsc.h"
#include "../common.h"


static inline uint32_t inl(uint16_t port) {
    uint32_t v;
    __asm__ volatile("inl %1,%0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outl(uint16_t port, uint32_t v) {
    __asm__ volatile("outl %0,%1" : : "a"(v), "Nd"(port));
}

static uint32_t pci_read(uint16_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    outl(0xCF8, 0x80000000u | ((uint32_t)bus<<16) | ((uint32_t)dev<<11)
              | ((uint32_t)fn<<8) | (off & 0xFC));
    return inl(0xCFC);
}
static void pci_write(uint16_t bus, uint8_t dev, uint8_t fn, uint8_t off, uint32_t val) {
    outl(0xCF8, 0x80000000u | ((uint32_t)bus<<16) | ((uint32_t)dev<<11)
              | ((uint32_t)fn<<8) | (off & 0xFC));
    outl(0xCFC, val);
}


typedef volatile struct {
    uint32_t clb, clbu;   
    uint32_t fb,  fbu;    
    uint32_t is;          // interrupt status  (W1C)
    uint32_t ie;
    uint32_t cmd;         // port command & status
    uint32_t _r0;
    uint32_t tfd;         // ATA shadow status/error
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;        
    uint32_t sact;
    uint32_t ci;          // command issue
    uint32_t sntf;
    uint32_t fbs;
    uint32_t _r1[11];
    uint32_t vendor[4];
} ahci_port_t;            // 0x80 bytes

typedef volatile struct {
    uint32_t cap, ghc, is, pi, vs;
    uint32_t ccc_ctl, ccc_pts, em_loc, em_ctl, cap2, bohc;
    uint8_t  _r[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    ahci_port_t ports[32];
} ahci_hba_t;



typedef struct __attribute__((packed)) {
    uint16_t opts;         
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba, ctbau;
    uint32_t _rsv[4];
} cmd_hdr_t;               // must be 32 bytes

typedef struct __attribute__((packed)) {
    uint32_t dba, dbau;
    uint32_t _rsv;
    uint32_t dbc;          // byte_count-1
} prdt_t;                  // 16 bytes

typedef struct __attribute__((packed)) {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t _rsv[48];
    prdt_t  prdt[1];       // we use exactly one PRDT per command
} cmd_table_t;             // 144 bytes, 128-byte aligned (kalloc gives 4KB alignment)


typedef struct __attribute__((packed)) {
    uint8_t type;          
    uint8_t pmport;        
    uint8_t command;
    uint8_t featurel;
    uint8_t lba0, lba1, lba2, device;
    uint8_t lba3, lba4, lba5, featureh;
    uint8_t countl, counth, icc, control;
    uint8_t _rsv[4];
} fis_h2d_t;


#define GHC_AE       (1u<<31)
#define GHC_HR       (1u<<0)

#define PCMD_ST      (1u<<0)
#define PCMD_SUD     (1u<<1)
#define PCMD_POD     (1u<<2)
#define PCMD_FRE     (1u<<4)
#define PCMD_FR      (1u<<14)
#define PCMD_CR      (1u<<15)

#define PORT_IS_TFES (1u<<30)

#define TFD_BSY      (1u<<7)
#define TFD_DRQ      (1u<<3)
#define TFD_ERR      (1u<<0)

#define ATA_READ_DMA_EXT  0x25
#define ATA_WRITE_DMA_EXT 0x35

// Module state
static ahci_hba_t *hba      = NULL;
static int         port_num = -1;

static uint64_t    cmdlist_phys;
static cmd_hdr_t  *cmdlist_virt;

static uint64_t    fis_phys;

static uint64_t    cmdtbl_phys;
static cmd_table_t *cmdtbl_virt;

// DMA bounce buffer: 1 kalloc page = 4 KB = 8 sectors per command
static uint64_t   dma_phys;
static uint8_t   *dma_virt;

// Port helpers
static void port_stop(ahci_port_t *p) {
    p->cmd &= ~PCMD_ST;
    uint32_t n = 1000000;
    while ((p->cmd & PCMD_CR) && n--);

    p->cmd &= ~PCMD_FRE;
    n = 1000000;
    while ((p->cmd & PCMD_FR) && n--);
}

static void port_start(ahci_port_t *p) {
    uint32_t n = 1000000;
    while ((p->cmd & PCMD_CR) && n--);
    p->cmd |= PCMD_FRE | PCMD_SUD | PCMD_POD;
    p->cmd |= PCMD_ST;
}

// Issue one DMA read or write command
static int ahci_do_cmd(int write, uint64_t lba, uint8_t count) {
    ahci_port_t *p = &hba->ports[port_num];

    // Wait for port idle
    uint32_t n = 5000000;
    while ((p->tfd & (TFD_BSY | TFD_DRQ)) && n--);
    if (!n) {
        return -1;
    }

    // Acknowledge any stale interrupts/errors
    p->is   = p->is;
    p->serr = p->serr;

    // Command header
    cmd_hdr_t *hdr = &cmdlist_virt[0];
    hdr->opts  = (uint16_t)(5u | (write ? (1u<<6) : 0u));
    hdr->prdtl = 1;
    hdr->prdbc = 0;
    hdr->ctba  = (uint32_t)(cmdtbl_phys & 0xFFFFFFFFu);
    hdr->ctbau = (uint32_t)(cmdtbl_phys >> 32);

    // Command table
    memset(cmdtbl_virt, 0, sizeof(*cmdtbl_virt));

    fis_h2d_t *fis = (fis_h2d_t *)cmdtbl_virt->cfis;
    fis->type    = 0x27;
    fis->pmport  = 0x80;                                 // C=1
    fis->command = write ? ATA_WRITE_DMA_EXT : ATA_READ_DMA_EXT;
    fis->device  = 0x40;                                 // LBA mode
    fis->lba0    = (uint8_t)(lba);
    fis->lba1    = (uint8_t)(lba >>  8);
    fis->lba2    = (uint8_t)(lba >> 16);
    fis->lba3    = (uint8_t)(lba >> 24);
    fis->lba4    = (uint8_t)(lba >> 32);
    fis->lba5    = (uint8_t)(lba >> 40);
    fis->countl  = count;
    fis->counth  = 0;

    cmdtbl_virt->prdt[0].dba  = (uint32_t)(dma_phys & 0xFFFFFFFFu);
    cmdtbl_virt->prdt[0].dbau = (uint32_t)(dma_phys >> 32);
    cmdtbl_virt->prdt[0].dbc  = (uint32_t)(count * 512u - 1u);

    
    __asm__ volatile("" ::: "memory");

    // Issue command slot 0
    p->ci = 1u;

    // Poll until slot 0 clears
    n = 50000000;
    while (p->ci & 1u) {
        if (p->is & PORT_IS_TFES) {            // Reset port to recover
            port_stop(p);
            p->serr = p->serr;
            port_start(p);
            return -1;
        }
        if (!--n) {
            return -1;
        }
    }

    if (p->tfd & TFD_ERR) {
        return -1;
    }

    return 0;
}



int ahci_read_sectors(uint32_t lba, uint8_t count, uint16_t *buf) {
    uint8_t *dst      = (uint8_t *)buf;
    int      remaining = count;
    uint32_t cur      = lba;

    while (remaining > 0) {
        int batch = (remaining > 8) ? 8 : remaining;
        if (ahci_do_cmd(0, cur, (uint8_t)batch) < 0) return -1;
        memcpy(dst, dma_virt, (uint32_t)(batch * 512));
        dst       += batch * 512;
        cur       += (uint32_t)batch;
        remaining -= batch;
    }
    return 0;
}

int ahci_write_sectors(uint32_t lba, uint8_t count, const uint16_t *buf) {
    const uint8_t *src      = (const uint8_t *)buf;
    int            remaining = count;
    uint32_t       cur      = lba;

    while (remaining > 0) {
        int batch = (remaining > 8) ? 8 : remaining;
        memcpy(dma_virt, src, (uint32_t)(batch * 512));
        if (ahci_do_cmd(1, cur, (uint8_t)batch) < 0) return -1;
        src       += batch * 512;
        cur       += (uint32_t)batch;
        remaining -= batch;
    }
    return 0;
}



int ahci_init(void) {

    uint64_t abar    = 0;
    uint16_t found_bus = 0;
    uint8_t  found_dev = 0, found_fn = 0;

    for (uint16_t bus = 0; bus < 256 && !abar; bus++) {
        for (uint8_t dev = 0; dev < 32 && !abar; dev++) {
            uint32_t vd = pci_read(bus, dev, 0, 0x00);
            if ((vd & 0xFFFF) == 0xFFFF) continue;

            // Bit 7 of header type = multi-function device
            uint8_t hdr_raw = (pci_read(bus, dev, 0, 0x0C) >> 16) & 0xFF;
            uint8_t max_fn  = (hdr_raw & 0x80) ? 8 : 1;

            for (uint8_t fn = 0; fn < max_fn && !abar; fn++) {
                uint32_t vid_did = pci_read(bus, dev, fn, 0x00);
                if ((vid_did & 0xFFFF) == 0xFFFF) continue;

                uint32_t cr  = pci_read(bus, dev, fn, 0x08);
                uint8_t  cls = (cr >> 24) & 0xFF;
                uint8_t  sub = (cr >> 16) & 0xFF;

                // Class 01 = Mass Storage, Subclass 06 = SATA AHCI
                if (cls != 0x01 || sub != 0x06) continue;

                uint32_t bar5 = pci_read(bus, dev, fn, 0x24) & ~0xFFFu;
                if (!bar5) {
                    continue;
                }
                abar      = bar5;
                found_bus = bus;
                found_dev = dev;
                found_fn  = fn;
            }
        }
    }

    if (!abar) { Mprint("[AHCI] no controller found\n"); return -1; }

    // Enable bus mastering + memory space
    uint32_t pcicmd = pci_read(found_bus, found_dev, found_fn, 0x04);
    pci_write(found_bus, found_dev, found_fn, 0x04, pcicmd | 0x06);

    hba = (ahci_hba_t *)PHYS_TO_VIRT(abar);

    if (hba->cap2 & 0x01) {
        hba->bohc |= 0x02;            
        tsc_delay_us(25000);                  
        uint32_t n = 2000000;
        while ((hba->bohc & 0x01) && n--);     
        if (hba->bohc & 0x10) {               
            n = 20000000;
            while ((hba->bohc & 0x10) && n--);
        }
    }

    // Enable AHCI mode
    hba->ghc |= GHC_AE;

    // Find first connected disk port
    port_num = -1;
    uint32_t pi = hba->pi;
    for (int i = 0; i < 32; i++) {
        if (!(pi & (1u << i))) continue;
        ahci_port_t *p = &hba->ports[i];
        uint32_t det = p->ssts & 0xF;
        uint32_t ipm = (p->ssts >> 8) & 0xF;
        if (det == 3 && ipm == 1) {
            port_num = i;
            break;
        }
    }
    if (port_num < 0) { Mprint("[AHCI] no active port\n"); return -1; }

    ahci_port_t *p = &hba->ports[port_num];

    
    port_stop(p);

    // Disable link power management so drive stays awake
    p->sctl = (p->sctl & ~0xF00u) | 0x300u;

    
    cmdlist_phys = (uint64_t)kalloc();
    cmdlist_virt = (cmd_hdr_t *)PHYS_TO_VIRT(cmdlist_phys);
    memset(cmdlist_virt, 0, PAGE_SIZE);

    fis_phys = (uint64_t)kalloc();
    memset((void *)PHYS_TO_VIRT(fis_phys), 0, PAGE_SIZE);

    cmdtbl_phys = (uint64_t)kalloc();
    cmdtbl_virt = (cmd_table_t *)PHYS_TO_VIRT(cmdtbl_phys);
    memset(cmdtbl_virt, 0, PAGE_SIZE);

    dma_phys = (uint64_t)kalloc();
    dma_virt = (uint8_t *)PHYS_TO_VIRT(dma_phys);
    memset(dma_virt, 0, PAGE_SIZE);

    
    cmdlist_virt[0].ctba  = (uint32_t)(cmdtbl_phys & 0xFFFFFFFFu);
    cmdlist_virt[0].ctbau = (uint32_t)(cmdtbl_phys >> 32);

    // Set port DMA base addresses
    p->clb  = (uint32_t)(cmdlist_phys & 0xFFFFFFFFu);
    p->clbu = (uint32_t)(cmdlist_phys >> 32);
    p->fb   = (uint32_t)(fis_phys & 0xFFFFFFFFu);
    p->fbu  = (uint32_t)(fis_phys >> 32);

    // Clear stale errors/interrupts
    p->serr = p->serr;
    p->is   = p->is;
    hba->is = (1u << port_num);

    
    port_start(p);
    tsc_delay_us(500000); // 500ms: give drive time to spin up / send D2H FIS

    uint32_t n = 1000000;
    while ((p->tfd & TFD_BSY) && n--);


    if (p->tfd & TFD_ERR) {
        Mprint("[AHCI] port error after start\n");
        return -1;
    }

    return 0;
}

#include "sb16.h"
#include "../timer/tsc.h"
#include "../pic/pic_helper.h"
#include "../memory/paging_helper.h"
#include "../scheduler/scheduler.h"

/* SB16 DSP ports (base 0x220) */
#define SB_RESET    0x226
#define SB_READ     0x22A
#define SB_WRITE    0x22C
#define SB_STATUS   0x22E
#define SB_STATUS16 0x22F

/* 16-bit DMA controller (channels 4-7) */
#define DMA2_MASK      0xD4
#define DMA2_MODE      0xD6
#define DMA2_FLIPFLOP  0xD8
#define DMA2_CH5_ADDR  0xC4
#define DMA2_CH5_COUNT 0xC6
#define DMA2_CH5_PAGE  0x8B

#define HALF_SAMPLES 630
#define BUF_SAMPLES  (HALF_SAMPLES * 2)

static short dma_buf[BUF_SAMPLES] __attribute__((aligned(4096)));
static int            write_pos  = 0;   /* next half to write: 0 or 1 */
static int            sb16_active = 0;
volatile int          dma_ready = 0;

// Returns 1 if the DSP write port is ready within the timeout, else 0.
static int dsp_write(uint8_t val) {
    for (int i = 0; i < 10000; i++) {
        if (!(inb(SB_WRITE) & 0x80)) {
            outb(SB_WRITE, val);
            return 1;
        }
    }
    return 0; // timed out — no SB16
}

int sb16_init(uint32_t sample_rate) {
    /* Reset DSP */
    outb(SB_RESET, 1);
    tsc_delay_us(10);
    outb(SB_RESET, 0);
    tsc_delay_us(10);

    // On hardware without SB16, port 0x22E reads 0xFF and 0x22A never gives 0xAA.
    int found = 0;
    for (int i = 0; i < 10000; i++) {
        if (inb(SB_STATUS) & 0x80) {
            if (inb(SB_READ) == 0xAA) { found = 1; break; }
        }
    }
    if (!found) return 0; // no SB16 present

    if (!dsp_write(0x41)) return 0;
    dsp_write((uint8_t)(sample_rate >> 8));
    dsp_write((uint8_t)(sample_rate & 0xFF));

    uint32_t phys = (uint32_t)KERNEL_PHYS(dma_buf);

    /* Configure 16-bit DMA channel 5, auto-init */
    outb(DMA2_MASK,      0x05);
    outb(DMA2_FLIPFLOP,  0x00);
    outb(DMA2_MODE,      0x59);   /* auto-init, read, ch5 */
    uint32_t word_addr = phys >> 1;
    outb(DMA2_CH5_ADDR,  (uint8_t)(word_addr & 0xFF));
    outb(DMA2_CH5_ADDR,  (uint8_t)((word_addr >> 8) & 0xFF));
    outb(DMA2_CH5_PAGE,  (uint8_t)((phys >> 16) & 0xFF));
    uint16_t wcount = (uint16_t)(BUF_SAMPLES - 1);
    outb(DMA2_CH5_COUNT, (uint8_t)(wcount & 0xFF));
    outb(DMA2_CH5_COUNT, (uint8_t)((wcount >> 8) & 0xFF));
    outb(DMA2_MASK,      0x01);

    uint16_t hcount = (uint16_t)(HALF_SAMPLES - 1);
    dsp_write(0xB6);
    dsp_write(0x10);
    dsp_write((uint8_t)(hcount & 0xFF));
    dsp_write((uint8_t)((hcount >> 8) & 0xFF));

    inb(SB_STATUS);
    inb(SB_STATUS16);
    pic_unmask_irq(5);

    write_pos   = 0;
    dma_ready   = 0;
    sb16_active = 1;
    return 1;
}

void sb16_irq_handler(void) {
    inb(SB_STATUS16);   /* ACK DSP — required on real HW or interrupts stop */
    pic_send_eoi(5);
    
    if (!sb16_active) return;
    
    write_pos ^= 1;
    dma_ready = 1;
    wakeup(&audio_wait_queue);
}

void sb16_fill_half(const short *src) {
    if (!sb16_active) return;
    while (!dma_ready) {
        sleep_on(&audio_wait_queue);
        if (!sb16_active) return;
    }
    dma_ready = 0;
    memcpy(dma_buf + write_pos * HALF_SAMPLES, src, HALF_SAMPLES * sizeof(short));
}


void sb16_sync(void) {
    if (!sb16_active) return;
    outb(DMA2_MASK, 0x05);  /* mask ch5 so IRQ doesn't fire while paused */
    dsp_write(0xD5);         /* pause 16-bit DMA */
    tsc_delay_us(50);
    dsp_write(0xD6);         /* resume 16-bit DMA */
    outb(DMA2_MASK, 0x01);  /* unmask ch5 */
}

void sb16_shutdown(void) {
    if (!sb16_active) return;
    sb16_active = 0;
    dsp_write(0xD5);         /* halt 16-bit DMA */
    outb(DMA2_MASK, 0x05);  /* mask DMA channel 5 */
    pic_mask_irq(5);
    dma_ready   = 0;
    wakeup(&audio_wait_queue);
}
void sb16_stop(void) {
    if (!sb16_active) return;
    outb(DMA2_MASK, 0x05);  
    dsp_write(0xD5);         /* pause 16-bit DMA */
    wakeup(&audio_wait_queue);
}

void sb16_start(void) {
    if (!sb16_active) return;
    memset(dma_buf, 0, sizeof(dma_buf));
    write_pos = 0;
    dma_ready = 0;
    outb(DMA2_MASK, 0x01);  /* unmask channel 5 */
    dsp_write(0xD6);         
}

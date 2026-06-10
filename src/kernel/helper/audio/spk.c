#include "spk.h"
#include "../scheduler/scheduler.h"


#define SPK_RATE       11025
#define SPK_SCHED_DIV  110           /* 11025/110 = 100 Hz scheduler */
#define SPK_RING_MASK  1023          /* 1024 entries ≈ 93 ms at 11025 Hz */
#define SPK_CHUNK      315           /* one Doom tick at 11025 Hz: 22050/35/2 = 315 */

extern void init_timer(uint32_t freq_hz);
extern volatile uint64_t sched_div_limit;

static short    spk_ring[SPK_RING_MASK + 1];
static volatile int spk_rd  = 0;
static volatile int spk_wr  = 0;
static int      spk_ready   = 0;
static uint8_t  spk_shadow;
static int      spk_silent  = 1;

void spk_init(void) {
    spk_rd = spk_wr = 0;
    spk_silent = 1;
    spk_shadow = inb(0x61) & ~0x03u;
    outb(0x61, spk_shadow);
    spk_ready = 1;

    uint64_t flags = save_flags_and_cli();
    init_timer(SPK_RATE);
    sched_div_limit = SPK_SCHED_DIV;
    restore_flags(flags);
}

void spk_shutdown(void) {
    if (!spk_ready) return;
    spk_ready = 0;
    outb(0x61, spk_shadow & ~0x02u);
    
    uint64_t flags = save_flags_and_cli();
    init_timer(100);
    sched_div_limit = 1;
    restore_flags(flags);
}


void spk_fill(const short *samples, unsigned count) {
    if (!spk_ready) return;

    // Wait if the buffer already has a full chunk queued
    while (((spk_wr - spk_rd) & SPK_RING_MASK) >= SPK_CHUNK) {
        sleep_on(&audio_wait_queue);
        if (!spk_ready) return;
    }

    int any = 0;
    for (unsigned i = 0; i < count; i += 2)
        any |= samples[i];
    if (!any) return;

    for (unsigned i = 0; i < count; i += 2) {
        int next = (spk_wr + 1) & SPK_RING_MASK;
        if (next == spk_rd) break;
        spk_ring[spk_wr] = samples[i];
        spk_wr = next;
    }
}


void spk_irq_tick(void) {
    if (!spk_ready) return;

    int rd = spk_rd;
    if (rd == spk_wr) {
        if (!spk_silent) {
            spk_shadow &= ~0x02u;
            outb(0x61, spk_shadow);
            spk_silent = 1;
        }
        return;
    }
    spk_silent = 0;

    short sample = spk_ring[rd];
    spk_rd = (rd + 1) & SPK_RING_MASK;

    // Wake up any waiting processes every time we consume a sample (or every chunk)
    // To keep IRQ overhead low, we could do this every N samples, but 11kHz is manageable.
    if ((spk_rd & 63) == 0) {
        wakeup(&audio_wait_queue);
    }

    uint8_t port = (sample > 0) ? (spk_shadow | 0x02u) : (spk_shadow & ~0x02u);
    if (port != spk_shadow) {
        outb(0x61, port);
        spk_shadow = port;
    }
}

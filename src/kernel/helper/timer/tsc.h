#ifndef TSC_HELPER_H
#define TSC_HELPER_H

#include "../common.h"

// TSC frequency in Hz, set by tsc_calibrate().
extern uint64_t tsc_hz;

// Read the raw 64-bit TSC value.
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    // "=a" → EAX into lo, "=d" → EDX into hi.
    // "volatile" stops the compiler from reordering or eliminating the read.
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t system_time_ms() {
    return rdtsc() * 1000 / tsc_hz;
}

// Use the PIT to measure TSC frequency. Call once at boot, after pic_init().
// Stores the result in tsc_hz and returns it.
uint64_t tsc_calibrate(void);

// Spin-wait for at least `us` microseconds.
// Requires tsc_calibrate() to have been called first.
void tsc_delay_us(uint64_t us);

#endif // TSC_HELPER_H

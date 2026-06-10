#include "tsc.h"
#include "pit.h"

uint64_t tsc_hz = 0;

// Number of PIT ticks for a measurement window.
// PIT_HZ / 20 = 1193182 / 20 ≈ 59659 ticks ≈ 50.000 ms
#define CALIBRATION_COUNT  (PIT_HZ / 20)

uint64_t tsc_calibrate(void) {
    // Program channel 2 to count down CALIBRATION_COUNT ticks.
    pit_ch2_start((uint16_t)CALIBRATION_COUNT);

    // Snapshot TSC right after the PIT starts.
    // The counter is now running; we race the clock.
    uint64_t tsc_start = rdtsc();

    // Block until channel 2 output goes high (counter hit zero).
    pit_ch2_wait_done();

    uint64_t tsc_end = rdtsc();

    // Δtsc ticks elapsed in exactly CALIBRATION_COUNT / PIT_HZ seconds.
    // Rearranging:  tsc_hz = Δtsc * PIT_HZ / CALIBRATION_COUNT
    // The multiply happens first to preserve precision before the divide.
    tsc_hz = (tsc_end - tsc_start) * PIT_HZ / CALIBRATION_COUNT;

    return tsc_hz;
}

void tsc_delay_us(uint64_t us) {
    // Convert microseconds to TSC ticks: ticks = us * tsc_hz / 1_000_000
    uint64_t ticks = us * (tsc_hz / 1000000ULL);
    uint64_t start = rdtsc();
    while ((rdtsc() - start) < ticks) {}
}

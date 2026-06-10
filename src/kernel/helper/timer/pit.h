#ifndef PIT_HELPER_H
#define PIT_HELPER_H

#include "../common.h"

// The 8254 PIT input clock is exactly 1,193,182 Hz.
// This is 1/3 of the original IBM PC color burst frequency (3.579545 MHz).
#define PIT_HZ          1193182ULL

// Channel data ports (read/write the counter value)
#define PIT_CH0_PORT    0x40    // channel 0 — connected to IRQ 0
#define PIT_CH2_PORT    0x42    // channel 2 — connected to PC speaker

// Mode/Command register (write-only).
// Bit layout: [7:6]=channel  [5:4]=access  [3:1]=mode  [0]=BCD
#define PIT_CMD_PORT    0x43

// Command byte we send: channel 2 | lo+hi access | mode 0 | binary
//   10 11 000 0
//   ^^ channel 2
//      ^^ lo byte first, then hi byte
//           ^^^ mode 0: "interrupt on terminal count"
//                       output goes LOW when count is written,
//                       goes HIGH once counter reaches zero
//               ^ binary (not BCD)
#define PIT_CH2_ONESHOT 0xB0

// PC speaker / channel-2 gate port.
// Bit 0 (W): gate for channel 2 — set 1 to start the counter
// Bit 1 (W): speaker enable    — clear to keep it silent
// Bit 5 (R): channel 2 output  — goes HIGH when the counter hits zero
#define PIT_SPEAKER_PORT 0x61

// Program channel 2 to count down from `count` ticks and return.
// The caller is responsible for reading RDTSC before and after
// pit_ch2_wait_done() to measure elapsed TSC ticks.
void pit_ch2_start(uint16_t count);

// Spin until channel 2 output goes high (counter reached zero).
void pit_ch2_wait_done(void);

// Play a tone on the PC speaker at freq_hz for duration_ms milliseconds.
void pit_beep(uint32_t freq_hz, uint32_t duration_ms);

#endif // PIT_HELPER_H

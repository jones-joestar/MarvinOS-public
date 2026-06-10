#include "pit.h"
#include "tsc.h"

// Saved state of port 0x61 so we can restore speaker/gate after calibration.
static uint8_t pit_saved_speaker;

void pit_ch2_start(uint16_t count) {
    // Save the current speaker port state so we can restore it later.
    pit_saved_speaker = inb(PIT_SPEAKER_PORT);

    // Enable the channel 2 gate (bit 0 = 1) and silence the speaker (bit 1 = 0).
    // Without setting the gate the counter never starts.
    outb(PIT_SPEAKER_PORT, (pit_saved_speaker & ~0x02) | 0x01);

    // Tell the PIT: channel 2, lo/hi access, mode 0 (one-shot), binary.
    // Writing this command immediately drives the channel 2 output LOW,
    // which is the starting condition we poll against.
    outb(PIT_CMD_PORT, PIT_CH2_ONESHOT);

    // Write the 16-bit count: low byte first, then high byte.
    // The PIT latches both bytes and starts counting on the second write.
    outb(PIT_CH2_PORT, (uint8_t)(count & 0xFF));
    outb(PIT_CH2_PORT, (uint8_t)(count >> 8));
}

void pit_beep(uint32_t freq_hz, uint32_t duration_ms) {
    if (freq_hz == 0) return;
    uint32_t count = (uint32_t)(PIT_HZ / freq_hz);

    // 0xB6 = channel 2 | lo+hi access | mode 3 (square wave) | binary
    outb(PIT_CMD_PORT, 0xB6);
    outb(PIT_CH2_PORT, (uint8_t)(count & 0xFF));
    outb(PIT_CH2_PORT, (uint8_t)(count >> 8));

    // bit 0 = channel 2 gate on, bit 1 = speaker connected to output
    uint8_t tmp = inb(PIT_SPEAKER_PORT);
    outb(PIT_SPEAKER_PORT, tmp | 0x03);

    tsc_delay_us((uint64_t)duration_ms * 1000);

    // silence the speaker
    outb(PIT_SPEAKER_PORT, inb(PIT_SPEAKER_PORT) & ~0x03);
}

void pit_ch2_wait_done(void) {
    // Bit 5 of port 0x61 mirrors the channel 2 output.
    // It is LOW while counting and goes HIGH when the counter reaches zero.
    while (!(inb(PIT_SPEAKER_PORT) & (1 << 5))) {}

    // Restore the speaker port to whatever it was before we touched it.
    outb(PIT_SPEAKER_PORT, pit_saved_speaker);
}

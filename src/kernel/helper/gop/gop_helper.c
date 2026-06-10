#include "gop_helper.h"
#include "../bmp/bmp_helper.h"
#include "../memory/malloc.h"

uint32_t *shadow_buffer;
uint64_t gop_size;

// Allocate the shadow buffer that mirrors the GOP buffer.
// Reading from the GOP buffer is very slow because the memory is not cached.
// Instead, we write changes to both buffers and only read from
// the shadow buffer to improve the performance when scrolling.
void init_shadow_buffer() {
    gop_size = (uint64_t)gop_info.pitch * gop_info.screen_h * sizeof(uint32_t); 
    shadow_buffer = (uint32_t*)malloc(gop_size);
}

// Copy a specific range of rows from the shadow buffer to the hardware framebuffer.
void gop_swap_region(uint32_t start_row, uint32_t num_rows) {
    if (num_rows > 0) {
        uint64_t start_pixel = (uint64_t)start_row * gop_info.pitch;
        uint64_t total_pixels = (uint64_t)num_rows * gop_info.pitch;
        uint64_t total_qwords = total_pixels / 2;

        void *dst = &gop_info.fb[start_pixel];
        const void *src = &shadow_buffer[start_pixel];

        // rep repeats the instruction by the number in rcx, 
        // movsq moves a qword (64bit) from one src to dst and increments both src and dst by 8 bytes
        // This is faster than regular memcpy
        __asm__ volatile(
            "rep movsq"
            : "+D"(dst), "+S"(src), "+c"(total_qwords)
            :
            : "memory"
        );

        // copy remaining pixel if odd number (since 1 pixel = 32bit)
        if (total_pixels & 1) {
            gop_info.fb[start_pixel + total_pixels - 1] = shadow_buffer[start_pixel + total_pixels - 1];
        }
    }
}

// Copy the contents of the shadow buffer to the main buffer
void gop_swap_buffer() {
    gop_swap_region(0, gop_info.screen_h);
}

uint32_t make_color(uint8_t r, uint8_t g, uint8_t b) {
    switch (gop_info.pixel_format) {
    case 1: // BGR
        return ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    case 2: { // BitMask
        uint32_t pixel = 0;
        // find lowest set bit of each mask and shift the channel there
        if (gop_info.pixel_red_mask)   pixel |= (uint32_t)r * (gop_info.pixel_red_mask   & -gop_info.pixel_red_mask);
        if (gop_info.pixel_green_mask) pixel |= (uint32_t)g * (gop_info.pixel_green_mask & -gop_info.pixel_green_mask);
        if (gop_info.pixel_blue_mask)  pixel |= (uint32_t)b * (gop_info.pixel_blue_mask  & -gop_info.pixel_blue_mask);
        return pixel;
    }
    default: // RGB (0) or unknown
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
}

//this fun draws a single pixel at y,x with given color
void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x < gop_info.screen_w && y < gop_info.screen_h) {
        uint64_t i = y * gop_info.pitch + x;
        gop_info.fb[i] = color;
        shadow_buffer[i] = color;
    }
}

//this fun creates a rectangle at start x,y using w,h and color.
void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t row = 0; row < h; row++) {
        for (uint32_t col = 0; col < w; col++) {
            if (row == 0 || row == h - 1 || col == 0 || col == w - 1) {
                draw_pixel(x + col, y + row, color);
            }
        }
    }
}

// Shift up the entire GOP buffer
void shift_up(uint32_t height, uint32_t bottom_margin) {
    uint64_t flags = save_flags_and_cli();
    if (height == 0) return;
    if (height >= gop_info.screen_h) {
        clear_screen(0);
        return;
    }

    // The buffer is virtually contiguous, so we can shift everything in one go
    uint32_t rows_to_copy = gop_info.screen_h - height - bottom_margin;
    uint64_t copy_pixels = (uint64_t)rows_to_copy * gop_info.pitch;
    uint64_t copy_qwords = copy_pixels / 2;
    void *dst = shadow_buffer;
    const void *src = &shadow_buffer[height * gop_info.pitch];

    __asm__ volatile(
        "rep movsq"
        : "+D"(dst), "+S"(src), "+c"(copy_qwords)
        :
        : "memory"
    );

    if (copy_pixels & 1) {
        shadow_buffer[copy_pixels - 1] = shadow_buffer[height * gop_info.pitch + copy_pixels - 1];
    }

    // clear the bottom region in the shadow buffer
    uint64_t clear_pixels = (uint64_t)(gop_info.screen_h - bottom_margin - rows_to_copy) * gop_info.pitch;
    uint64_t clear_qwords = clear_pixels / 2;
    void *clear_dst = &shadow_buffer[rows_to_copy * gop_info.pitch];
    uint64_t zero = 0;

    // stosq: move a register to a memory address, here rax to clear_dst
    __asm__ volatile(
        "rep stosq"
        : "+D"(clear_dst), "+c"(clear_qwords)
        : "a"(zero)
        : "memory"
    );

    if (clear_pixels & 1) {
        shadow_buffer[rows_to_copy * gop_info.pitch + clear_pixels - 1] = 0;
    }

    // only swap the buffer above the bottom margin
    gop_swap_region(0, gop_info.screen_h - bottom_margin);
    restore_flags(flags);
}

//change the sreen to a color
void clear_screen(uint32_t color) {
    for (uint32_t y = 0; y < gop_info.screen_h; y++) {
        for (uint32_t x = 0; x < gop_info.screen_w; x++) {
            uint64_t i = y * gop_info.pitch + x;
            gop_info.fb[i] = color;
            if (shadow_buffer) {
                shadow_buffer[i] = color;
            }
        }
    }
}

// creates 15 different colors over the screen
void color_gradient() {
    uint32_t row_count = 15;
    uint32_t band_h = gop_info.screen_h / row_count;
    if (band_h == 0) band_h = 1;

    for (uint32_t y = 0; y < gop_info.screen_h; y++) {
        uint32_t band = y / band_h;
        uint32_t color = (band * 17) << 8;
        for (uint32_t x = 0; x < gop_info.screen_w; x++) {
            uint64_t i = y * gop_info.pitch + x;
            gop_info.fb[i] = color;
            if (shadow_buffer) {
                shadow_buffer[i] = color;
            }
        }
    }
}

//the goal would be to just wait and do nothing.
void sleep(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 10000; i++) {
        __asm__ volatile("nop");
    }
}

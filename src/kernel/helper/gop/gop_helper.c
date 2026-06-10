#include "gop_helper.h"
#include "../bmp/bmp_helper.h"


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
    if (x < gop_info.screen_w && y < gop_info.screen_h)
        gop_info.fb[y * gop_info.pitch + x] = color;
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

void shift_up(uint32_t height, uint32_t bottom_margin) {
    uint64_t flags = save_flags_and_cli();
    if (height == 0) return;
    if (height >= gop_info.screen_h) {
        clear_screen(0);
        return;
    }

    uint32_t rows_to_copy = gop_info.screen_h - height - bottom_margin;
    for (uint32_t y = 0; y < rows_to_copy; y++) {
        memcpy(&gop_info.fb[y * gop_info.pitch], &gop_info.fb[(y + height) * gop_info.pitch], gop_info.screen_w * sizeof(uint32_t));
    }

    // Clear the bottom part
    for (uint32_t y = rows_to_copy; y < gop_info.screen_h - bottom_margin; y++) {
        memset(&gop_info.fb[y * gop_info.pitch], 0, gop_info.screen_w * sizeof(uint32_t));
    }
    restore_flags(flags);
}

//change the sreen to a color
void clear_screen(uint32_t color) {
    for (uint32_t y = 0; y < gop_info.screen_h; y++) {
        for (uint32_t x = 0; x < gop_info.screen_w; x++) {
            gop_info.fb[y * gop_info.pitch + x] = color;
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
            gop_info.fb[y * gop_info.pitch + x] = color;
        }
    }
}

//the goal would be to just wait and do nothing.
void sleep(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 10000; i++) {
        __asm__ volatile("nop");
    }
}

#pragma once

#include "../common.h"
#include "../uefi/uefi_helper.h"

extern gop_info_t gop_info;

// Convert value to the hardware pixel for the current GOP format
uint32_t make_color(uint8_t r, uint8_t g, uint8_t b);

void draw_pixel(uint32_t x, uint32_t y, uint32_t color);
void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void shift_up(uint32_t height, uint32_t bottom_margin);
void clear_screen(uint32_t color);
void color_gradient();
void sleep(uint32_t ms);
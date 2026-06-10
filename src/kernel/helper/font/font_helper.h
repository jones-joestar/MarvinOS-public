#ifndef FONT_HELPER_H
#define FONT_HELPER_H

#include "../common.h"
#define FONT_W 8
#define FONT_H 8

void draw_char(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);
void draw_string(const char *s, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);

#endif
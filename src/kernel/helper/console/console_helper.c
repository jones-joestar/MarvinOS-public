#include "console_helper.h"
#include "font/font_helper.h"
#include "gop/gop_helper.h"
#include "serial/serial.h"

#define LINE_SPACING 2
#define LEFT_MARGIN  4
#define TOP_MARGIN   4

static uint32_t cx = LEFT_MARGIN;
static uint32_t cy = TOP_MARGIN;
static uint32_t fg_color;
static uint32_t bg_color;

static char int_buffer[21];

void console_init(uint32_t fg, uint32_t bg) {
    serial_init();
    fg_color = fg;
    bg_color = bg;
    cx = LEFT_MARGIN;
    cy = TOP_MARGIN;
}

void console_set_color(uint32_t fg, uint32_t bg) {
    fg_color = fg;
    bg_color = bg;
}

void Mclear(void) {
    clear_screen(bg_color);
    cx = LEFT_MARGIN;
    cy = TOP_MARGIN;
}

void Mscroll() {
    shift_up(FONT_H + LINE_SPACING, CONSOLE_BOTTOM_MARGIN);
}

static void newline(void) {
    cx = LEFT_MARGIN;
    if (cy + 2 * FONT_H + LINE_SPACING > gop_info.screen_h - CONSOLE_BOTTOM_MARGIN) {
        Mscroll();
    } else {
        cy += FONT_H + LINE_SPACING;
    }
}

void Mprint(const char *s) {
    serial_write(s);
    while (*s) {
        if (*s == '\n') {
            newline();
        }else if (*s == '\r') {
            cx = LEFT_MARGIN;
        }else if (*s == '\b') {
            if (cx > LEFT_MARGIN) {
                cx -= FONT_W + 1;
                draw_char('\b', cx, cy, fg_color, bg_color);
            }
        }else {
            if (cx + FONT_W > gop_info.screen_w) {
                newline();
            }
            draw_char(*s, cx, cy, fg_color, bg_color);
            cx += FONT_W + 1;
        }
        s++;
    }
}

void Merror(const char *s) {
    uint32_t prev_fg = fg_color;
    fg_color = 0x00FF0000;  // red
    Mprint("[ERROR] ");
    Mprint(s);
    Mprint("\n");
    fg_color = prev_fg;
}

// Prints a uint64_t
void Mprint_int(uint64_t i) {
    char *ptr = int_buffer;
    char *ptr1 = int_buffer;
    char tmp_char;

    if (i == 0) {
        Mprint("0");
        return;
    }

    while (i > 0) {
        *ptr++ = (char)((i % 10) + '0');
        i /= 10;
    }

    *ptr-- = '\0';

    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    Mprint(int_buffer);
}

void Mprint_hex(uint64_t i) {
    char *ptr = int_buffer;
    char *ptr1 = int_buffer;
    char tmp_char;
    const char *hex_chars = "0123456789ABCDEF";

    if (i == 0) {
        Mprint("0x0");
        return;
    }

    while (i > 0) {
        *ptr++ = hex_chars[i % 16];
        i /= 16;
    }

    *ptr-- = '\0';

    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    Mprint("0x");
    Mprint(int_buffer);
}
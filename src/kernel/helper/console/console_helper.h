#ifndef CONSOLE_HELPER_H
#define CONSOLE_HELPER_H

#include "../common.h"

#define CONSOLE_BOTTOM_MARGIN 80

void console_init(uint32_t fg, uint32_t bg);
void console_set_color(uint32_t fg, uint32_t bg);
void Mclear(void);
void Mprint(const char *s);
void Merror(const char *s);
void Mprint_int(uint64_t i);
void Mprint_hex(uint64_t i);

#endif
#ifndef PANIC_HELPER_H
#define PANIC_HELPER_H

#include "../../bootinfo.h"

void panic_init(BootInfo *bi);
void Manic(const char *msg);

#endif /* PANIC_HELPER_H */

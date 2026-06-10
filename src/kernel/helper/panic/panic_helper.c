#include "panic_helper.h"
#include "console/console_helper.h"
#include "gop/gop_helper.h"
#include "marvin_logo/marvin_logo.h"

#define PANIC_BG 0x00AA0000 
#define PANIC_FG 0x00FFFFFF

static BootInfo *panic_bi = (void *)0;

void panic_init(BootInfo *bi) {
    panic_bi = bi;
}

void Manic(const char *msg) {
    clear_screen(PANIC_BG);
    console_init(PANIC_FG, PANIC_BG);

    Mprint("##############################################\n");
    Mprint("#                                            #\n");
    Mprint("#           *** KERNEL PANIC ***             #\n");
    Mprint("#                                            #\n");
    Mprint("##############################################\n");
    Mprint("\n");
    Mprint(msg);
    Mprint("\n");

    if (panic_bi)
        draw_logo(panic_bi, 250, 180);

    while (1) {}
}

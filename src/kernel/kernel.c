#include "bootinfo.h"
#include "helper/gop/gop_helper.h"
#include "helper/memory/paging_helper.h"
#include "helper/console/console_helper.h"
#include "helper/serial/serial.h"
#include "helper/gdt/gdt_helper.h"
#include "helper/idt/idt_helper.h"
#include "helper/pic/pic_helper.h"
#include "helper/syscall/syscall.h"
#include "helper/panic/panic_helper.h"
#include "helper/keyboard/keyboard_helper.h"
#include "helper/uefi/uefi_helper.h"
#include "helper/memory/kalloc.h"
#include "helper/disk/ata.h"
#include "helper/disk/mbr.h"
#include "helper/disk/fat32.h"
#include "helper/timer/tsc.h"
#include "helper/scheduler/scheduler.h"
#include "helper/bmp/bmp_helper.h"

BootInfo* bootInfo;
gop_info_t gop_info;
uefi_memory_map_t uefi_memory_map;

extern char __kernel_start[];
extern char __kernel_end[];

void set_gop_physical(BootInfo *info);
void set_gop_virtual(BootInfo *info);
void set_memmap(BootInfo *info);
void cpu_idle();
void super_manic();
void kernel_screen(bool ok_to_be_fat, uint64_t hz, uint32_t total_pages);

// Kernel entry point
void kernel_main(BootInfo *info) {
    bootInfo = info;

    // Set up GOP with the physical Framebuffer address
    // remaps the framebuffer.
    set_gop_physical(bootInfo);

    // init serial
    serial_init();
    serial_write("MarvinOS: kernel_main entered.\n");

    init_paging();
    serial_write("Paging: active.\n");

    // frambuffer is now mapped to virtual address space
    set_gop_virtual(bootInfo);
    set_memmap(bootInfo);

    // init Manic
    panic_init(bootInfo);

    console_init(0x0000FF00, 0x00000000);
    clear_screen(0x00000000);

    init_kalloc();

    gdt_init();
    idt_init();
    pic_init();
    __asm__ volatile("sti");
    uint64_t hz = tsc_calibrate();
    kb_init();
    init_syscall();
    init_processes();

    bool fat_ok = true;
    if (ata_init() < 0) {
        Mprint("[INFO] ATA not found (expected on NVMe hardware). gpt will most likely not work\n");
    } else {
        uint32_t lbas[8];
        int n = mbr_find_all_fat32_lba(lbas, 8);
        fat_ok = true;
        for (int i = 0; i < n; i++) {
            if (fat32_init(lbas[i]) < 0) continue;
            fat32_file_t probe = fat32_open("/bin/sh");
            if (probe.valid) {
                fat_ok = false;
                break;
            }
        }
        if (fat_ok)
            Mprint("[DISK] no MarvinOS partition found on any FAT32 partition, booting from stick with AHCI?\n");
    }

    bmp_draw("/bmp/logo.bmp", 430, 30);

    kernel_screen(fat_ok, hz, total_pages);

    start_scheduler();

    cpu_idle();

    // should never reach here, but supermanic if we do :D
    super_manic();
}

void super_manic() {
    Manic("SUPER MANIC!");
}

void cpu_idle() {
    while (1) {
        __asm__ volatile("sti");
        __asm__ volatile("hlt");
    }
}

// Called before init_paging() !!framebuffer_base is still a physical address!!
void set_gop_physical(BootInfo *info) {
    gop_info.fb      = (uint32_t *)info->framebuffer_base;
    gop_info.screen_w = info->width;
    gop_info.screen_h = info->height;
    gop_info.pitch    = info->pitch;
}

// Called after init_paging() !!framebuffer_base to virtual!!
void set_gop_virtual(BootInfo *info) {
    gop_info.fb               = (uint32_t *)info->framebuffer_base;
    gop_info.screen_w         = info->width;
    gop_info.screen_h         = info->height;
    gop_info.pitch            = info->pitch;
    gop_info.pixel_format     = info->pixel_format;
    gop_info.pixel_red_mask   = info->pixel_red_mask;
    gop_info.pixel_green_mask = info->pixel_green_mask;
    gop_info.pixel_blue_mask  = info->pixel_blue_mask;
}

// memory map info from bootloader
void set_memmap(BootInfo *info) {
    uefi_memory_map.mmap_addr     = (uint64_t*)info->mmap_addr;
    uefi_memory_map.mmap_size     = info->mmap_size;
    uefi_memory_map.mmap_desc_size = info->mmap_desc_size;
    uefi_memory_map.mmap_desc_ver = info->mmap_desc_ver;
}

// making the kernel screen look pretty
void kernel_screen(bool ok_to_be_fat, uint64_t hz, uint32_t total_pages) {
    const uint32_t BG    = 0x00000000;
    const uint32_t CYAN  = 0x0055FFFF;
    const uint32_t GREEN = 0x0055FF55;
    const uint32_t WHITE = 0x00EAEAEA;
    const uint32_t LBLUE = 0x005599FF;
    const uint32_t RED   = 0x00FF5555;
    const uint32_t GRAY  = 0x00888888;

    console_set_color(GREEN, BG);
    char *line = "----------------------------------------------------------------------\n";
    Mprint(line);
    Mprint("   __  __                  _        ___  ____  \n");
    Mprint("  |  \\/  | __ _ _ ____   _(_)_ __  / _ \\/ ___|\n");
    Mprint("  | |\\/| |/ _` | '__\\ \\ / / | '_ \\| | | \\___ \\\n");
    Mprint("  | |  | | (_| | |   \\ V /| | | | | |_| |___) |\n");
    Mprint("  |_|  |_|\\__,_|_|    \\_/ |_|_| |_|\\___/|____/\n");

    console_set_color(GREEN, BG);
    Mprint("  -------------------\n");

    console_set_color(LBLUE, BG); Mprint("  OS           ");
    console_set_color(WHITE, BG); Mprint("MarvinOS x86_64\n");

    console_set_color(LBLUE, BG); Mprint("  Kernel       ");
    console_set_color(WHITE, BG);
    Mprint_int((uint32_t)(__kernel_end - __kernel_start));
    Mprint(" bytes\n");

    console_set_color(LBLUE, BG); Mprint("  CPU          ");
    console_set_color(WHITE, BG);
    Mprint_int(hz / 1000000);
    Mprint(" MHz\n");

    console_set_color(LBLUE, BG); Mprint("  Memory       ");
    console_set_color(WHITE, BG);
    Mprint_int((uint64_t)total_pages * 4 / 1024);
    Mprint(" MB\n");

    console_set_color(LBLUE, BG); Mprint("  Paging       ");
    console_set_color(GREEN, BG); Mprint("enabled\n");
    console_set_color(LBLUE, BG); Mprint("  GDT          ");
    console_set_color(GREEN, BG); Mprint("enabled\n");
    console_set_color(LBLUE, BG); Mprint("  IDT          ");
    console_set_color(GREEN, BG); Mprint("enabled\n");
    console_set_color(LBLUE, BG); Mprint("  PIC          ");
    console_set_color(GREEN, BG); Mprint("enabled\n");
    console_set_color(LBLUE, BG); Mprint("  TSC          ");
    console_set_color(GREEN, BG); Mprint("enabled\n");
    console_set_color(LBLUE, BG); Mprint("  Keyboard     ");
    console_set_color(GREEN, BG); Mprint("enabled\n");
    console_set_color(LBLUE, BG); Mprint("  Syscall      ");
    console_set_color(GREEN, BG); Mprint("enabled\n");
    console_set_color(LBLUE, BG); Mprint("  Scheduler    ");
    console_set_color(GREEN, BG); Mprint("enabled\n");

    console_set_color(LBLUE, BG); Mprint("  ATA/FAT32    ");
    if (ok_to_be_fat) {
        console_set_color(RED,   BG); Mprint("failed\n");
    } else {
        console_set_color(GREEN, BG); Mprint("enabled\n");
    }

    console_set_color(GREEN, BG);
    Mprint(line);

    //color palette
    uint32_t palette[32] = {
        0x00AA0000, 0x00AA5500, 0x00AAAA00, 0x0000AA00,
        0x0000AAAA, 0x000055AA, 0x00AA00AA, 0x00555555,
        0x00FF5555, 0x00FFAA55, 0x00FFFF55, 0x0055FF55,
        0x0055FFFF, 0x005599FF, 0x00FF55FF, 0x00FFFFFF,
        0x00000000, 0x00550000, 0x00555500, 0x00005500,
        0x00005555, 0x000000AA, 0x00550055, 0x00AAAAAA,
        0x00222222, 0x00770000, 0x00777700, 0x00007700,
        0x00007777, 0x00000077, 0x00770077, 0x00DDDDDD,
    };
    Mprint("  ");
    for (int i = 0; i < sizeof(palette) / sizeof(palette[0]); i++) {
        console_set_color(palette[i], BG);
        Mprint("\x7f"); // DEL char as color block we can create an 8x8 block, maybe little cheating but it works and looks nice XD
    }
    console_set_color(GREEN, BG);
    Mprint("\n");
}

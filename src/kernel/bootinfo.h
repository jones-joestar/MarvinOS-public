#ifndef BOOTINFO_H
#define BOOTINFO_H

#include "helper/common.h"

// define structure of the boot info passed from the bootloader to the kernel

#define BOOT_MAX_FILES    8
#define BOOT_MAX_FILENAME 64

typedef struct {
    char     name[BOOT_MAX_FILENAME];
    uint64_t data;   // physical address of the loaded bytes
    uint32_t size;
    uint32_t _pad;   // keep struct at 80 bytes so bootloader/kernel offsets agree
} boot_file_t;

typedef struct {
    uint64_t framebuffer_base;   // physical address where the framebuffer starts
    uint32_t width;              // width of the display
    uint32_t height;             // height of the display
    uint32_t pitch;              // pixels per scan line
    uint32_t pixel_format;       // EFI_GRAPHICS_PIXEL_FORMAT: 0=RGB, 1=BGR, 2=BitMask
    uint64_t mmap_addr;          // physical address of the EFI memory map
    uint64_t mmap_size;          // total byte size of the memory map
    uint64_t mmap_desc_size;     // size of one EFI_MEMORY_DESCRIPTOR entry
    uint32_t mmap_desc_ver;      // descriptor version must be 1
    uint32_t pixel_red_mask;     // only valid for pixel_format == 2 (BitMask)
    uint32_t pixel_green_mask;
    uint32_t pixel_blue_mask;
    uint32_t file_count;
    boot_file_t files[BOOT_MAX_FILES];
} BootInfo;

#endif

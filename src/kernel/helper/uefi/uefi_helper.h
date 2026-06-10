#pragma once
#include "../common.h"

typedef struct {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} efi_memory_descriptor_t;

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
} efi_memory_type_t;

typedef struct {
    uint64_t *mmap_addr;
    uint64_t mmap_size;
    uint64_t mmap_desc_size;
    uint32_t mmap_desc_ver;
} uefi_memory_map_t;

typedef struct {
    uint32_t *fb;
    uint64_t phys_fb;
    uint32_t screen_w;
    uint32_t screen_h;
    uint32_t pitch;
    uint32_t pixel_format;      // 0=RGB, 1=BGR, 2=BitMask
    uint32_t pixel_red_mask;
    uint32_t pixel_green_mask;
    uint32_t pixel_blue_mask;
} gop_info_t;
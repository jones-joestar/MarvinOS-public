#pragma once
#include "../common.h"

struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

struct tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  type : 4;
    uint8_t  s    : 1;
    uint8_t  dpl  : 2;
    uint8_t  p    : 1;
    uint8_t  limit_high : 4;
    uint8_t  avl  : 1;
    uint8_t  l    : 1;
    uint8_t  db   : 1;
    uint8_t  g    : 1;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

void init_tss(void);
struct gdt_entry;
void put_tss_entry(struct gdt_entry* gdt);
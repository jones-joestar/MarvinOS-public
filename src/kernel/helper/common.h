#pragma once

#ifndef _STDINT_H
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long long uint64_t;
#endif

typedef uint32_t paddr_t;
typedef uint32_t vaddr_t;

#if !defined(__size_t__) && !defined(_SIZE_T_DEFINED)
typedef unsigned long size_t;
#define __size_t__
#endif

#ifndef __bool_true_false_are_defined
#define bool  _Bool
#define true  1
#define false 0
#define __bool_true_false_are_defined
#endif

#ifndef NULL
#define NULL  ((void *) 0)
#endif

#ifndef _STRING_H
void *memset(void *buf, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
char *strcpy(char *dst, const char *src);
int strcmp(const char *s1, const char *s2);
#endif

#ifndef _STDIO_H
int printf(const char *fmt, ...);
#endif

#define ADDRESS_INVALID ((void *)0xFFFFFFFFFFFFFFFFULL)

#define	RAND_MAX 2147483647

// Port I/O
// Direct hardware communication. Used by PIC, PIT, keyboard, and anything
// else that talks to the CPU via I/O ports instead of memory.

// Send a byte to a hardware port.
void outb(uint16_t port, uint8_t val);

// Read a byte from a hardware port.
uint8_t inb(uint16_t port);

// Small delay between port writes — old hardware needs a moment to catch up.
void io_wait(void);

static inline uint64_t save_flags_and_cli() {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags));
    return flags;
}

static inline void restore_flags(uint64_t flags) {
    __asm__ volatile("push %0; popfq" :: "r"(flags));
}

int    rand(void);
void   srand(unsigned int seed);

#include "common.h"

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *) dst;
    const uint8_t *s = (const uint8_t *) src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memset(void *buf, int c, size_t n) {
    uint8_t *p = (uint8_t *) buf;
    while (n--)
        *p++ = (uint8_t)c;
    return buf;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while (*src)
        *d++ = *src++;
    *d = '\0';
    return dst;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2)
            break;
        s1++;
        s2++;
    }

    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

// Writes to port 0x80 (POST diagnostic port) — harmless and burns ~1 µs.
void io_wait(void) {
    outb(0x80, 0);
}

static unsigned long rand_state = 1;

// generate pseudo-random number
int rand(void) {
    rand_state = rand_state * 6364136223846793005UL + 1442695040888963407UL;
    return (int)((rand_state >> 33) & RAND_MAX);
}

// seed the random number
void srand(unsigned int seed) {
    rand_state = seed;
}

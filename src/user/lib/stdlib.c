#include <stdlib.h>
#include "syscall.h"
#include <stddef.h>

#define ERANGE 34
int errno = 0;

#define ULONG_MAX    18446744073709551615UL
#define LONG_MAX     9223372036854775807L
#define LONG_MIN     (-LONG_MAX - 1L)

void exit(int status) {
    (void)status;
    sys_exit();
    while (1) {}
}

void abort(void) {
    sys_exit();
    while (1) {}
}

int atoi(const char *s) {
    int res = 0, neg = 0;
    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        res = res * 10 + (*s++ - '0');
    return neg ? -res : res;
}

long atol(const char *s) {
    return strtol(s, (char **)0, 10);
}

long strtol(const char *s, char **endptr, int base) {
    const char *orig = s;
    unsigned long acc = 0;
    int c, neg = 0, any = 0;

    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;

    c = *s;
    if (c == '-')      { neg = 1; s++; }
    else if (c == '+') { s++; }

    if ((base == 0 || base == 16) &&
        s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2; base = 16;
    }
    if (base == 0) base = (s[0] == '0') ? 8 : 10;

    unsigned long cutoff = neg ? (unsigned long)LONG_MAX + 1UL : (unsigned long)LONG_MAX;
    unsigned long cutlim = cutoff % (unsigned long)base;
    cutoff /= (unsigned long)base;

    for (;; s++) {
        c = (unsigned char)*s;
        if (c >= '0' && c <= '9')      c -= '0';
        else if (c >= 'A' && c <= 'Z') c -= 'A' - 10;
        else if (c >= 'a' && c <= 'z') c -= 'a' - 10;
        else break;

        if (c >= base) break;

        if (any < 0 || acc > cutoff || (acc == cutoff && (unsigned long)c > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc = acc * (unsigned long)base + (unsigned long)c;
        }
    }

    long result;
    if (any < 0) {
        result = neg ? LONG_MIN : LONG_MAX;
        errno = ERANGE;
    } else if (neg) {
        result = -(long)acc;
    } else {
        result = (long)acc;
    }

    if (endptr) *endptr = (char *)(any ? s : orig);
    return result;
}

unsigned long strtoul(const char *s, char **endptr, int base) {
    const char *orig = s;
    unsigned long acc = 0;
    int neg = 0, any = 0, digit;

    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;

    if (*s == '-')      { neg = 1; s++; }
    else if (*s == '+') { s++; }

    if ((base == 0 || base == 16) &&
        s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2; base = 16;
    }
    if (base == 0) base = (s[0] == '0') ? 8 : 10;

    unsigned long cutoff = ULONG_MAX / (unsigned long)base;
    unsigned long cutlim = ULONG_MAX % (unsigned long)base;

    for (;; s++) {
        unsigned char c = (unsigned char)*s;
        if (c >= '0' && c <= '9')      digit = c - '0';
        else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') digit = c - 'A' + 10;
        else break;

        if (digit >= base) break;

        if (any < 0 || acc > cutoff || (acc == cutoff && (unsigned long)digit > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc = acc * (unsigned long)base + (unsigned long)digit;
        }
    }

    if (any < 0) {
        acc = ULONG_MAX;
        errno = ERANGE;
    } else if (neg) {
        acc = (unsigned long)(-(long)acc);
    }

    if (endptr) *endptr = (char *)(any ? s : orig);
    return acc;
}

double strtod(const char *s, char **endptr) {
    const char *orig = s;
    double result = 0.0;
    int neg = 0;

    while (*s == ' ' || (*s >= '\t' && *s <= '\r')) s++;

    if      (*s == '-') { neg = 1; s++; }
    else if (*s == '+') {          s++; }

    // integer part
    while (*s >= '0' && *s <= '9')
        result = result * 10.0 + (*s++ - '0');

    // fractional part
    if (*s == '.') {
        double frac = 0.1;
        s++;
        while (*s >= '0' && *s <= '9') {
            result += (*s++ - '0') * frac;
            frac *= 0.1;
        }
    }

    // exponent part
    if (*s == 'e' || *s == 'E') {
        s++;
        int eneg = 0;
        if      (*s == '-') { eneg = 1; s++; }
        else if (*s == '+') {           s++; }
        int exp = 0;
        while (*s >= '0' && *s <= '9')
            exp = exp * 10 + (*s++ - '0');
        double scale = 1.0;
        while (exp-- > 0) scale *= 10.0;
        if (eneg) result /= scale;
        else      result *= scale;
    }

    if (endptr) *endptr = (char *)(s != orig ? s : orig);
    return neg ? -result : result;
}

double atof(const char *s) {
    return strtod(s, (char **)0);
}

int abs(int x)   { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }

// byte-level swap
static void swap_bytes(char *a, char *b, size_t n) {
    while (n--) { char t = *a; *a++ = *b; *b++ = t; }
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*cmp)(const void *, const void *)) {
    char *b = (char *)base;
    for (size_t i = 1; i < nmemb; i++) {
        size_t j = i;
        while (j > 0 && cmp(b + (j - 1) * size, b + j * size) > 0) {
            swap_bytes(b + (j - 1) * size, b + j * size, size);
            j--;
        }
    }
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*cmp)(const void *, const void *)) {
    const char *b = (const char *)base;
    size_t lo = 0, hi = nmemb;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int r = cmp(key, b + mid * size);
        if (r == 0) return (void *)(b + mid * size);
        if (r  < 0) hi = mid;
        else        lo = mid + 1;
    }
    return (void *)0;
}

char *getenv(const char *name) {
    (void)name;
    return (void *)0;
}

int mkdir(const char *path, unsigned int mode) {
    // TODO: sys_mkdir — needs a FAT32 create-directory syscall
    (void)path; (void)mode;
    return 0;
}

int system(const char *cmd) {
    (void)cmd;
    return -1; // no shell — ZenityAvailable() returns 0, zenity path skipped
}

// LCG random
static unsigned long rand_state = 1;

int rand(void) {
    rand_state = rand_state * 6364136223846793005UL + 1442695040888963407UL;
    return (int)((rand_state >> 33) & RAND_MAX);
}

void srand(unsigned int seed) {
    rand_state = seed;
}

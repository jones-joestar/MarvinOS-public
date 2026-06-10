#include <string.h>
#include <stdlib.h>

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *p1 = (const unsigned char *)a;
    const unsigned char *p2 = (const unsigned char *)b;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    while (n--) {
        if (*p == (unsigned char)c) return (void *)p;
        p++;
    }
    return (void *)0;
}

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

char *strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++)) ;
    return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *ret = dst;
    while (n > 0 && *src) {
        *dst++ = *src++;
        n--;
    }
    while (n > 0) {
        *dst++ = '\0';
        n--;
    }
    return ret;
}

char *strcat(char *dst, const char *src) {
    char *ret = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++)) ;
    return ret;
}

char *strncat(char *dst, const char *src, size_t n) {
    char *ret = dst;
    while (*dst) dst++;
    while (n-- && (*dst++ = *src++)) ;
    if (n == (size_t)-1) *dst = '\0';
    return ret;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    if (!n) return 0;
    while (--n && *a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (void *)0;
}

char *strrchr(const char *s, int c) {
    const char *last = (void *)0;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return (void *)0;
}

char *strtok(char *s, const char *delim) {
    static char *last = (void *)0;
    if (s) last = s;
    if (!last) return (void *)0;

    // skip leading delimiters
    while (*last) {
        const char *d = delim;
        while (*d && *d != *last) d++;
        if (!*d) break;
        last++;
    }
    if (!*last) return (void *)0;

    char *ret = last;
    // find end of token
    while (*last) {
        const char *d = delim;
        while (*d && *d != *last) d++;
        if (*d) {
            *last++ = '\0';
            return ret;
        }
        last++;
    }
    last = (void *)0;
    return ret;
}

char *strdup(const char *s) {
    size_t len = strlen(s);
    char *ret = malloc(len + 1);
    if (ret) memcpy(ret, s, len + 1);
    return ret;
}

char *strndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len]) len++;
    char *ret = malloc(len + 1);
    if (ret) {
        memcpy(ret, s, len);
        ret[len] = '\0';
    }
    return ret;
}

int strcasecmp(const char *a, const char *b) {
    unsigned char ca, cb;
    do {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
    } while (ca && ca == cb);
    return ca - cb;
}

int strncasecmp(const char *a, const char *b, size_t n) {
    unsigned char ca, cb;
    if (!n) return 0;
    do {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (!ca) break;
    } while (--n && ca == cb);
    return ca - cb;
}

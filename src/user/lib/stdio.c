#include <stdio.h>
#include <stdarg.h>
#include "syscall.h"
#include "malloc.h"

struct FILE {
    int fd;                // 0=stdin, 1=stdout, 2=stderr; >= 0 for disk files
    int eof;
    int error;
    int unget;
    long cur_offset;
};

static FILE _stdin  = { .fd = 0, .unget = -1, .cur_offset = 0 };
static FILE _stdout = { .fd = 1, .unget = -1, .cur_offset = 0 };
static FILE _stderr = { .fd = 2, .unget = -1, .cur_offset = 0 };

FILE *stdin  = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

// console output helpers

int fputc(int c, FILE *f) {
    if (f == stdout || f == stderr) {
        char tmp[2] = { (char)c, '\0' };
        sys_write(tmp);
        return (unsigned char)c;
    }
    return EOF;
}

int fputs(const char *s, FILE *f) {
    if (f == stdout || f == stderr) {
        sys_write(s);
        return 0;
    }
    return EOF;
}

int puts(const char *s) {
    sys_write(s);
    sys_write("\n");
    return 0;
}

int putchar(int c) {
    return fputc(c, stdout);
}

size_t fwrite(const void *buf, size_t size, size_t nmemb, FILE *f) {
    if ((f == stdout || f == stderr) && size > 0 && nmemb > 0) {
        const char *p = (const char *)buf;
        size_t total = size * nmemb;
        char tmp[256];
        size_t i = 0;
        while (i < total) {
            size_t chunk = total - i;
            if (chunk >= sizeof(tmp)) chunk = sizeof(tmp) - 1;
            for (size_t j = 0; j < chunk; j++) tmp[j] = p[i + j];
            tmp[chunk] = '\0';
            sys_write(tmp);
            i += chunk;
        }
        return nmemb;
    }
    return 0;
}

// console input helpers

int fgetc(FILE *f) {
    if (!f) return EOF;
    if (f->unget != -1) { int c = f->unget; f->unget = -1; return c; }
    if (f == stdin) return sys_read_char();
    if (f == stdout || f == stderr) return EOF;
    if (f->fd >= 0) {
        unsigned char c = 0;
        if (sys_fread(f->fd, &c, 1) == 1) {
            f->cur_offset++;
            return (int)c;
        }
        f->eof = 1;
        return EOF;
    }
    return EOF;
}

char *fgets(char *buf, int n, FILE *f) {
    if (!buf || n <= 0) return (void *)0;
    int i = 0;
    while (i < n - 1) {
        int c = fgetc(f);
        if (c == EOF) { if (i == 0) return (void *)0; break; }
        buf[i++] = (char)c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return buf;
}

int ungetc(int c, FILE *f) {
    if (f) { f->unget = c; return c; }
    return EOF;
}

// file I/O

FILE *fopen(const char *path, const char *mode) {
    (void)mode;
    if (!path) return (void *)0;
    int fd = sys_fopen(path);
    if (fd < 0) return (void *)0;

    FILE *f = malloc(sizeof(FILE));
    if (!f) {
        return (void *)0;
    }
    f->fd    = fd;
    f->eof   = 0;
    f->error = 0;
    f->unget = -1;
    f->cur_offset = 0;
    return f;
}

int fclose(FILE *f) {
    if (!f || f == stdin || f == stdout || f == stderr) return 0;
    free(f);
    return 0;
}

size_t fread(void *buf, size_t size, size_t nmemb, FILE *f) {
    if (!f || f == stdin || f == stdout || f == stderr || f->fd < 0 || size == 0 || nmemb == 0) return 0;
    uint32_t want = (uint32_t)(size * nmemb);
    uint32_t got  = sys_fread(f->fd, buf, want);
    if (got < want) f->eof = 1;
    f->cur_offset += got;
    return got / size;
}

int fseek(FILE *f, long offset, int whence) {
    if (!f || f == stdin || f == stdout || f == stderr || f->fd < 0) return -1;
    uint32_t abs_off;
    if (whence == SEEK_SET) {
        abs_off = (uint32_t)offset;
    } else if (whence == SEEK_CUR) {
        abs_off = (uint32_t)(f->cur_offset + offset);
    } else { /* SEEK_END */
        return -1;
    }
    int ret = sys_fseek(f->fd, abs_off);
    if (ret == 0) {
        f->cur_offset = abs_off;
        f->eof = 0;
    }
    return ret;
}

long ftell(FILE *f) {
    if (!f || f == stdin || f == stdout || f == stderr || f->fd < 0) return -1;
    return f->cur_offset;
}

int feof(FILE *f) {
    if (!f) return 1;
    return f->eof;
}

int ferror(FILE *f) {
    return f ? f->error : 1;
}

void rewind(FILE *f) {
    fseek(f, 0, SEEK_SET);
    if (f) f->eof = 0;
}

int fflush(FILE *f) {
    (void)f;
    return 0;
}

int remove(const char *path) {
    (void)path;
    return 0;
}

int rename(const char *oldpath, const char *newpath) {
    (void)oldpath; (void)newpath;
    return 0;
}

// vsnprintf

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
    size_t pos = 0;

#define EMIT(c) do { char _c = (char)(c); if (n > 0 && pos + 1 < n) buf[pos] = _c; pos++; } while (0)

    while (*fmt) {
        if (*fmt != '%') { EMIT(*fmt++); continue; }
        fmt++;

        /* flags */
        int flag_left = 0, flag_plus = 0, flag_space = 0, flag_zero = 0, flag_alt = 0;
        for (;;) {
            if      (*fmt == '-') { flag_left  = 1; fmt++; }
            else if (*fmt == '+') { flag_plus  = 1; fmt++; }
            else if (*fmt == ' ') { flag_space = 1; fmt++; }
            else if (*fmt == '0') { flag_zero  = 1; fmt++; }
            else if (*fmt == '#') { flag_alt   = 1; fmt++; }
            else break;
        }

        int width = 0;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            fmt++;
            if (width < 0) { flag_left = 1; width = -width; }
        } else {
            while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        }

        // precision
        int prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') {
                prec = va_arg(ap, int);
                fmt++;
                if (prec < 0) prec = -1;
            } else {
                while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0');
            }
        }

        // length modifier
        int len_l = 0, len_h = 0, len_z = 0;
        if (*fmt == 'h') {
            len_h = 1; fmt++;
            if (*fmt == 'h') { len_h = 2; fmt++; }
        } else if (*fmt == 'l') {
            len_l = 1; fmt++;
            if (*fmt == 'l') { len_l = 2; fmt++; }
        } else if (*fmt == 'z' || *fmt == 'Z') {
            len_z = 1; fmt++;
        } else if (*fmt == 'j' || *fmt == 't') {
            len_l = 2; fmt++;
        }

        char spec = *fmt;
        if (!spec) break;
        fmt++;

        if (spec == '%') { EMIT('%'); continue; }

        if (spec == 'c') {
            char c = (char)va_arg(ap, int);
            int pad = (width > 1) ? width - 1 : 0;
            if (!flag_left) for (int i = 0; i < pad; i++) EMIT(' ');
            EMIT(c);
            if (flag_left)  for (int i = 0; i < pad; i++) EMIT(' ');
            continue;
        }

        if (spec == 's') {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            int slen = 0;
            while (s[slen]) slen++;
            if (prec >= 0 && slen > prec) slen = prec;
            int pad = (width > slen) ? width - slen : 0;
            if (!flag_left) for (int i = 0; i < pad; i++) EMIT(' ');
            for (int i = 0; i < slen; i++) EMIT(s[i]);
            if (flag_left)  for (int i = 0; i < pad; i++) EMIT(' ');
            continue;
        }

        if (spec == 'd' || spec == 'i' || spec == 'u' ||
            spec == 'x' || spec == 'X' || spec == 'o' || spec == 'p') {

            int base, upper = 0, is_signed = (spec == 'd' || spec == 'i');
            int negative = 0;
            unsigned long long val;

            if (spec == 'p') {
                base = 16;
                flag_alt = 1;
                val = (unsigned long long)(unsigned long)va_arg(ap, void *);
            } else if (spec == 'x' || spec == 'X') {
                base = 16;
                upper = (spec == 'X');
                if      (len_l == 2) val = va_arg(ap, unsigned long long);
                else if (len_l == 1) val = (unsigned long long)va_arg(ap, unsigned long);
                else if (len_z)      val = (unsigned long long)va_arg(ap, size_t);
                else                 val = (unsigned long long)va_arg(ap, unsigned int);
            } else if (spec == 'o') {
                base = 8;
                if      (len_l == 2) val = va_arg(ap, unsigned long long);
                else if (len_l == 1) val = (unsigned long long)va_arg(ap, unsigned long);
                else if (len_z)      val = (unsigned long long)va_arg(ap, size_t);
                else                 val = (unsigned long long)va_arg(ap, unsigned int);
            } else if (is_signed) {
                base = 10;
                long long sval;
                if      (len_l == 2) sval = va_arg(ap, long long);
                else if (len_l == 1) sval = (long long)va_arg(ap, long);
                else if (len_z)      sval = (long long)(size_t)va_arg(ap, size_t);
                else if (len_h == 2) sval = (long long)(signed char)va_arg(ap, int);
                else if (len_h == 1) sval = (long long)(short)va_arg(ap, int);
                else                 sval = (long long)va_arg(ap, int);
                if (sval < 0) { negative = 1; val = (unsigned long long)(-(sval + 1)) + 1; }
                else            val = (unsigned long long)sval;
            } else {
                base = 10;
                if      (len_l == 2) val = va_arg(ap, unsigned long long);
                else if (len_l == 1) val = (unsigned long long)va_arg(ap, unsigned long);
                else if (len_z)      val = (unsigned long long)va_arg(ap, size_t);
                else if (len_h == 2) val = (unsigned long long)(unsigned char)va_arg(ap, unsigned int);
                else if (len_h == 1) val = (unsigned long long)(unsigned short)va_arg(ap, unsigned int);
                else                 val = (unsigned long long)va_arg(ap, unsigned int);
            }

            const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
            char tmp[66];
            int tlen = 0;
            if (val == 0) {
                if (prec != 0) tmp[tlen++] = '0';
            } else {
                unsigned long long v = val;
                while (v) { tmp[tlen++] = digits[v % (unsigned)base]; v /= (unsigned)base; }
            }

            char prefix[4];
            int  plen = 0;
            if (negative)         prefix[plen++] = '-';
            else if (flag_plus)   prefix[plen++] = '+';
            else if (flag_space)  prefix[plen++] = ' ';
            if (flag_alt && base == 16 && val != 0) {
                prefix[plen++] = '0';
                prefix[plen++] = upper ? 'X' : 'x';
            } else if (flag_alt && base == 8 && (tlen == 0 || tmp[tlen - 1] != '0')) {
                prefix[plen++] = '0';
            }

            // zero padding
            int zpad = (prec > tlen) ? prec - tlen : 0;
            if (prec >= 0) flag_zero = 0;

            int total = plen + zpad + tlen;
            int pad   = (width > total) ? width - total : 0;

            if (!flag_left && !flag_zero) for (int i = 0; i < pad;  i++) EMIT(' ');
            for (int i = 0; i < plen; i++) EMIT(prefix[i]);
            if (!flag_left &&  flag_zero) for (int i = 0; i < pad;  i++) EMIT('0');
            for (int i = 0; i < zpad; i++) EMIT('0');
            for (int i = tlen - 1; i >= 0; i--) EMIT(tmp[i]);
            if  (flag_left)               for (int i = 0; i < pad;  i++) EMIT(' ');
            continue;
        }

        if (spec == 'f' || spec == 'F' || spec == 'e' || spec == 'E' || spec == 'g' || spec == 'G') {
            double val = va_arg(ap, double);
            if (prec < 0) prec = 6;

            char sign = 0;
            if (val < 0.0) { sign = '-'; val = -val; }
            else if (flag_plus)  sign = '+';
            else if (flag_space) sign = ' ';

            unsigned long long ipart = (unsigned long long)val;
            double fpart = val - (double)ipart;

            int fprint = prec > 18 ? 18 : prec;
            double p10 = 1.0;
            for (int i = 0; i < fprint; i++) p10 *= 10.0;
            
            unsigned long long fval = (unsigned long long)(fpart * p10 + 0.500000000000001);
            if (fval >= (unsigned long long)p10) {
                ipart++;
                fval = 0;
            }

            // integer digits
            char itmp[24];
            int ilen = 0;
            if (ipart == 0) { itmp[ilen++] = '0'; }
            else { unsigned long long v = ipart; while (v) { itmp[ilen++] = (char)('0' + v % 10); v /= 10; } }

            // fractional digits
            char ftmp[20];
            unsigned long long fv = fval;
            for (int i = fprint - 1; i >= 0; i--) {
                ftmp[i] = (char)('0' + (fv % 10));
                fv /= 10;
            }

            int total = (sign ? 1 : 0) + ilen + (prec > 0 ? 1 + prec : 0);
            int pad   = (width > total) ? width - total : 0;

            if (!flag_left && !flag_zero) for (int i = 0; i < pad;  i++) EMIT(' ');
            if (sign)                      EMIT(sign);
            if (!flag_left &&  flag_zero) for (int i = 0; i < pad;  i++) EMIT('0');
            for (int i = ilen - 1; i >= 0; i--) EMIT(itmp[i]);
            if (prec > 0) {
                EMIT('.');
                for (int i = 0; i < fprint; i++) EMIT(ftmp[i]);
                for (int i = fprint; i < prec; i++) EMIT('0');
            }
            if (flag_left) for (int i = 0; i < pad; i++) EMIT(' ');
            continue;
        }

        // unhandled specifier
        EMIT('%');
        EMIT(spec);
    }

    if (n > 0) buf[pos < n ? pos : n - 1] = '\0';
    return (int)pos;

#undef EMIT
}

// rest of printf

int vsprintf(char *buf, const char *fmt, va_list ap) {
    return vsnprintf(buf, (size_t)-1, fmt, ap);
}

int vprintf(const char *fmt, va_list ap) {
    char buf[4096];
    int r = vsnprintf(buf, sizeof(buf), fmt, ap);
    sys_write(buf);
    return r;
}

int vfprintf(FILE *f, const char *fmt, va_list ap) {
    if (f == stdout || f == stderr)
        return vprintf(fmt, ap);
    return 0;
}

int snprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vsprintf(buf, fmt, ap);
    va_end(ap);
    return r;
}

int printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

int fprintf(FILE *f, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}

// basic sscanf

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

int sscanf(const char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const char *p = buf;
    int count = 0;

    while (*fmt && *p) {
        if (*fmt == '%') {
            fmt++;
            int suppress = (*fmt == '*');
            if (suppress) fmt++;
            while (*fmt >= '0' && *fmt <= '9') fmt++;
            // length modifier
            int ll = 0;
            if (*fmt == 'l') { ll = 1; fmt++; if (*fmt == 'l') { ll = 2; fmt++; } }
            char spec = *fmt++;

            // skip leading whitespace
            if (spec != 'c') while (is_space(*p)) p++;
            if (!*p) break;

            if (spec == 'd' || spec == 'i') {
                long long val = 0; int neg = 0;
                if (*p == '-') { neg = 1; p++; }
                else if (*p == '+') p++;
                if (*p < '0' || *p > '9') break;
                while (*p >= '0' && *p <= '9') val = val * 10 + (*p++ - '0');
                if (!suppress) {
                    if (ll >= 2) *va_arg(ap, long long *) = neg ? -val : val;
                    else if (ll == 1) *va_arg(ap, long *) = (long)(neg ? -val : val);
                    else *va_arg(ap, int *) = (int)(neg ? -val : val);
                    count++;
                }
            } else if (spec == 'u') {
                unsigned long long val = 0;
                while (*p >= '0' && *p <= '9') val = val * 10 + (*p++ - '0');
                if (!suppress) {
                    if (ll >= 2) *va_arg(ap, unsigned long long *) = val;
                    else if (ll == 1) *va_arg(ap, unsigned long *) = (unsigned long)val;
                    else *va_arg(ap, unsigned int *) = (unsigned int)val;
                    count++;
                }
            } else if (spec == 'x' || spec == 'X') {
                unsigned long long val = 0;
                if (*p == '0' && (*(p+1) == 'x' || *(p+1) == 'X')) p += 2;
                while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
                    int d = (*p >= '0' && *p <= '9') ? *p - '0' :
                            (*p >= 'a' && *p <= 'f') ? *p - 'a' + 10 : *p - 'A' + 10;
                    val = val * 16 + d;
                    p++;
                }
                if (!suppress) {
                    if (ll >= 2) *va_arg(ap, unsigned long long *) = val;
                    else if (ll == 1) *va_arg(ap, unsigned long *) = (unsigned long)val;
                    else *va_arg(ap, unsigned int *) = (unsigned int)val;
                    count++;
                }
            } else if (spec == 'f' || spec == 'g' || spec == 'e') {
                double val = 0.0; int neg = 0;
                if (*p == '-') { neg = 1; p++; }
                else if (*p == '+') p++;
                while (*p >= '0' && *p <= '9') val = val * 10.0 + (*p++ - '0');
                if (*p == '.') {
                    p++; double frac = 0.1;
                    while (*p >= '0' && *p <= '9') { val += (*p++ - '0') * frac; frac *= 0.1; }
                }
                if (!suppress) {
                    if (ll >= 1) *va_arg(ap, double *) = neg ? -val : val;
                    else *va_arg(ap, float *) = (float)(neg ? -val : val);
                    count++;
                }
            } else if (spec == 's') {
                char *out = suppress ? (void *)0 : va_arg(ap, char *);
                while (*p && !is_space(*p)) {
                    if (!suppress && out) *out++ = *p;
                    p++;
                }
                if (!suppress && out) { *out = '\0'; count++; }
            } else if (spec == 'c') {
                if (!suppress) { *va_arg(ap, char *) = *p; count++; }
                p++;
            } else if (spec == 'n') {
                if (!suppress) *va_arg(ap, int *) = (int)(p - buf);
            }
        } else if (is_space(*fmt)) {
            while (is_space(*p)) p++;
            while (is_space(*fmt)) fmt++;
        } else {
            if (*fmt == *p) { fmt++; p++; }
            else break;
        }
    }

    va_end(ap);
    return count;
}

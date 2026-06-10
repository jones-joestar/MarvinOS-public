#pragma once
#include <stddef.h>
#include <stdarg.h>

#define EOF       (-1)
#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2
#define BUFSIZ    1024

typedef struct FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

FILE  *fopen(const char *path, const char *mode);
int    fclose(FILE *f);
size_t fread(void *buf, size_t size, size_t nmemb, FILE *f);
size_t fwrite(const void *buf, size_t size, size_t nmemb, FILE *f);
int    fseek(FILE *f, long offset, int whence);
long   ftell(FILE *f);
int    feof(FILE *f);
int    ferror(FILE *f);
void   rewind(FILE *f);
int    fflush(FILE *f);

int    fgetc(FILE *f);
int    fputc(int c, FILE *f);
char  *fgets(char *buf, int n, FILE *f);
int    fputs(const char *s, FILE *f);
int    ungetc(int c, FILE *f);

int    puts(const char *s);
int    putchar(int c);

int    printf(const char *fmt, ...);
int    fprintf(FILE *f, const char *fmt, ...);
int    sprintf(char *buf, const char *fmt, ...);
int    snprintf(char *buf, size_t n, const char *fmt, ...);
int    vprintf(const char *fmt, va_list ap);
int    vfprintf(FILE *f, const char *fmt, va_list ap);
int    vsprintf(char *buf, const char *fmt, va_list ap);
int    vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);

int    sscanf(const char *buf, const char *fmt, ...);

int    remove(const char *path);
int    rename(const char *oldpath, const char *newpath);

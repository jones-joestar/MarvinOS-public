#pragma once
#include <stdint.h>

#define SYS_WRITE            1   // write(const char *s)
#define SYS_READ_CHAR        2   // read_char() -> int
#define SYS_EXEC             3   // exec(const char *path)
#define SYS_SHUTDOWN         4   // shutdown()
#define SYS_EXIT             5   // exit() -> restarts shell
#define SYS_GET_FRAMEBUFFER  6   // get_framebuffer(fb_info_t *out)
#define SYS_SLEEP_MS         7   // sleep_ms(unsigned int ms)
#define SYS_GET_TICKS_MS     8   // get_ticks_ms() -> unsigned int
#define SYS_KEY_EVENT        9   // key_event(key_event_t *ev) -> int
#define SYS_CLEAR           10   // clear()
#define SYS_HEAP_BASE       11   // heap_base() -> void*
#define SYS_ENLARGE_HEAP    12   // enlarge_heap() -> void* new heap_top
#define SYS_FOPEN           13   // fopen(path) -> int fd
#define SYS_FREAD           14   // fread(fd, buf, nbytes) -> uint32_t
#define SYS_FSEEK           15   // fseek(fd, offset) -> int
#define SYS_SET_COLOR       16   // set_color(fg, bg)
#define SYS_GIF_DRAW        17   // gif_draw(path, x, y, time) -> int
#define SYS_READDIR         18   // readdir(path, fat_dirent_t *buf, max) -> int count
#define SYS_BEEP            19   // beep(freq_hz, duration_ms)
#define SYS_KILL            20   // kill(int pid) -> int
#define SYS_SYNC            23
#define SYS_EXECBG          24
#define SYS_KEY_EVENT_FLUSH 25   // key_event_flush() -> void
#define SYS_PUT_CHAR        26   // put_char(c, x, y)
#define SYS_GET_TOTAL_PAGES 27   // get_total_pages() -> uint64_t
#define SYS_GET_ALLOC_PAGES 28   // get_allocated_pages() -> uint64_t
#define SYS_FCLOSE          31   // fclose(fd) -> int

typedef struct {
    uint32_t first_cluster;
    uint32_t cur_cluster;
    uint32_t size;
    uint32_t cur_offset;
    uint8_t  valid;
} fat32_file_t;

typedef struct {
    char    name[13];
    uint8_t is_dir;
} fat_dirent_t;

typedef struct {
    unsigned char scancode; // extended keys have bit 7 set
    unsigned char pressed;  // 1 = key down, 0 = key up
} key_event_t;

typedef struct {
    void         *fb;
    unsigned int  width;
    unsigned int  height;
    unsigned int  pitch;
} fb_info_t;

// write null terminated string to console
static inline void sys_write(const char *s) {
    __asm__ volatile(
        "mov $1, %%rax\n\t"
        "syscall"
        : : "D"(s) : "rax", "rcx", "r11", "memory"
    );
}

// read one character from keyboard
static inline int sys_read_char(void) {
    int ret;
    __asm__ volatile(
        "mov $2, %%rax\n\t"
        "syscall"
        : "=a"(ret) : : "rcx", "r11", "memory"
    );
    return ret;
}

// execute binary at path
static inline void sys_exec(const char *path) {
    __asm__ volatile(
        "mov $3, %%rax\n\t"
        "syscall"
        : : "D"(path) : "rax", "rcx", "r11", "memory"
    );
}

// power off the machine
static inline void sys_shutdown(void) {
    __asm__ volatile(
        "mov $4, %%rax\n\t"
        "syscall"
        : : : "rax", "rcx", "r11", "memory"
    );
}

// exit current process
static inline void sys_exit(void) {
    __asm__ volatile(
        "mov $5, %%rax\n\t"
        "syscall"
        : : : "rax", "rcx", "r11", "memory"
    );
}

// fill *out with framebuffer address, width, height, pitch
static inline void sys_get_framebuffer(fb_info_t *out) {
    __asm__ volatile(
        "mov $6, %%rax\n\t"
        "syscall"
        : : "D"(out) : "rax", "rcx", "r11", "memory"
    );
}

// spin-sleep for ms milliseconds
static inline void sys_sleep_ms(unsigned int ms) {
    __asm__ volatile(
        "mov $7, %%rax\n\t"
        "syscall"
        : : "D"((unsigned long)ms) : "rax", "rcx", "r11", "memory"
    );
}

// milliseconds elapsed since boot
static inline unsigned int sys_get_ticks_ms(void) {
    unsigned long ret;
    __asm__ volatile(
        "mov $8, %%rax\n\t"
        "syscall"
        : "=a"(ret) : : "rcx", "r11", "memory"
    );
    return (unsigned int)ret;
}

// clear the console
static inline void sys_clear(void) {
    __asm__ volatile(
        "mov $10, %%rax\n\t"
        "syscall"
        : : : "rax", "rcx", "r11", "memory"
    );
}

// pop one raw key event
static inline int sys_key_event(key_event_t *ev) {
    int ret;
    __asm__ volatile(
        "mov $9, %%rax\n\t"
        "syscall"
        : "=a"(ret) : "D"(ev) : "rcx", "r11", "memory"
    );
    return ret;
}

static inline void *sys_heap_base(void) {
    void *ret;
    __asm__ volatile(
        "mov $11, %%rax\n\t"
        "syscall"
        : "=a"(ret) : : "rcx", "r11", "memory"
    );
    return ret;
}

static inline void *sys_enlarge_heap(void) {
    void *ret;
    __asm__ volatile(
        "mov $12, %%rax\n\t"
        "syscall"
        : "=a"(ret) : : "rcx", "r11", "memory"
    );
    return ret;
}

// open file by path
static inline int sys_fopen(const char *path) {
    int ret;
    __asm__ volatile(
        "mov $13, %%rax\n\t"
        "syscall"
        : "=a"(ret) : "D"(path) : "rcx", "r11", "memory"
    );
    return ret;
}

// read nbytes from file descriptor fd into buf
static inline uint32_t sys_fread(int fd, void *buf, uint32_t nbytes) {
    uint32_t ret;
    __asm__ volatile(
        "mov $14, %%rax\n\t"
        "syscall"
        : "=a"(ret) : "D"((unsigned long)fd), "S"(buf), "d"((unsigned long)nbytes) : "rcx", "r11", "memory"
    );
    return ret;
}

// seek to absolute offset in file descriptor fd
static inline int sys_fseek(int fd, uint32_t offset) {
    int ret;
    __asm__ volatile(
        "mov $15, %%rax\n\t"
        "syscall"
        : "=a"(ret) : "D"((unsigned long)fd), "S"((unsigned long)offset) : "rcx", "r11", "memory"
    );
    return ret;
}

// set console foreground and background color
static inline void sys_set_color(unsigned int fg, unsigned int bg) {
    __asm__ volatile(
        "mov $16, %%rax\n\t"
        "syscall"
        : : "D"((unsigned long)fg), "S"((unsigned long)bg) : "rax", "rcx", "r11", "memory"
    );
}

// deprecated: GIF rendering moved to userspace
static inline int sys_gif_draw(const char *path, unsigned int x, unsigned int y, unsigned int time) {
    int ret;
    register unsigned long r10 __asm__("r10") = (unsigned long)time;
    __asm__ volatile(
        "mov $17, %%rax\n\t"
        "syscall"
        : "=a"(ret)
        : "D"(path), "S"((unsigned long)x), "d"((unsigned long)y), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// list directory entries at path into buf
static inline int sys_readdir(const char *path, fat_dirent_t *buf, uint32_t max) {
    int ret;
    __asm__ volatile(
        "mov $18, %%rax\n\t"
        "syscall"
        : "=a"(ret) : "D"(path), "S"(buf), "d"((unsigned long)max) : "rcx", "r11", "memory"
    );
    return ret;
}

// play a tone on the PC speaker
static inline void sys_beep(uint32_t freq_hz, uint32_t duration_ms) {
    __asm__ volatile(
        "mov $19, %%rax\n\t"
        "syscall"
        : : "D"((unsigned long)freq_hz), "S"((unsigned long)duration_ms)
        : "rax", "rcx", "r11", "memory"
    );
}

// kill a process
static inline int sys_kill(int pid) {
    int ret;
    __asm__ volatile(
        "mov $20, %%rax\n\t"
        "syscall"
        : "=a"(ret) : "D"(pid) : "rcx", "r11", "memory"
    );
    return ret;
}

#define SYS_AUDIO_INIT  21
#define SYS_AUDIO_WRITE 22

// Initialise SB16 at the given sample rate
static inline int sys_audio_init(unsigned int sample_rate) {
    long ret;
    __asm__ volatile(
        "mov $21, %%rax\n\t"
        "syscall\n\t"
        "mov %%rax, %0"
        : "=r"(ret)
        : "D"((unsigned long)sample_rate)
        : "rax", "rcx", "r11", "memory"
    );
    return (int)ret;
}

static inline void sys_audio_write(const short *buf, unsigned int count) {
    __asm__ volatile(
        "mov $22, %%rax\n\t"
        "syscall"
        : : "D"(buf), "S"((unsigned long)count)
        : "rax", "rcx", "r11", "memory"
    );
}

static inline void sys_sync(void) {
    __asm__ volatile(
        "mov $23, %%rax\n\t"
        "syscall"
        : : : "rax", "rcx", "r11", "memory"
    );
}

// execute a binary at the given path as a background process
static inline int sys_execbg(const char *path) {
    int ret;
    __asm__ volatile(
        "mov $24, %%rax\n\t"
        "syscall"
        : "=a"(ret) : "D"(path) : "rcx", "r11", "memory"
    );
    return ret;
}

// discard all pending raw key events
static inline void sys_key_event_flush(void) {
    __asm__ volatile(
        "mov $25, %%rax\n\t"
        "syscall"
        : : : "rax", "rcx", "r11", "memory"
    );
}

static inline void sys_put_char(char *c, uint32_t x, uint32_t y) {
    __asm__ volatile(
        "mov $26, %%rax\n\t"
        "syscall"
        : : "D"((unsigned long)*c), "S"((unsigned long)x), "d"((unsigned long)y)
        : "rax", "rcx", "r11", "memory"
    );
}

// returns total number of physical pages available at boot
static inline unsigned long long sys_get_total_pages(void) {
    unsigned long long ret;
    __asm__ volatile(
        "mov $27, %%rax\n\t"
        "syscall"
        : "=a"(ret) : : "rcx", "r11", "memory"
    );
    return ret;
}

// returns number of currently allocated pages
static inline unsigned long long sys_get_allocated_pages(void) {
    unsigned long long ret;
    __asm__ volatile(
        "mov $28, %%rax\n\t"
        "syscall"
        : "=a"(ret) : : "rcx", "r11", "memory"
    );
    return ret;
}

// close a file descriptor
static inline int sys_fclose(int fd) {
    int ret;
    __asm__ volatile(
        "mov $31, %%rax\n\t"
        "syscall"
        : "=a"(ret) : "D"((unsigned long)fd) : "rcx", "r11", "memory"
    );
    return ret;
}

// pause DMA before heavy disk I/O
static inline void sys_audio_stop(void) {
    __asm__ volatile(
        "mov $29, %%rax\n\t"
        "syscall"
        : : : "rax", "rcx", "r11", "memory"
    );
}

// resume DMA after disk I/O
static inline void sys_audio_start(void) {
    __asm__ volatile(
        "mov $30, %%rax\n\t"
        "syscall"
        : : : "rax", "rcx", "r11", "memory"
    );
}

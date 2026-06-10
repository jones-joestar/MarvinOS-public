#include "syscall.h"
#include "../../bootinfo.h"
#include "../console/console_helper.h"
#include "../panic/panic_helper.h"
#include "../keyboard/keyboard_helper.h"
#include "../process/process.h"
#include "../scheduler/dispatcher.h"
#include "../gop/gop_helper.h"
#include "../memory/paging_helper.h"
#include "../memory/kalloc.h"
#include "../memory/malloc.h"
#include "../timer/tsc.h"
#include "../timer/pit.h"
#include "../disk/fat32.h"
#include "../scheduler/scheduler.h"
#include "../process/process_map.h"
#include "../audio/sb16.h"
#include "../font/font_helper.h"
#include "../audio/spk.h"

/* 0=not initialised, 1=SB16, 2=PC speaker */
static int audio_mode = 0;

/* MSR Addresses */
#define MSR_EFER       0xC0000080 /* Extended Feature Enable Register */
#define MSR_IA32_STAR  0xC0000081 /* Ring 0 and Ring 3 segment bases */
#define MSR_IA32_LSTAR 0xC0000082 /* Target RIP for syscall */
#define MSR_IA32_FMASK 0xC0000084 /* RFLAGS mask */

/* EFER Bit Definitions */
#define EFER_SCE       (1ULL << 0)  /* System Call Extensions */

/* RFLAGS Bit Definitions */
#define X86_EFLAGS_IF  (1ULL << 9)  /* Interrupt Flag */
#define X86_EFLAGS_DF  (1ULL << 10) /* Direction Flag */

#define KERNEL_CS_BASE 0x08ULL
#define USER_CS_BASE   0x10ULL

#define NUM_SYSCALLS 32

typedef struct {
    void         *fb;
    unsigned int  width;
    unsigned int  height;
    unsigned int  pitch;
} fb_info_t;

extern void syscall_entry(void); // defined in syscall_entry.asm

static inline void outw(uint16_t port, uint16_t val);
static inline void outl(uint16_t port, uint32_t val);

/* syscall 0: no-op placeholder */
static uint64_t syscall_noop(struct pt_regs *regs) {
    (void)regs;
    return 0;
}

/* syscall 1: write null-terminated string to console */
static uint64_t syscall_write(struct pt_regs *regs) {
    const char *s = (const char *)regs->rdi;
    Mprint(s);
    return 0;
}

/* syscall 2: read one character from keyboard ring buffer */
// TODO: remove this and move functionality to fread
static uint64_t syscall_read_char(struct pt_regs *regs) {
    (void)regs;
    char c;
    // block if buffer is empty
    while ((c = kb_getchar(running_process->stdin)) == 0) {
        sleep_on(&kb_wait_queue);
    }
    return (uint64_t)(unsigned char)c;
}

// syscall 3: execute an elf binary and pause the current application until it finishes. returns -1 on failure
static uint64_t syscall_exec(struct pt_regs *regs) {
    process_t* proc = create_process((char *)regs->rdi, true);
    if (proc == NULL)
        return -1;

    running_process->state = WAITING_CHILD;
    yield();
    if (audio_mode == 1) sb16_shutdown();
    else if (audio_mode == 2) spk_shutdown();
    audio_mode = 0;
    return 0;
}


/* syscall 4: power-off */
static uint64_t syscall_shutdown(pt_regs_t *regs) {
    (void)regs;
    /* isa-debug-exit device (requires -device isa-debug-exit,iobase=0xf4 in QEMU) */
    outl(0xf4, 0x00);
    __asm__ volatile("cli; hlt");
    load_page_table(0);
    return 0;
}

/* syscall 5: exit current process and start a fresh shell */
static uint64_t syscall_exit(struct pt_regs *regs) {
    (void)regs;
    if (audio_mode == 1) sb16_shutdown();
    else if (audio_mode == 2) spk_shutdown();
    audio_mode = 0;
    terminate_process(running_process);
    return 0; /* unreachable */
}
/* syscall 6: return framebuffer base, dimensions, and pitch */
static uint64_t syscall_get_framebuffer(struct pt_regs *regs) {
    fb_info_t *out = (fb_info_t *)regs->rdi;

    uint64_t fb_phys = gop_info.phys_fb;
    uint64_t offset  = fb_phys & (TWO_MEGABYTE - 1);
    uint64_t aligned = fb_phys - offset;

    // map GOP for process with Write-Combining (PAGE_PAT_LARGE)
    for (uint64_t i = 0; i < GOP_NUM_PAGES; i++) {
        map_address((void*)(aligned + i * TWO_MEGABYTE),
            (void*)(GOP_USERSPACE + i * TWO_MEGABYTE),
            PAGE_WRITE | PAGE_HUGE | PAGE_PAT_LARGE | PAGE_USER_MODE, true);
    }
    out->fb     = (void*)(GOP_USERSPACE + offset);
    out->width  = gop_info.screen_w;
    out->height = gop_info.screen_h;
    out->pitch  = gop_info.pitch;
    return 0;
}
/* syscall 7: sleep for rdi milliseconds */
static uint64_t syscall_sleep_ms(struct pt_regs *regs) {
    running_process->wakeup_time = system_time_ms() + (uint64_t)regs->rdi;
    sleep_on(&sleep_queue);
    return 0;
}

/* syscall 8: milliseconds elapsed since boot */
static uint64_t syscall_get_ticks_ms(struct pt_regs *regs) {
    (void)regs;
    return system_time_ms();
}

/* syscall 9: pop one raw key event; returns 1 if got one, 0 if buffer empty */
static uint64_t syscall_key_event(struct pt_regs *regs) {
    // only the foreground process may process key events
    if (running_process == fg_process)
        return kb_get_key_event((key_event_t *)regs->rdi);

    return 0;
}

/* syscall 10: clear the console */
static uint64_t syscall_clear(struct pt_regs *regs) {
    (void)regs;
    Mclear();
    return 0;
}

// syscall 11: get heap base
static uint64_t syscall_heap_base(struct pt_regs *regs) {
    return (uint64_t)running_process->heap_base;
}

// syscall 12: map one new page at heap_top, advance heap_top by PAGE_SIZE, return new heap_top
static uint64_t syscall_enlarge_heap(struct pt_regs *regs) {
    void* frame;

    if ((frame = kalloc()) != ADDRESS_INVALID) {
        if (map_address(frame, (void*)running_process->heap_top, PAGE_WRITE | PAGE_USER_MODE | PAGE_EXECUTE_DISABLE, true)) {
            running_process->heap_top += PAGE_SIZE;
        } else {
            kfree(frame);
        }
    }
    return (uint64_t)running_process->heap_top;
}

extern BootInfo *bootInfo;

static const char *skip_slash(const char *p) {
    return (*p == '/') ? p + 1 : p;
}

static int strcmp_simple(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

// syscall 13: open a file; return the file descriptor
static uint64_t syscall_fopen(struct pt_regs *regs) {
    const char *path = (const char *)regs->rdi;
    if (!path) return (uint64_t)-1;

    // find a free slot first
    int slot = -1;
    for (int i = 3; i < MAX_FILE_DESCRIPTORS; i++) {
        if (running_process->fd[i].type == FD_NONE) { slot = i; break; }
    }
    if (slot < 0) return (uint64_t)-1;

    // check bootInfo preloaded files first (works on NVMe hardware without FAT32)
    if (bootInfo) {
        const char *want = skip_slash(path);
        for (uint32_t fi = 0; fi < bootInfo->file_count; fi++) {
            if (strcmp_simple(want, skip_slash(bootInfo->files[fi].name)) == 0) {
                preloaded_fd_t *pfd = malloc(sizeof(preloaded_fd_t));
                if (!pfd) return (uint64_t)-1;
                pfd->data   = (const uint8_t *)PHYS_TO_VIRT(bootInfo->files[fi].data);
                pfd->size   = bootInfo->files[fi].size;
                pfd->offset = 0;
                running_process->fd[slot].type      = FD_PRELOADED;
                running_process->fd[slot].preloaded = pfd;
                return (uint64_t)slot;
            }
        }
    }

    // fall back to FAT32
    running_process->fd[slot].type = FD_FAT32;
    running_process->fd[slot].file = malloc(sizeof(fat32_file_t));
    fat32_file_t *out = running_process->fd[slot].file;
    *out = fat32_open(path);
    if (out->valid) return (uint64_t)slot;

    free(out);
    running_process->fd[slot].file = NULL;
    running_process->fd[slot].type = FD_NONE;
    return (uint64_t)-1;
}

// syscall 14: read nbytes from a file descriptor into buf; returns bytes read
static uint64_t syscall_fread(struct pt_regs *regs) {
    uint16_t  fd     = (uint16_t)regs->rdi;
    void     *buf    = (void *)regs->rsi;
    uint32_t  nbytes = (uint32_t)regs->rdx;

    if (fd >= MAX_FILE_DESCRIPTORS || running_process->fd[fd].type == FD_NONE)
        return 0;

    if (running_process->fd[fd].type == FD_PRELOADED) {
        preloaded_fd_t *pfd = running_process->fd[fd].preloaded;
        uint32_t avail = (pfd->offset < pfd->size) ? pfd->size - pfd->offset : 0;
        uint32_t n = (nbytes < avail) ? nbytes : avail;
        if (n > 0) {
            uint8_t *dst = (uint8_t *)buf;
            const uint8_t *src = pfd->data + pfd->offset;
            for (uint32_t i = 0; i < n; i++) dst[i] = src[i];
            pfd->offset += n;
        }
        return n;
    }

    fat32_file_t *f = running_process->fd[fd].file;
    return fat32_read(f, buf, nbytes);
}

/* syscall 15: seek to absolute offset in file; returns 0 on success, -1 on error */
static uint64_t syscall_fseek(struct pt_regs *regs) {
    uint16_t fd     = (uint16_t)regs->rdi;
    uint32_t offset = (uint32_t)regs->rsi;

    if (fd >= MAX_FILE_DESCRIPTORS || running_process->fd[fd].type == FD_NONE)
        return (uint64_t)-1;

    if (running_process->fd[fd].type == FD_PRELOADED) {
        preloaded_fd_t *pfd = running_process->fd[fd].preloaded;
        if (offset > pfd->size) return (uint64_t)-1;
        pfd->offset = offset;
        return 0;
    }

    if (running_process->fd[fd].type != FD_FAT32)
        return (uint64_t)-1;

    fat32_file_t *f = running_process->fd[fd].file;
    return (uint64_t)fat32_seek(f, offset);
}

/* syscall 16: set console colors */
static uint64_t syscall_set_color(struct pt_regs *regs) {
    uint32_t fg = (uint32_t)regs->rdi;
    uint32_t bg = (uint32_t)regs->rsi;

    console_set_color(fg, bg);
    return 0;
}

/* syscall 17: deprecated — GIF rendering moved to userspace */
static uint64_t syscall_gif_draw(struct pt_regs *regs) {
    (void)regs;
    return 0;
}

/* syscall 18: list directory entries */
static uint64_t syscall_readdir(struct pt_regs *regs) {
    const char   *path = (const char *)regs->rdi;
    fat_dirent_t *buf  = (fat_dirent_t *)regs->rsi;
    uint32_t      max  = (uint32_t)regs->rdx;
    return (uint64_t)fat32_readdir(path, buf, max);
}

/* syscall 19: play PC speaker tone — rdi=freq_hz, rsi=duration_ms */
static uint64_t syscall_beep(struct pt_regs *regs) {
    pit_beep((uint32_t)regs->rdi, (uint32_t)regs->rsi);
    return 0;
}

// syscall 20: kill a process by PID
static uint64_t syscall_kill(struct pt_regs *regs) {
    uint16_t pid = (uint16_t)regs->rdi;
    process_t *proc = get_process(pid);
    if (proc == NULL || proc->state == TERMINATED) {
        return -1;
    }
    terminate_process(proc);
    return 0;
}

/* syscall 21: initialise audio — rdi=sample_rate
 * Returns 1 if SB16 found, 2 if falling back to PC speaker, 0 on total failure. */
static uint64_t syscall_audio_init(struct pt_regs *regs) {
    uint32_t rate = (uint32_t)regs->rdi;
    if (sb16_init(rate)) {
        audio_mode = 1;
        return 1;
    }
    /* No SB16 on this hardware — use PC speaker delta-sigma DAC instead. */
    spk_init();
    audio_mode = 2;
    return 2;
}

/* syscall 22: write audio samples — rdi=buf, rsi=count */
static uint64_t syscall_audio_write(struct pt_regs *regs) {
    const short  *buf   = (const short *)regs->rdi;
    unsigned int  count = (unsigned int)regs->rsi;
    if (audio_mode == 1)
        sb16_fill_half(buf);
    else if (audio_mode == 2)
        spk_fill(buf, count);
    return 0;
}

/* syscall 23: pause+resume DMA to yield QEMU event loop once per frame */
static uint64_t syscall_audio_sync(struct pt_regs *regs) {
    (void)regs;
    sb16_sync();
    return 0;
}

// syscall 24: execute a process in the background. returns -1 on failure
static uint64_t syscall_execbg(struct pt_regs *regs) {
    process_t* proc = create_process((char *)regs->rdi, false);
    if (proc)
        return proc->PID;

    return -1;
}

// syscall 25: flush key event buffer
static uint64_t syscall_key_event_flush(struct pt_regs *regs) {
    (void)regs;
    kb_event_flush();
    return 0;
}

// syscall 26: print char(rdi) at position (rsi, rdx)
static uint64_t syscall_put_char(struct pt_regs *regs) {
    char c = (char)regs->rdi;
    uint32_t xpos = (uint32_t)regs->rsi;
    uint64_t ypos = (uint32_t)regs->rdx;
    draw_char(c, xpos, ypos, 0x0000FF00, 0x00000000);
    return 0;
}

// syscall 27: return total physical pages
static uint64_t syscall_getTotal_Number(struct pt_regs *regs) {
    (void)regs;
    return total_pages;
}

// syscall 28: return currently allocated pages
static uint64_t syscall_getAllocated_Number(struct pt_regs *regs) {
    (void)regs;
    return allocated_pages;
}

/* syscall 29: pause DMA before heavy disk I/O */
static uint64_t syscall_audio_stop(struct pt_regs *regs) {
    (void)regs;
    sb16_stop();
    return 0;
}

/* syscall 30: resume DMA after disk I/O, buffer zeroed */
static uint64_t syscall_audio_start(struct pt_regs *regs) {
    (void)regs;
    sb16_start();
    return 0;
}

/* syscall 31: close a file descriptor */
static uint64_t syscall_fclose(struct pt_regs *regs) {
    uint16_t fd = (uint16_t)regs->rdi;
    if (fd >= MAX_FILE_DESCRIPTORS || running_process->fd[fd].type == FD_NONE)
        return (uint64_t)-1;
    free(running_process->fd[fd].file);
    running_process->fd[fd].file = NULL;
    running_process->fd[fd].type = FD_NONE;
    return 0;
}

typedef uint64_t (*syscall_ptr_t)(struct pt_regs*);
syscall_ptr_t syscall_table[NUM_SYSCALLS] = {
    &syscall_noop,
    &syscall_write,
    &syscall_read_char,
    &syscall_exec,
    &syscall_shutdown,
    &syscall_exit,
    &syscall_get_framebuffer,
    &syscall_sleep_ms,
    &syscall_get_ticks_ms,
    &syscall_key_event,
    &syscall_clear,
    &syscall_heap_base,
    &syscall_enlarge_heap,
    &syscall_fopen,
    &syscall_fread,
    &syscall_fseek,
    &syscall_set_color,
    &syscall_gif_draw,
    &syscall_readdir,
    &syscall_beep,
    &syscall_kill,
    &syscall_audio_init,
    &syscall_audio_write,
    &syscall_audio_sync,
    &syscall_execbg,
    &syscall_key_event_flush,
    &syscall_put_char,
    &syscall_getTotal_Number,
    &syscall_getAllocated_Number,
    &syscall_audio_stop,
    &syscall_audio_start,
    &syscall_fclose,
};

// Dispatches the function corresponding to the system call number
void syscall_dispatcher(pt_regs_t* regs) {
    if (regs->rax >= NUM_SYSCALLS) {
        // invalid syscall
        Manic("invalid system call number");
    }

    // Save the current kernel stack pointer for possible context switch
    running_process->kernel_stack.rsp = regs;

    // reenable interrupts
    __asm__ volatile("sti");

    // call the function and place return value in rax of pt_regs
    regs->rax = syscall_table[regs->rax](regs);
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

// read from 64-bit model specific registers
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile (
        "rdmsr"
        : "=a" (low), "=d" (high)
        : "c" (msr)
    );
    return ((uint64_t)high << 32) | low;
}

// write to 64-bit model specific registers
static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)val;
    uint32_t high = (uint32_t)(val >> 32);
    
    __asm__ volatile (
        "wrmsr"
        : 
        : "c" (msr), "a" (low), "d" (high)
        : "memory"
    );
}

// NGL i have no clue what this actually does
void init_syscall(void) {
    // Enable SCE (System Call Extensions) in EFER
    uint64_t efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | EFER_SCE);

    /* * 1. Program IA32_FMASK
     * Hardware applies an inverted bitwise AND (RFLAGS &= ~FMASK).
     * Setting the IF and DF bits in the mask forces the processor 
     * to clear them in RFLAGS immediately upon syscall execution.
     * This disables hardware interrupts during early entry and 
     * satisfies the ABI direction flag requirement.
     */
    uint64_t fmask = X86_EFLAGS_IF | X86_EFLAGS_DF;
    wrmsr(MSR_IA32_FMASK, fmask);

    // Program IA32_LSTAR. Defines the instruction pointer the CPU will jump to upon syscall
    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry);

    /*
     * 3. Program IA32_STAR
     * Defines the segment selectors for CS and SS. 
     * Bits [47:32] define the kernel base.
     * Bits [63:48] define the user base.
     */
    wrmsr(MSR_IA32_STAR, (KERNEL_CS_BASE << 32) | (USER_CS_BASE << 48));
}
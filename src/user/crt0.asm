[bits 64]

global _start
extern main     ; defined in shell.c

; .text.entry is placed first by user.ld, guaranteeing _start is at
; the very first byte of the flat binary (virtual address 0x10000).
section .text.entry
_start:
    cld         ; x86-64 ABI requires direction flag clear on function entry
    call main
    mov rax, 5  ; sys_exit: restart shell
    syscall
.hang:
    jmp .hang   ; unreachable, but prevents falling off into garbage

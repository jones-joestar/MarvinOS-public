[bits 64]
[extern kernel_main]
global boot_pml4
global boot_pdpt_ident
global boot_pdpt_kernel
global boot_pd
global boot_pd_1
global boot_pd_2
global boot_pd_3
global boot_pt_kernel
global stack_top

%define PAGE_PRESENT 0x1
%define PAGE_WRITE 0x2
%define HUGE_PAGE 0x80
%define FLAGS (PAGE_PRESENT | PAGE_WRITE)

%define HIGHER_OFFSET 0xFFFFFFFF80000000
%define PHYS(addr) (addr - HIGHER_OFFSET)

section .bss
align 16
resb 16384              ; 16 KB kernel stack
stack_top:

section .data
    align 4096
boot_pml4:
    dq PHYS(boot_pdpt_ident) + FLAGS
    times 510 dq 0
    dq PHYS(boot_pdpt_kernel) + FLAGS

boot_pdpt_ident:
    dq PHYS(boot_pd)   + FLAGS   ; 0–1 GB
    dq PHYS(boot_pd_1) + FLAGS   ; 1–2 GB
    dq PHYS(boot_pd_2) + FLAGS   ; 2–3 GB
    dq PHYS(boot_pd_3) + FLAGS   ; 3–4 GB (overwritten at runtime to cover the actual FB)
    times 508 dq 0

boot_pdpt_kernel:
    times 510 dq 0
    dq PHYS(boot_pd) + FLAGS     ; reuse first GB for higher-half early access
    dq 0

; boot_pd covers physical 0–1 GB with 2 MB pages
boot_pd:
%assign i 0
%rep 512
    dq (i * 0x200000) + FLAGS + HUGE_PAGE
    %assign i i+1
%endrep

; boot_pd_1 covers physical 1–2 GB
boot_pd_1:
%assign i 512
%rep 512
    dq (i * 0x200000) + FLAGS + HUGE_PAGE
    %assign i i+1
%endrep

; boot_pd_2 covers physical 2–3 GB
boot_pd_2:
%assign i 1024
%rep 512
    dq (i * 0x200000) + FLAGS + HUGE_PAGE
    %assign i i+1
%endrep

; boot_pd_3 is a scratch PD — _start overwrites it at runtime for the actual FB region
boot_pd_3:
%assign i 1536
%rep 512
    dq (i * 0x200000) + FLAGS + HUGE_PAGE
    %assign i i+1
%endrep

align 4096
boot_pt_kernel:
    times 512 dq 0


section .text.entry
[extern __bss_start]
[extern __bss_end]

global _start
_start:
    cli

    ; UEFI uses the Microsoft ABI, first argument is in RCX.
    ; Guard against callers that use System V (RDI) or Windows fast-call (RDX).
    mov r12, rcx
    test r12, r12
    jnz .start_init
    mov r12, rdi
    test r12, r12
    jnz .start_init
    mov r12, rdx

.start_init:
    ; The UEFI FB can be placed anywhere in physical RAM.  Ensure its 1 GB
    ; region is identity-mapped so we don't triple-fault before C paging runs.
    mov rdi, [r12]          ; BootInfo.framebuffer_base (first field, physical address)
    mov rax, rdi
    shr rax, 30
    and rax, 511            ; PDPT index of the 1 GB region containing the FB

    ; redirect boot_pdpt_ident[rax] → boot_pd_3
    mov rsi, PHYS(boot_pdpt_ident)
    mov rbx, PHYS(boot_pd_3)
    or  rbx, FLAGS
    mov [rsi + rax*8], rbx

    ; fill boot_pd_3 with 512 x 2 MB pages that identity-map that 1 GB
    mov rsi, rdi
    shr rsi, 30
    shl rsi, 30             ; round down to 1 GB boundary
    mov rbx, PHYS(boot_pd_3)
    mov rcx, 512
.fill_pd:
    mov rax, rsi
    or  rax, FLAGS | HUGE_PAGE
    mov [rbx], rax
    add rbx, 8
    add rsi, 0x200000
    loop .fill_pd

    ; LOAD NEW PAGE TABLES
    mov rax, PHYS(boot_pml4)
    mov cr3, rax

    ; ZERO BSS (physical addresses, before higher-half jump)
    mov rdi, __bss_start
    sub rdi, HIGHER_OFFSET
    mov rcx, __bss_end
    sub rcx, HIGHER_OFFSET
    sub rcx, rdi
    xor al, al
    rep stosb

    ; JUMP TO HIGHER HALF
    mov rsp, stack_top
    mov rax, higher_half
    jmp rax

higher_half:
    mov rdi, r12            ; pass BootInfo pointer as first argument (System V ABI)
    call kernel_main
    cli
    hlt

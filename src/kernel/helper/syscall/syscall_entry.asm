[bits 64]
global syscall_entry
extern tss
extern syscall_dispatcher

%define tss.rsp0 tss + 4
%define tss.rsp2 tss + 20

syscall_entry:
    swapgs
    mov [tss.rsp2], rsp  ; Save user stack pointer
    mov rsp, [tss.rsp0]  ; Load kernel stack pointer

    ; Construct pt_regs structure on the kernel stack
    ; The sequence of pushes defines the memory layout of the C structure
    push qword (0x18 | 3) ; user_ss
    push qword [tss.rsp2] ; user_rsp
    push r11              ; user_rflags (hardware saved to r11)
    push qword (0x20 | 3) ; user_cs
    push rcx              ; user_rip (hardware saved to rcx)

    push r15
    push r14
    push r13
    push r12
    push r11
    push r10              ; syscall arg 4
    push r9               ; syscall arg 6
    push r8               ; syscall arg 5
    push rbp
    push rdi              ; syscall arg 1
    push rsi              ; syscall arg 2
    push rdx              ; syscall arg 3
    push rcx
    push rbx
    push rax              ; syscall number

    ; Pass the stack pointer (pt_regs) as the first argument to the C function
    mov rdi, rsp
    call syscall_dispatcher
syscall_return:
    cli
    ; Pop general purpose registers in reverse order of pt_regs structure.
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    ; Pop the trap frame
    pop rcx      ; user_rip
    add rsp, 8   ; skip user_cs
    pop r11      ; user_rflags
    pop rsp      ; user_rsp
    ; user_ss is discarded as we switch to the user stack

    swapgs
    o64 sysret
    hlt
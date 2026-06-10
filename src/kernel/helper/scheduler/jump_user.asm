[bits 64]
global jump_to_user

section .text
 ; 1st arg in rdi: kernel stack pointer (base of pt_regs)
jump_to_user:
    cli
    ; Set rsp to the kernel stack pointer of the target process
    mov rsp, rdi

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

    iretq
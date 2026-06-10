[bits 64]

global yield_handler
extern schedule

yield_handler:
    ; The CPU (via int 0x81) has already pushed: SS, RSP, RFLAGS, CS, RIP
    ; Now we push the remaining general purpose registers to match pt_regs
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    ; Pass pt_regs to schedule
    mov rdi, rsp
    call schedule

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

    ; IRETQ expects: RIP, CS, RFLAGS, RSP, SS
    iretq


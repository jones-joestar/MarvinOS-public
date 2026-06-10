[bits 64]

extern isr_handler
extern isr_default_handler
extern keyboard_irq_handler

global idt_flush
idt_flush:
    lidt [rdi]
    ret

; Save and restore registers
%macro PUSH_ALL 0
    push rbp
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POP_ALL 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    pop rbp
%endmacro

; No error code - push 0 as placeholder
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    PUSH_ALL
    sub rsp, 8
    mov rdi, %1     ; vector
    mov rsi, 0      ; kein error code
    call isr_handler
    add rsp, 8
    POP_ALL
    iretq
%endmacro

; With error code - on the stack below the interrupt frame
%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    PUSH_ALL
    sub rsp, 8
    mov rdi, %1     ; vector
    mov rsi, [rsp + 8 + 15*8]   ; error code is above the stored registers
    call isr_handler
    add rsp, 8
    POP_ALL
    add rsp, 8      ; clear error code from stack
    iretq
%endmacro

; default for other interrupts
global isr_default
isr_default:
    cli
    PUSH_ALL
    sub rsp, 8
    call isr_default_handler
    add rsp, 8
    POP_ALL
    iretq

; exceptions without error code
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7

; IRQ handlers
global irq1
irq1:
    cli
    PUSH_ALL
    sub rsp, 8
    call keyboard_irq_handler
    add rsp, 8
    POP_ALL
    iretq

global irq5_stub
extern sb16_irq_handler

irq5_stub:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    call sb16_irq_handler

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    
    iretq


; exceptions with error code
ISR_ERR 8
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13

global isr14
extern page_fault_handler
isr14:
    cli
    ; Push general purpose registers to match pt_regs_t layout (r15..rax)
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

    ; Read the error code (currently at rsp + 120) into rsi (2nd parameter)
    mov rsi, [rsp + 120]

    ; Shift the return frame down by 8 bytes to overwrite the error code slot
    mov rax, [rsp + 128]  ; rip
    mov [rsp + 120], rax
    mov rax, [rsp + 136]  ; cs
    mov [rsp + 128], rax
    mov rax, [rsp + 144]  ; rflags
    mov [rsp + 136], rax
    mov rax, [rsp + 152]  ; rsp
    mov [rsp + 144], rax
    mov rax, [rsp + 160]  ; ss
    mov [rsp + 152], rax

    ; Align stack to 16 bytes for C function call (subtract 8 bytes)
    sub rsp, 8
    
    ; Pass pt_regs_t pointer (at rsp + 8) as 1st parameter in rdi
    lea rdi, [rsp + 8]
    
    call page_fault_handler
    
    ; Restore stack alignment
    add rsp, 8

    ; Pop general purpose registers
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
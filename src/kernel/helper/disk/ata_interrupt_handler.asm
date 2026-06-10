[bits 64]

global ata_interrupt_handler
extern pic_send_eoi
extern wakeup_disk_queue
extern schedule

ata_interrupt_handler:
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

    ; EOI for IRQ 14 (Primary ATA)
    mov rdi, 14
    call pic_send_eoi

    call wakeup_disk_queue

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

    iretq

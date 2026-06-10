[bits 64]

global gdt_flush
gdt_flush:
    lgdt [rdi]              ; load the GDT register

    ; Reload CS using a far return trick:
    ; push the new CS selector and the return address onto the stack,
    ; then do retfq which pops both and jumps there with the new CS.
    mov ax, 0x10            ; kernel data selector
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; For CS we a far return
    pop rdi                 ; save return address
    push 0x08               ; push kernel code selector
    push rdi                ; push return address back
    retfq                   ; far return: pops RIP and CS
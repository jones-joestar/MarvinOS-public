[bits 64]
global init_pt_regs

 ; 1st arg in rdi: kernel stack top
 ; 2nd arg in rsi: stack pointer (stack top)
 ; 3rd arg in rdx: instruction pointer (entry point)
init_pt_regs:
    ; start of the pt_regs structure (20 * 8 bytes = 160)
    sub rdi, 160

    ; Fill the trap frame (offsets 120-152)
    mov qword [rdi + 152], (0x18 | 3) ; ss (User data segment)
    mov qword [rdi + 144], rsi        ; rsp
    mov qword [rdi + 136], 0x202      ; rflags (IF set)
    mov qword [rdi + 128], (0x20 | 3) ; cs (User code segment)
    mov qword [rdi + 120], rdx        ; rip

    ; Fill general purpose registers with 0
    xor rax, rax
    %assign i 0
    %rep 15
        mov qword [rdi + i*8], rax
        %assign i i+1
    %endrep

    ; Return the pointer to the base of the pt_regs structure
    mov rax, rdi
    ret

global init_kernel_pt_regs
 ; 1st arg in rdi: kernel stack top
 ; 2nd arg in rsi: entry point
init_kernel_pt_regs:
    ; start of the pt_regs structure (20 * 8 bytes = 160)
    sub rdi, 160

    ; Fill the trap frame (offsets 120-152)
    mov qword [rdi + 152], 0x10       ; ss (Kernel data segment)
    mov qword [rdi + 144], rdi        ; rsp
    mov qword [rdi + 136], 0x202      ; rflags (IF set)
    mov qword [rdi + 128], 0x08       ; cs (Kernel code segment)
    mov qword [rdi + 120], rsi        ; rip (entry point)

    ; Fill general purpose registers with 0
    xor rax, rax
    %assign i 0
    %rep 15
        mov qword [rdi + i*8], rax
        %assign i i+1
    %endrep

    ; Return the pointer to the base of the pt_regs structure
    mov rax, rdi
    ret

[bits 64]
nop
nop
nop
mov rax, 4096
mov dword [rax], 'test'
syscall
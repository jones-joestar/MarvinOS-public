[bits 64]

global load_tss
load_tss:
    mov ax, di              ; selector is passed in di
    ltr ax                  ; load task register
    ret
[bits 64]

global timer_interrupt_handler
extern schedule
extern pic_send_eoi
extern wakeup_sleep_queue
extern spk_irq_tick
extern sched_div_limit   ; uint64_t in scheduler.c: 1 at boot, 110 when PIT@11025Hz

section .bss
sched_tick: resq 1   ; counts IRQ0 ticks between scheduler runs

section .text

timer_interrupt_handler:
    ; interrupt frame is already on the stack, so we push the rest of the pt_regs struct
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

    mov rdi, 0
    call pic_send_eoi ; end of interrupt

    call spk_irq_tick ; PC speaker delta-sigma output (no-op until spk_init)

    ; Throttle: only run wakeup + schedule every sched_div_limit ticks.
    ; At 100 Hz PIT sched_div_limit=1 so every tick runs the scheduler.
    ; At 11025 Hz PIT sched_div_limit=110 so ~100 Hz scheduler rate.
    ; yield_handler calls schedule directly and is unaffected by this gate.
    inc qword [rel sched_tick]
    mov rax, [rel sched_div_limit]
    cmp qword [rel sched_tick], rax
    jl .skip_sched
    mov qword [rel sched_tick], 0

    call wakeup_sleep_queue

    mov rdi, rsp ; pass pointer to pt_regs on stack
    call schedule

.skip_sched:
    ; if no context switch is needed, schedule will just return to here
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

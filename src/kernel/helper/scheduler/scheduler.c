#include "scheduler.h"
#include "dispatcher.h"
#include "../timer/pit.h"
#include "../timer/tsc.h"
#include "../idt/idt_helper.h"
#include "../pic/pic_helper.h"
#include "../process/process.h"
#include "../panic/panic_helper.h"
#include "../console/console_helper.h"


extern void timer_interrupt_handler(void);
extern void yield_handler(void);

/* Scheduler tick divisor — IRQ0 calls schedule() only every sched_div_limit ticks.
 * 1 at boot (100 Hz PIT → schedule every tick).
 * Set to 110 by spk_init() when PIT switches to 11025 Hz (11025/110 ≈ 100 Hz). */
volatile uint64_t sched_div_limit = 1;

void init_timer(uint32_t freq_hz);
void reload_timer();

static process_t* round_robin_schedule();
static process_t* BOGO_schedule();

process_t *running_process;

process_queue_t ready_queue = {NULL, NULL};
process_queue_t disk_wait_queue = {NULL, NULL};
process_queue_t sleep_queue = {NULL, NULL};
process_queue_t kb_wait_queue = {NULL, NULL};
process_queue_t audio_wait_queue = {NULL, NULL};
process_queue_t terminated_queue = {NULL, NULL};

static uint16_t timer_initial_count = 0;

// schedule the next process to be run
void schedule(pt_regs_t *regs) {
    running_process->kernel_stack.rsp = regs;

    // work on cleaning up terminated processes if necessary
    if (terminated_queue.tail && terminated_queue.tail != running_process) {
        process_t *proc = pop(&terminated_queue);
        if (proc && !delete_process(proc)) {
            push(&terminated_queue, proc);
        }
    }

    // mark running processes as ready (if not waiting or otherwise)
    if (running_process->state == RUNNING) {
        running_process->state = READY;

        // push to ready queue (except proc0)
        if (running_process->PID)
            push(&ready_queue, running_process);
    }

    process_t *next_process = round_robin_schedule();
    //process_t *next_process = BOGO_schedule();

    if (!next_process) {
        // if no process can run, idle (process 0)
        if (running_process != &proc0) {
            proc0_idle();
        }
        
        // continue the current process if it is proc0 or no other process is ready
        running_process->state = RUNNING;
        return;
    }

    if (next_process == running_process) {
        running_process->state = RUNNING;
        return;
    }

    switch_context(next_process);
}

static process_t* round_robin_schedule() {
    return pop(&ready_queue);
}

static process_t* BOGO_schedule() {
    uint64_t flags = save_flags_and_cli();
    if (!ready_queue.tail) {
        restore_flags(flags);
        return NULL;
    }

    // count number of procs in queue
    int count = 0;
    process_t* current = ready_queue.tail;
    while (current) {
        count++;
        current = current->prev;
    }

    // choose random index
    int p = rand() % count;
    
    current = ready_queue.tail;
    for (int i = 0; i < p; i++) {
        current = current->prev;
    }

    // remove from queue
    if (current->prev) {
        current->prev->next = current->next;
    } else {
        ready_queue.head = current->next;
    }

    if (current->next) {
        current->next->prev = current->prev;
    } else {
        ready_queue.tail = current->prev;
    }

    current->next = NULL;
    current->prev = NULL;

    restore_flags(flags);
    return current;
}

// put the current process on the specified waiting queue and yield
void sleep_on(process_queue_t *queue) {
    uint64_t flags = save_flags_and_cli();
    if (running_process == &proc0) {
        restore_flags(flags);
        return;
    }

    running_process->state = WAITING;
    
    // add to queue head
    running_process->next = queue->head;
    running_process->prev = NULL;
    if (queue->head) {
        queue->head->prev = running_process;
    } else {
        queue->tail = running_process;
    }
    queue->head = running_process;
    
    yield();
    restore_flags(flags);
}

// Wake up all processes in the specified queue
void wakeup(process_queue_t *queue) {
    uint64_t flags = save_flags_and_cli();
    process_t *p = queue->tail;

    while (p) {
        process_t *prev = p->prev;
        p->state = READY;
        push(&ready_queue, p);
        p = prev;
    }
    queue->head = NULL;
    queue->tail = NULL;
    restore_flags(flags);
}

// wake up processes in the sleep queue where timer has expired
void wakeup_sleep_queue(void) {
    uint64_t flags = save_flags_and_cli();
    process_t *p = sleep_queue.tail;
    uint64_t current_time = system_time_ms();

    while (p) {
        process_t *prev = p->prev;
        if (current_time >= p->wakeup_time) {
            // Remove from sleep_queue
            if (p->prev) {
                p->prev->next = p->next;
            } else {
                sleep_queue.head = p->next;
            }
            
            if (p->next) {
                p->next->prev = p->prev;
            } else {
                sleep_queue.tail = p->prev;
            }
            
            p->state = READY;
            push(&ready_queue, p);
        }
        p = prev;
    }
    restore_flags(flags);
}

void wakeup_kb_queue() {
    uint64_t flags = save_flags_and_cli();
    process_t *p = kb_wait_queue.tail;

    while (p) {
        process_t *prev = p->prev;
        
        if (p == fg_process){
            // Remove from kb_wait_queue
            if (p->prev) {
                p->prev->next = p->next;
            } else {
                kb_wait_queue.head = p->next;
            }
            
            if (p->next) {
                p->next->prev = p->prev;
            } else {
                kb_wait_queue.tail = p->prev;
            }

            p->state = READY;
            push(&ready_queue, p);
            break;
        }
        p = prev;
    }
    restore_flags(flags);
}

// yield execution, this will call the scheduler
void yield(void) {
    __asm__ volatile("int $0x81");
}

// create the first process and start the round-robin scheduler
void start_scheduler() {
    uint64_t tsc = rdtsc();
    srand((unsigned int)tsc);

    __asm__ volatile("cli");
    idt_set_entry(0x81, yield_handler);

    create_process("/bin/sh", true);
    //create_process("/bin/count");
    //create_process("/bin/doom");
    init_timer(100);
    
    process_t *next = pop(&ready_queue);
    if (next) {
        switch_context(next);
    } else {
        // Should not happen as we just created /bin/sh
        Merror("didn't find a process");
        switch_context(&proc0);
    }
}

// push a process into a queue. atomic operation
void push(process_queue_t *queue, process_t *process) {
    uint64_t flags = save_flags_and_cli();
    process->prev = NULL;
    if (queue->head) {
        queue->head->prev = process;
        process->next = queue->head;
        queue->head = process;
    } else {
        process->next = NULL;
        queue->head = process;
        queue->tail = process;
    }
    restore_flags(flags);
}

// gets the next process from a queue. atomic operation
process_t* pop(process_queue_t *queue) {
    uint64_t flags = save_flags_and_cli();
    if (!queue->tail) {
        restore_flags(flags);
        return NULL;
    }
    process_t *proc = queue->tail;
    queue->tail = proc->prev;
    if (queue->tail) {
        queue->tail->next = NULL;
    } else {
        queue->head = NULL;
    }
    proc->next = NULL;
    proc->prev = NULL;
    restore_flags(flags);
    return proc;
}

// Resets the scheduler timer
void reload_timer() {
    if (timer_initial_count == 0) return;
    
    // 0x43 = command port
    // 00 = channel 0 | 11 = lo then hi access for next 2 writes | 010 = mode 2 rate generator
    outb(0x43, 0b00110100);

    // 0x40 = channel 0 data port
    outb(0x40, (uint8_t)(timer_initial_count & 0xFF)); // low byte
    outb(0x40, (uint8_t)((timer_initial_count >> 8) & 0xFF)); // high byte
}

// Intialize PIT channel 0 to generate interrupts for the scheduler
void init_timer(uint32_t freq_hz) {
    if (freq_hz == 0) return;
    uint32_t count = (uint32_t)(PIT_HZ / freq_hz);
    
    // Max value for count is 65535, but 0 is 65536
    if (count > 65536) {
        count = 0;
    } else if (count < 2) {
        count = 2;
    }

    timer_initial_count = (uint16_t)count;

    reload_timer();

    idt_set_entry(PIC_IRQ_OFFSET_MASTER + 0, timer_interrupt_handler);
    pic_unmask_irq(0);
}

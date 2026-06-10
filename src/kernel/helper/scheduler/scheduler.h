#pragma once
#include "../common.h"
#include "../process/process.h"

typedef struct {
    process_t *head;
    process_t *tail;
} process_queue_t;

extern process_t *running_process;
extern process_queue_t disk_wait_queue;
extern process_queue_t ready_queue;
extern process_queue_t sleep_queue;
extern process_queue_t kb_wait_queue;
extern process_queue_t audio_wait_queue;
extern process_queue_t terminated_queue;

void start_scheduler();
void schedule(pt_regs_t *regs);
void push(process_queue_t *queue, process_t *process);
process_t* pop(process_queue_t *queue);

void sleep_on(process_queue_t *queue);
void wakeup(process_queue_t *queue);
void wakeup_kb_queue();
void yield(void);
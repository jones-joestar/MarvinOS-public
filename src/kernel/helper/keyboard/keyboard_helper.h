#pragma once
#include "../common.h"

#define KB_BUFFER_SIZE 256

typedef struct {
    uint8_t scancode; 
    uint8_t pressed;  /* 1 = key down, 0 = key up */
} key_event_t;

typedef struct {
    char data[KB_BUFFER_SIZE];
    uint8_t head; // last element + 1
    uint8_t tail; // first element
} ring_buffer_t;

// Register the IRQ1 handler and unmask the keyboard IRQ.
void kb_init(void);

// Returns 0 if the buffer is empty.
char kb_getchar(ring_buffer_t* buf);

// Discard all pending characters in the shell keyboard buffer.
void kb_flush(ring_buffer_t* buf);

// Discard all pending key events
void kb_event_flush();

// Pop one raw key event. 
int kb_get_key_event(key_event_t *ev);

// Called from the IRQ1 ASM stub — do not call directly.
void keyboard_irq_handler(void);

extern void irq1(void);

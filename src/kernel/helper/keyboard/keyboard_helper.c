#include "keyboard_helper.h"
#include "idt/idt_helper.h"
#include "pic/pic_helper.h"
#include "../process/process.h"
#include "../scheduler/scheduler.h"

#define KB_DATA_PORT   0x60
#define KEY_UP     0x80
#define KEY_DOWN   0x81
#define KEY_LEFT   0x82
#define KEY_RIGHT  0x83

// Swiss German (CH-DE) Scancode Set 1 — unshifted

static const char scancode_normal[128] = {
    0,    0,    '1',  '2',  '3',  '4',  '5',  '6',  // 0x00
    '7',  '8',  '9',  '0',  '\'', '^',  '\b', '\t', // 0x08
    'q',  'w',  'e',  'r',  't',  'z',  'u',  'i',  // 0x10
    'o',  'p',  0,    0,    '\n', 0,    'a',  's',  // 0x18
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  0,    // 0x20
    0,    0,    0,    '$',  'y',  'x',  'c',  'v',  // 0x28
    'b',  'n',  'm',  ',',  '.',  '-',  0,    '*',  // 0x30
    0,    ' ',  0,    0,    0,    0,    0,    0,    // 0x38
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x40
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x48
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x50
    '<',  0,    0,    0,    0,    0,    0,    0,    // 0x56
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x60
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x68
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x70
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x78
};

// Swiss German (CH-DE) Scancode Set 1 — shifted
static const char scancode_shift[128] = {
    0,    0,    '+',  '"',  '*',  0,    '%',  '&',  // 0x00
    '/',  '(',  ')',  '=',  '?',  '`',  '\b', '\t', // 0x08
    'Q',  'W',  'E',  'R',  'T',  'Z',  'U',  'I',  // 0x10
    'O',  'P',  0,    '!',  '\n', 0,    'A',  'S',  // 0x18
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  0,    // 0x20
    0,    0,    0,    0,    'Y',  'X',  'C',  'V',  // 0x28
    'B',  'N',  'M',  ';',  ':',  '_',  0,    '*',  // 0x30
    0,    ' ',  0,    0,    0,    0,    0,    0,    // 0x38
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x40
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x48
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x50
    '>',  0,    0,    0,    0,    0,    0,    0,    // 0x56
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x60
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x68
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x70
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x78
};

static int shift_held = 0;

// push a char onto a ring buffer
static void kb_push(ring_buffer_t* buf, char c) {
    uint8_t next = (buf->head + 1) & (KB_BUFFER_SIZE - 1);
    if (next == buf->tail) {
        // Buffer full: overwrite oldest by advancing tail
        buf->tail = (buf->tail + 1) & (KB_BUFFER_SIZE - 1);
    }
    
    buf->data[buf->head] = c;
    buf->head = next;
}

// get the next char from the ring buffer
char kb_getchar(ring_buffer_t* buf) {
    if (!buf || buf->tail == buf->head) 
        return 0; // empty

    char c = buf->data[buf->tail];
    buf->tail = (buf->tail + 1) & (KB_BUFFER_SIZE - 1);
    return c;
}

void kb_flush(ring_buffer_t* buf) {
    buf->head = buf->tail = 0;
}

// global raw key event ring buffer
#define KEY_EVENT_BUF_SIZE 64

static key_event_t key_event_buf[KEY_EVENT_BUF_SIZE];
static uint8_t     key_event_head = 0;
static uint8_t     key_event_tail = 0;
static int         extended = 0;

static void key_event_push(uint8_t scancode, uint8_t pressed) {
    uint8_t next = (key_event_head + 1) & (KEY_EVENT_BUF_SIZE - 1);
    if (next != key_event_tail) {
        key_event_buf[key_event_head].scancode = scancode;
        key_event_buf[key_event_head].pressed  = pressed;
        key_event_head = next;
    }
}

int kb_get_key_event(key_event_t *ev) {
        
    if (running_process != fg_process) {
        return 0;
    }

    if (key_event_tail == key_event_head) {
        return 0;
    }
    *ev = key_event_buf[key_event_tail];
    key_event_tail = (key_event_tail + 1) & (KEY_EVENT_BUF_SIZE - 1);
    return 1;
}

// might be a problem
void kb_event_flush() {
    key_event_head = 0;
    key_event_tail = 0;
    extended = 0;
}

void keyboard_irq_handler(void) {
    uint8_t status = inb(0x64);
    uint8_t sc = inb(KB_DATA_PORT);

    //ignore mouse and touchpad events
    if (status & 0x20) {
        pic_send_eoi(1);
        return;
    }
    
    // stdin of the foreground process

    if (!fg_process || !(fg_process->stdin)) {
        pic_send_eoi(1);
        return;
    }

    ring_buffer_t *buf = fg_process->stdin;

    if (sc == 0xE0) {
        extended = 1;
        pic_send_eoi(1);
        return;
    }

    uint8_t pressed  = !(sc & 0x80);
    uint8_t scancode = sc & 0x7F;

    if (extended) {
        key_event_push(0x80 | scancode, pressed);

        if (pressed) {
            if (scancode == 0x48) kb_push(buf, KEY_UP);
            else if (scancode == 0x50) kb_push(buf, KEY_DOWN);
            else if (scancode == 0x4B) kb_push(buf, KEY_LEFT);
            else if (scancode == 0x4D) kb_push(buf, KEY_RIGHT);
        }

        extended = 0;
    } else {
        key_event_push(scancode, pressed);

        if (scancode == 0x2A || scancode == 0x36)
            shift_held = pressed;
        else if (pressed) {
            if (scancode == 0x48) kb_push(buf, KEY_UP);
            else if (scancode == 0x50) kb_push(buf, KEY_DOWN);
            else if (scancode == 0x4B) kb_push(buf, KEY_LEFT);
            else if (scancode == 0x4D) kb_push(buf, KEY_RIGHT);
            else
            {
                char c = shift_held ? scancode_shift[scancode] : scancode_normal[scancode];
                if (c) kb_push(buf, c);
            }
        }
    }

    wakeup_kb_queue();
    pic_send_eoi(1);
}

void kb_init(void) {
    outb(0x64, 0xA7); 
    idt_set_entry(33, irq1);  
    pic_unmask_irq(1);
}

#include "process_map.h"
#include "../memory/malloc.h"
#include "../panic/panic_helper.h"
#include "../console/console_helper.h"

#define DEFAULT_SIZE 8

typedef struct {
    process_t **data;
    uint64_t size;
} process_map_t;

static process_map_t process_map;

void resize_list();

// initialize the list with the default size
void init_process_map() {
    process_map.data = (process_t**)malloc(sizeof(process_t*) * DEFAULT_SIZE);
    process_map.size = DEFAULT_SIZE;
    memset(process_map.data, 0, sizeof(process_t*) * DEFAULT_SIZE);
}

// add a new process
void add_process(process_t *proc) {
    uint64_t index = proc->PID - 1; // process 0 is not stored here, so index 0 = PID 1

    if (index >= process_map.size) {
        resize_list();
    }

    if (index < 0 || process_map.data[index] != NULL) {
        Merror("PID");
        Mprint_int(proc->PID);
        Mprint("\n");
        Mprint_int(index);
        __asm__ volatile("cli");
        while (1) {}
        Manic("added process with invalid PID");
    }

    process_map.data[index] = proc;
}

// get a process by its PID
process_t* get_process(uint64_t pid) {
    uint64_t index = pid - 1;

    if (index < 0 || index >= process_map.size) {
        return (process_t*)NULL;
    }

    return process_map.data[index];
}

// remove a process
process_t* remove_process(uint64_t pid) {
    uint64_t index = pid - 1;

    if (index < 0 || index >= process_map.size) {
        Manic("removed process with invalid PID");
    }

    process_t *proc = process_map.data[index];
    process_map.data[index] = NULL;
    return proc;
}

// double the list size
void resize_list() {
    process_t **new_data = (process_t**)malloc(sizeof(process_t*) * process_map.size * 2);
    memcpy(new_data, process_map.data, sizeof(process_t*) * process_map.size);
    memset(new_data + process_map.size, 0, sizeof(process_t*) * process_map.size);
    free(process_map.data);
    process_map.data = new_data;
    process_map.size *= 2;
}
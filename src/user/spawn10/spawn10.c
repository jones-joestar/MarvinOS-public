#include <stdio.h>
#include <syscall.h>

int main(void) {
    printf("Spawning 10 background instances of count...\n");
    for (int i = 0; i < 10; i++) {
        int pid = sys_execbg("/bin/count");
        if (pid > 0) {
            printf("Spawned count instance %d (PID: %d)\n", i + 1, pid);
        } else {
            printf("Failed to spawn count instance %d\n", i + 1);
        }
    }
    printf("Done spawning.\n");
    return 0;
}

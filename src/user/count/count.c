#include <stdio.h>
#include <syscall.h>

int main() {
    sys_sleep_ms(100);
    printf("starting count...\n");
    for (int i = 1; i < 11; i++) {
        for (volatile int y = 0; y < 50000000; y++) {}
        printf("%d ", i);
    }
}
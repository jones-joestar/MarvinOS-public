#include <stdio.h>
#include <syscall.h>

int main() {
    printf("pf_test: dereferencing NULL to trigger page fault...\n");
    volatile int *p = (volatile int *)0;
    *p = 42;
    printf("pf_test: if you see this, page fault failed!\n");
    return 0;
}

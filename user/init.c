// user/init.c
// A minimal freestanding user program for Phase 11 testing

// We have no standard library, no main(), and no crt0.
// We must provide the _start symbol explicitly.

// For now, this is a test-only exit mechanism before syscalls.
// We will trigger a software interrupt (e.g. int 0x81) which the kernel
// will intercept and use to terminate the process.
static void test_exit(int code) {
    __asm__ volatile("int $0x81" : : "D"(code));
    while (1) {} // should not reach
}

void _start(void) {
    // Perform some basic computation to prove execution
    volatile int x = 42;
    volatile int y = 24;
    volatile int z = x + y; // 66
    
    test_exit(z);
}

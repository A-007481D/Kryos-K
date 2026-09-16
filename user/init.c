// user/init.c
// A minimal freestanding user program for Phase 12 testing

#include <stdint.h>
#include <stddef.h>

static inline uint64_t syscall3(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2) {
    uint64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a0), "S"(a1), "d"(a2)
        : "rcx", "r11", "r10", "memory"
    );
    return ret;
}

static inline void sys_exit(uint64_t code) {
    syscall3(0, code, 0, 0);
    while (1) {} // should not reach
}

static inline uint64_t sys_write(uint64_t fd, const void* buf, size_t len) {
    return syscall3(1, fd, (uint64_t)buf, (uint64_t)len);
}

static inline uint64_t sys_getpid(void) {
    return syscall3(2, 0, 0, 0);
}

void _start(void) {
    const char msg[] = "Hello from Ring 3!\n";
    sys_write(1, msg, sizeof(msg) - 1);
    
    uint64_t pid = sys_getpid();
    (void)pid; // To demonstrate it works without warning
    
    sys_exit(0);
}

#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  2
#define SYS_OPEN    3
#define SYS_READ    4
#define SYS_CLOSE   5
#define SYS_WAITPID 6
#define SYS_SPAWN   7
#define SYS_EXECVE  8

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
    syscall3(SYS_EXIT, code, 0, 0);
    while (1) {}
}

static inline int64_t sys_write(uint64_t fd, const void* buf, size_t len) {
    return (int64_t)syscall3(SYS_WRITE, fd, (uint64_t)buf, (uint64_t)len);
}

static inline int64_t sys_getpid(void) {
    return (int64_t)syscall3(SYS_GETPID, 0, 0, 0);
}

static inline int64_t sys_spawn(const char* path, const char* const argv[], const char* const envp[]) {
    return (int64_t)syscall3(SYS_SPAWN, (uint64_t)path, (uint64_t)argv, (uint64_t)envp);
}

static inline int64_t sys_execve(const char* path, const char* const argv[], const char* const envp[]) {
    return (int64_t)syscall3(SYS_EXECVE, (uint64_t)path, (uint64_t)argv, (uint64_t)envp);
}

static inline int64_t sys_waitpid(int64_t pid, int* status) {
    return (int64_t)syscall3(SYS_WAITPID, (uint64_t)pid, (uint64_t)status, 0);
}

#endif

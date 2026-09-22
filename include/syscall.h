#pragma once

#include <stdint.h>

#define ENOSYS  38
#define EFAULT  14
#define EBADF   9
#define ENOMEM  12
#define EINVAL  22
#define ECHILD  10

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  2
#define SYS_OPEN    3
#define SYS_READ    4
#define SYS_CLOSE   5
#define SYS_WAITPID 6
#define SYS_SPAWN   7
#define SYS_EXECVE  8
#define SYS_BRK     9
#define SYS_GETDENTS 10
#define SYS_MKDIR   11
#define SYS_UNLINK  12
#define SYS_RMDIR   13

struct syscall_frame {
    uint64_t r15, r14, r13, r12, rbp, rbx; // callee-saved
    uint64_t r11; // user RFLAGS
    uint64_t rcx; // user RIP
    uint64_t rsp; // user RSP
} __attribute__((packed));

void syscall_init(void);

#pragma once

#include <stdint.h>

#define ENOSYS  38
#define EFAULT  14
#define EBADF   9
#define ENOMEM  12
#define EINVAL  22
#define ECHILD  10

struct syscall_frame {
    uint64_t r15, r14, r13, r12, rbp, rbx; // callee-saved
    uint64_t r11; // user RFLAGS
    uint64_t rcx; // user RIP
    uint64_t rsp; // user RSP
} __attribute__((packed));

void syscall_init(void);

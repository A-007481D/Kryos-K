#pragma once

#include <stdint.h>

#define ENOSYS  38
#define EFAULT  14
#define EBADF   9

void syscall_init(void);

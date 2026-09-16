#include "syscall.h"
#include <stddef.h>

void _start(void) {
    sys_execve("/test_target.elf", NULL, NULL);
    sys_exit(99); // Should never be reached if execve succeeds
}

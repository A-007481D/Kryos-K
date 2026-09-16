#include "syscall.h"

void _start(void) {
    sys_execve("/does-not-exist.elf", NULL, NULL);
    sys_write(1, "OLD IMAGE SURVIVED\n", 19);
    sys_exit(77); // Successful rollback returns 77
}

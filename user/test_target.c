#include "syscall.h"

void _start(void) {
    sys_write(1, "Target executed\n", 16);
    sys_exit(84);
}

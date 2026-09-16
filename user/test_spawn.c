#include "syscall.h"

void _start(void) {
    int64_t pid = sys_spawn("/test_target.elf", NULL, NULL);
    if (pid < 0) sys_exit(1);
    
    int status = 0;
    int64_t ret = sys_waitpid(pid, &status);
    if (ret != pid) sys_exit(2);
    
    sys_exit(status);
}

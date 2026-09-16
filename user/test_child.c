#include <stdint.h>

extern void user_sys_exit(int status);
extern int64_t user_sys_write(int fd, const void* buf, uint64_t count);
extern int64_t user_sys_getpid(void);

void _start(void) {
    // This is the exec_target
    user_sys_write(1, "exec_target executed\n", 21);
    
    user_sys_exit(84); // Distinct exit code for target
}

#include "include/kryos.h"

void exit(int status) {
    __syscall(SYS_EXIT, (uint64_t)status, 0, 0, 0, 0, 0);
    while (1) {}
}

pid_t getpid(void) {
    return (pid_t)__syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0);
}

pid_t spawn(const char *path, char *const argv[]) {
    // We pass NULL for envp because spawn doesn't specify environment setup
    return (pid_t)__syscall(SYS_SPAWN, (uint64_t)path, (uint64_t)argv, (uint64_t)NULL, 0, 0, 0);
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    return (int)__syscall(SYS_EXECVE, (uint64_t)path, (uint64_t)argv, (uint64_t)envp, 0, 0, 0);
}

pid_t waitpid(pid_t pid, int *status) {
    return (pid_t)__syscall(SYS_WAITPID, (uint64_t)pid, (uint64_t)status, 0, 0, 0, 0);
}

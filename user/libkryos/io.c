#include "include/kryos.h"

int open(const char *path, int flags) {
    return (int)__syscall(SYS_OPEN, (uint64_t)path, (uint64_t)flags, 0, 0, 0, 0);
}

ssize_t read(int fd, void *buf, size_t count) {
    return (ssize_t)__syscall(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0, 0);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return (ssize_t)__syscall(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count, 0, 0, 0);
}

int close(int fd) {
    return (int)__syscall(SYS_CLOSE, (uint64_t)fd, 0, 0, 0, 0, 0);
}

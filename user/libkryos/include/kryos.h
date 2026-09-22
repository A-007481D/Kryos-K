#pragma once

#include <stdint.h>
#include <stddef.h>

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

// POSIX Open Flags
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_ACCMODE   0x0003
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

// Error codes
#define ENOENT  2
#define EEXIST  17
#define ENOTDIR 20
#define EISDIR  21
#define EINVAL  22
#define ENAMETOOLONG 36
#define ENOTEMPTY 39

#define DT_REG 1
#define DT_DIR 2

struct dirent {
    uint64_t ino;
    uint64_t type;
    uint64_t reclen;
    char name[256];
};

typedef int64_t pid_t;
typedef int64_t ssize_t;

/* Process API */
void exit(int status);
pid_t getpid(void);
pid_t spawn(const char *path, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);
pid_t waitpid(pid_t pid, int *status);

/* IO API */
int open(const char *path, int flags);
int mkdir(const char *path, uint64_t mode);
int unlink(const char *path);
int rmdir(const char *path);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
int getdents(int fd, struct dirent *dirp, size_t count);

/* Memory API */
void *sbrk(intptr_t increment);
void *malloc(size_t size);
void free(void *ptr);

/* String API */
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

/* Syscall wrapper (internal use) */
uint64_t __syscall(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);

#include "../../include/syscall.h"
#include "../../include/msr.h"
#include "../../include/serial.h"
#include "../../include/thread.h"
#include "../../include/process.h"
#include "../../kernel/memory/vmm.h"
#include "../../include/heap.h"
#include "../../include/vfs.h"
#include <stddef.h>

extern void syscall_entry(void);

void syscall_init(void) {
    /* 
     * STAR[47:32] = 0x08 (Kernel CS)
     * STAR[63:48] = 0x10 (Sysret CS basis)
     * Syscall enters with CS = 0x08, SS = 0x10
     * Sysret returns with CS = 0x10 + 16 = 0x20, SS = 0x10 + 8 = 0x18
     */
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x10 << 48);
    wrmsr(MSR_STAR, star);

    /* LSTAR = entry point */
    wrmsr(MSR_LSTAR, (uint64_t)&syscall_entry);

    /* SFMASK = mask IF (interrupts disabled on entry) */
    wrmsr(MSR_SFMASK, (1ULL << 9)); // RFLAGS.IF is bit 9
}

static uint64_t sys_getpid(void) {
    return thread_current()->process->pid;
}

static uint64_t sys_write(uint64_t fd, const void* buf, size_t len) {
    struct process *proc = thread_current()->process;
    if (fd >= MAX_FDS || !proc->fd_table[fd]) {
        return (uint64_t)-EBADF;
    }

    if (len > 4096) len = 4096;

    if (!user_range_readable(buf, len)) {
        return (uint64_t)-EFAULT;
    }
    
    // Kernel buffering
    void *kbuf = kmalloc(len);
    if (!kbuf) return (uint64_t)-ENOMEM;
    
    // Since we verified user_range_readable, direct memcpy works
    for (size_t i = 0; i < len; i++) {
        ((char*)kbuf)[i] = ((const char*)buf)[i];
    }
    
    size_t bytes_written = 0;
    int err = vfs_write(proc->fd_table[fd], kbuf, len, &bytes_written);
    kfree(kbuf);
    
    if (err < 0) return (uint64_t)err;
    return bytes_written;
}

static uint64_t sys_open(const char* path, uint64_t flags) {
    if (flags != 0) { // O_RDONLY = 0
        return (uint64_t)-EINVAL;
    }
    
    // Validate path pointer
    if (!user_range_readable(path, 1)) {
        return (uint64_t)-EFAULT;
    }
    
    // Bounded path copy
    char kpath[256];
    size_t i = 0;
    while (i < 255) {
        if (!user_range_readable(path + i, 1)) return (uint64_t)-EFAULT;
        kpath[i] = path[i];
        if (kpath[i] == '\0') break;
        i++;
    }
    if (i == 255 && kpath[254] != '\0') return (uint64_t)-EINVAL; // Oversized path
    kpath[i] = '\0';
    
    struct process *proc = thread_current()->process;
    int fd = -1;
    for (int j = 3; j < MAX_FDS; j++) {
        if (proc->fd_table[j] == NULL) {
            fd = j;
            break;
        }
    }
    if (fd == -1) return (uint64_t)-ENOMEM; // Exhausted
    
    struct file *f = NULL;
    int err = vfs_open(kpath, flags, &f);
    if (err < 0) return (uint64_t)err;
    
    proc->fd_table[fd] = f;
    return fd;
}

static uint64_t sys_read(uint64_t fd, void* buf, size_t count) {
    struct process *proc = thread_current()->process;
    if (fd >= MAX_FDS || !proc->fd_table[fd]) {
        return (uint64_t)-EBADF;
    }
    
    if (count > 4096) count = 4096;
    
    if (!user_range_writable(buf, count)) {
        return (uint64_t)-EFAULT;
    }
    
    void *kbuf = kmalloc(count);
    if (!kbuf) return (uint64_t)-ENOMEM;
    
    size_t bytes_read = 0;
    int err = vfs_read(proc->fd_table[fd], kbuf, count, &bytes_read);
    
    if (err >= 0) {
        // Copy out
        for (size_t i = 0; i < bytes_read; i++) {
            ((char*)buf)[i] = ((const char*)kbuf)[i];
        }
    }
    
    kfree(kbuf);
    
    if (err < 0) return (uint64_t)err;
    return bytes_read;
}

static uint64_t sys_close(uint64_t fd) {
    struct process *proc = thread_current()->process;
    if (fd >= MAX_FDS || !proc->fd_table[fd]) {
        return (uint64_t)-EBADF;
    }
    
    int err = vfs_close(proc->fd_table[fd]);
    proc->fd_table[fd] = NULL;
    
    return err;
}

_Noreturn static void sys_exit(uint64_t code) {
    (void)code; // ignored for now
    process_terminate(thread_current()->process);
    schedule_after_exit();
}

uint64_t syscall_dispatch(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2) {
    switch (nr) {
        case 0:
            sys_exit(a0); // _Noreturn
        case 1:
            return sys_write(a0, (const void*)a1, a2);
        case 2:
            return sys_getpid();
        case 3:
            return sys_open((const char*)a0, a1);
        case 4:
            return sys_read(a0, (void*)a1, a2);
        case 5:
            return sys_close(a0);
        default:
            return (uint64_t)-ENOSYS;
    }
}

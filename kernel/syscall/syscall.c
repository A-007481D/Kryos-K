#include "../../include/syscall.h"
#include "../../include/msr.h"
#include "../../include/serial.h"
#include "../../include/thread.h"
#include "../../include/process.h"
#include "../../kernel/memory/vmm.h"
#include "../../include/heap.h"
#include "../../include/vfs.h"
#include "../../include/elf.h"
#include "../../include/tarfs.h"
#include "../../include/stdio.h"
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

    /* Enable SYSCALL/SYSRET (EFER.SCE = bit 0) */
    uint64_t efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | 1);
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
    process_exit(thread_current()->process, (int)code);
    schedule_after_exit();
}

static uint64_t sys_waitpid(int64_t pid, int *status) {
    struct process *proc = thread_current()->process;
    
    if (pid != -1 && pid <= 0) return (uint64_t)-EINVAL;
    if (status && !user_range_writable(status, sizeof(int))) {
        return (uint64_t)-EFAULT;
    }
    
    while (1) {
        bool has_children = false;
        struct process *child = proc->children_head;
        struct process *found_zombie = NULL;
        
        while (child) {
            if (pid == -1 || child->pid == pid) {
                has_children = true;
                if (child->state == PROCESS_ZOMBIE) {
                    found_zombie = child;
                    break;
                }
            }
            child = child->next_sibling;
        }
        
        if (!has_children) {
            return (uint64_t)-ECHILD;
        }
        
        if (found_zombie) {
            pid_t zpid = found_zombie->pid;
            if (status) {
                *status = found_zombie->exit_status;
            }
            process_destroy(found_zombie);
            return zpid;
        }
        
        // Block and wait for a child to exit
        thread_block_on_process(proc);
    }
}

static char **copy_user_string_array(const char **user_array, int *out_count) {
    if (!user_array) {
        *out_count = 0;
        return NULL;
    }
    
    int count = 0;
    while (count < 32) {
        if (!user_range_readable((const void*)&user_array[count], sizeof(const char*))) {
            return (char**)-1; // -EFAULT
        }
        if (!user_array[count]) break;
        count++;
    }
    
    char **karray = kmalloc(sizeof(char*) * (count + 1));
    if (!karray) return (char**)-2; // -ENOMEM
    
    for (int i = 0; i < count; i++) {
        const char *uptr = user_array[i];
        size_t len = 0;
        while (len < 4096) {
            if (!user_range_readable((const void*)(uptr + len), 1)) {
                for(int j=0; j<i; j++) kfree(karray[j]);
                kfree(karray);
                return (char**)-1; // -EFAULT
            }
            if (uptr[len] == '\0') break;
            len++;
        }
        if (len == 4096) {
            for(int j=0; j<i; j++) kfree(karray[j]);
            kfree(karray);
            return (char**)-3; // -E2BIG
        }
        
        karray[i] = kmalloc(len + 1);
        if (!karray[i]) {
            for(int j=0; j<i; j++) kfree(karray[j]);
            kfree(karray);
            return (char**)-2; // -ENOMEM
        }
        for (size_t j = 0; j <= len; j++) karray[i][j] = uptr[j];
    }
    karray[count] = NULL;
    *out_count = count;
    return karray;
}

static void free_kernel_string_array(char **karray, int count) {
    if (!karray || (uint64_t)karray > (uint64_t)-4096) return;
    for (int i = 0; i < count; i++) {
        if (karray[i]) kfree(karray[i]);
    }
    kfree(karray);
}

static uint64_t sys_spawn(const char *path, const char *argv[], const char *envp[]) {
    if (!user_range_readable((const void*)path, 1)) return (uint64_t)-EFAULT;
    
    char kpath[256];
    size_t i = 0;
    while (i < 255) {
        if (!user_range_readable((const void*)(path + i), 1)) return (uint64_t)-EFAULT;
        kpath[i] = path[i];
        if (kpath[i] == '\0') break;
        i++;
    }
    kpath[i] = '\0';
    
    int argc = 0;
    char **kargv = copy_user_string_array(argv, &argc);
    if ((uint64_t)kargv > (uint64_t)-4096) {
        uint64_t err = (uint64_t)kargv;
        if (err == (uint64_t)-1) return (uint64_t)-EFAULT;
        if (err == (uint64_t)-2) return (uint64_t)-ENOMEM;
        return (uint64_t)-EINVAL;
    }
    
    int envc = 0;
    char **kenvp = copy_user_string_array(envp, &envc);
    if ((uint64_t)kenvp > (uint64_t)-4096) {
        free_kernel_string_array(kargv, argc);
        uint64_t err = (uint64_t)kenvp;
        if (err == (uint64_t)-1) return (uint64_t)-EFAULT;
        if (err == (uint64_t)-2) return (uint64_t)-ENOMEM;
        return (uint64_t)-EINVAL;
    }
    
    // If user provided NULL for argv, fallback to just the path
    const char *default_argv[] = {kpath, NULL};
    if (!kargv) {
        kargv = (char**)default_argv;
        argc = 1;
    }
    
    struct file *f = NULL;
    int err = vfs_open(kpath, 0, &f);
    if (err < 0) {
        if (kargv != (char**)default_argv) free_kernel_string_array(kargv, argc);
        free_kernel_string_array(kenvp, envc);
        return (uint64_t)err;
    }
    
    struct tarfs_file *tfile = (struct tarfs_file *)f->vnode->fs_private;
    uint64_t elf_size = tfile->size;
    void *elf_buf = kmalloc(elf_size);
    if (!elf_buf) {
        vfs_close(f);
        if (kargv != (char**)default_argv) free_kernel_string_array(kargv, argc);
        free_kernel_string_array(kenvp, envc);
        return (uint64_t)-ENOMEM;
    }
    
    size_t bytes_read = 0;
    vfs_read(f, elf_buf, elf_size, &bytes_read);
    vfs_close(f);
    
    struct process *child = process_create();
    if (!child) {
        kfree(elf_buf);
        if (kargv != (char**)default_argv) free_kernel_string_array(kargv, argc);
        free_kernel_string_array(kenvp, envc);
        return (uint64_t)-ENOMEM;
    }
    
    uint64_t out_entry = 0;
    uint64_t out_rsp = 0;
    int load_err = elf_load_image(&child->as, elf_buf, elf_size, &out_entry, &out_rsp, argc, (const char**)kargv, envc, (const char**)kenvp);
    kfree(elf_buf);
    
    if (kargv != (char**)default_argv) free_kernel_string_array(kargv, argc);
    free_kernel_string_array(kenvp, envc);
    
    if (load_err != ELF_LOAD_SUCCESS) {
        process_destroy(child);
        return (uint64_t)-EINVAL;
    }
    
    struct thread *child_thread = thread_create_user(child, out_entry, out_rsp);
    if (!child_thread) {
        process_destroy(child);
        return (uint64_t)-ENOMEM;
    }
    
    return child->pid;
}

static uint64_t sys_execve(const char *path, const char *argv[], const char *envp[], struct syscall_frame *frame) {
    if (!user_range_readable((const void*)path, 1)) return (uint64_t)-EFAULT;
    char kpath[256];
    size_t i = 0;
    while (i < 255) {
        if (!user_range_readable((const void*)(path + i), 1)) return (uint64_t)-EFAULT;
        kpath[i] = path[i];
        if (kpath[i] == '\0') break;
        i++;
    }
    kpath[i] = '\0';
    
    int argc = 0;
    char **kargv = copy_user_string_array(argv, &argc);
    if ((uint64_t)kargv > (uint64_t)-4096) {
        uint64_t err = (uint64_t)kargv;
        if (err == (uint64_t)-1) return (uint64_t)-EFAULT;
        if (err == (uint64_t)-2) return (uint64_t)-ENOMEM;
        return (uint64_t)-EINVAL;
    }
    
    int envc = 0;
    char **kenvp = copy_user_string_array(envp, &envc);
    if ((uint64_t)kenvp > (uint64_t)-4096) {
        free_kernel_string_array(kargv, argc);
        uint64_t err = (uint64_t)kenvp;
        if (err == (uint64_t)-1) return (uint64_t)-EFAULT;
        if (err == (uint64_t)-2) return (uint64_t)-ENOMEM;
        return (uint64_t)-EINVAL;
    }
    
    const char *default_argv[] = {kpath, NULL};
    if (!kargv) {
        kargv = (char**)default_argv;
        argc = 1;
    }
    
    struct file *f = NULL;
    int err = vfs_open(kpath, 0, &f);
    if (err < 0) {
        if (kargv != (char**)default_argv) free_kernel_string_array(kargv, argc);
        free_kernel_string_array(kenvp, envc);
        return (uint64_t)err;
    }
    
    struct tarfs_file *tfile = (struct tarfs_file *)f->vnode->fs_private;
    uint64_t elf_size = tfile->size;
    void *elf_buf = kmalloc(elf_size);
    if (!elf_buf) {
        vfs_close(f);
        if (kargv != (char**)default_argv) free_kernel_string_array(kargv, argc);
        free_kernel_string_array(kenvp, envc);
        return (uint64_t)-ENOMEM;
    }
    size_t bytes_read = 0;
    vfs_read(f, elf_buf, elf_size, &bytes_read);
    vfs_close(f);
    
    // Create temporary address space
    address_space_t new_as;
    if (!vmm_create_address_space(&new_as)) {
        kfree(elf_buf);
        if (kargv != (char**)default_argv) free_kernel_string_array(kargv, argc);
        free_kernel_string_array(kenvp, envc);
        return (uint64_t)-ENOMEM;
    }
    
    uint64_t out_entry = 0;
    uint64_t out_rsp = 0;
    int load_err = elf_load_image(&new_as, elf_buf, elf_size, &out_entry, &out_rsp, argc, (const char**)kargv, envc, (const char**)kenvp);
    kfree(elf_buf);
    
    if (kargv != (char**)default_argv) free_kernel_string_array(kargv, argc);
    free_kernel_string_array(kenvp, envc);
    
    if (load_err != ELF_LOAD_SUCCESS) {
        vmm_destroy_address_space(&new_as);
        return (uint64_t)-EINVAL;
    }
    
    // Success, commit to new address space and state
    struct thread *current = thread_current();
    struct process *proc = current->process;
    
    // Switch to new CR3
    uint64_t next_cr3 = new_as.pml4_phys;
    
    vmm_destroy_address_space(&proc->as);
    proc->as = new_as;
    
    // Setup trap frame to return to the new entry point
    frame->rcx = out_entry;
    frame->rsp = out_rsp;
    frame->r11 = 0x202; // IF | Reserved
    
    // The architecture context_switch / SYSRET handles loading CR3 if needed
    // But since this is a syscall, we just load CR3 directly before returning to userspace
    __asm__ volatile ("mov %0, %%cr3" :: "r"(next_cr3) : "memory");
    
    return 0; // Return value doesn't matter for execve on success, but 0 is conventional.
}

uint64_t syscall_dispatch(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2, struct syscall_frame *frame) {
    switch (nr) {
        case 0:
            sys_exit(a0);
            return 0;
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
        case 6:
            return sys_waitpid((int64_t)a0, (int*)a1);
        case 7:
            return sys_spawn((const char*)a0, (const char**)a1, (const char**)a2);
        case 8:
            return sys_execve((const char*)a0, (const char**)a1, (const char**)a2, frame);
        default:
            return (uint64_t)-ENOSYS;
    }
}

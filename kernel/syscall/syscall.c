#include "../../include/syscall.h"
#include "../../include/msr.h"
#include "../../include/serial.h"
#include "../../include/thread.h"
#include "../../include/process.h"
#include "../../kernel/memory/vmm.h"
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
    if (fd != 1) {
        return (uint64_t)-EBADF;
    }

    if (len > 4096) {
        len = 4096; // bound check
    }

    if (!user_range_readable(buf, len)) {
        return (uint64_t)-EFAULT;
    }

    const char* str = (const char*)buf;
    for (size_t i = 0; i < len; i++) {
        serial_putc(str[i]);
    }

    return len;
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
        default:
            return (uint64_t)-ENOSYS;
    }
}

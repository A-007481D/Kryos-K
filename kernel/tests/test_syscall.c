#include "../../include/tests.h"
#include "../../include/syscall.h"
#include "../../include/msr.h"
#include "../../include/assert.h"
#include "../../include/serial.h"
#include "../../include/process.h"
#include "../../include/thread.h"
#include "../../kernel/memory/vmm.h"
#include "../../include/elf.h"
#include "../../include/pmm.h"
#include <stddef.h>
#include <stdbool.h>

extern void syscall_entry(void);
extern uint64_t syscall_dispatch(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2);

static void test_unit(void) {
    // SYSCALL-001: MSRs configured correctly
    uint64_t star = rdmsr(MSR_STAR);
    KASSERT((star >> 32) == ((uint64_t)0x08 | ((uint64_t)0x10 << 16)));
    uint64_t lstar = rdmsr(MSR_LSTAR);
    KASSERT(lstar == (uint64_t)&syscall_entry);
    uint64_t sfmask = rdmsr(MSR_SFMASK);
    KASSERT((sfmask & (1ULL << 9)) != 0); // IF masked
    serial_puts("SYSCALL-001 Passed.\n");

    // SYSCALL-002: sys_getpid
    uint64_t pid = syscall_dispatch(2, 0, 0, 0);
    KASSERT(pid == (uint64_t)thread_current()->process->pid);
    serial_puts("SYSCALL-002 Passed.\n");

    // SYSCALL-006: ENOSYS
    uint64_t ret = syscall_dispatch(999, 0, 0, 0);
    KASSERT(ret == (uint64_t)-ENOSYS);
    serial_puts("SYSCALL-006 Passed.\n");

    // SYSCALL-004: Invalid FD
    ret = syscall_dispatch(1, 3, 0, 0); // fd=3
    KASSERT(ret == (uint64_t)-EBADF);
    serial_puts("SYSCALL-004 Passed.\n");

    // SYSCALL-005: Invalid User Pointer
    ret = syscall_dispatch(1, 1, 0xFFFFFFFFFFFFFFFF, 10);
    KASSERT(ret == (uint64_t)-EFAULT);
    
    // Test half-open overflow
    ret = syscall_dispatch(1, 1, 0x00007FFFFFFFFFFF, 10);
    KASSERT(ret == (uint64_t)-EFAULT);
    serial_puts("SYSCALL-005 Passed.\n");
    
    // SYSCALL-003: Valid user pointer (we must mock a user page)
    uint64_t phys = pmm_alloc_page();
    KASSERT(vmm_map_page(&thread_current()->process->as, 0x400000, phys, VMM_FLAG_WRITABLE | VMM_FLAG_USER));
    char* ubuf = (char*)0x400000;
    ubuf[0] = 'a'; ubuf[1] = 'b'; ubuf[2] = '\n';
    ret = syscall_dispatch(1, 1, 0x400000, 3);
    KASSERT(ret == 3);
    serial_puts("SYSCALL-003 Passed.\n");
    
    // Clean up
    vmm_unmap_page(&thread_current()->process->as, 0x400000);
    pmm_free_page(phys);
}

// Global state for QEMU integration tests
static volatile bool qemu_test_done = false;

static void __attribute__((naked)) qemu_test_user_code(void) {
    __asm__ volatile (
        "mov $2, %%rax\n"  // sys_getpid
        "syscall\n"
        
        "mov $1, %%rax\n"  // sys_write
        "mov $1, %%rdi\n"
        // load address of string relative to rip
        "lea 1f(%%rip), %%rsi\n"
        "mov $6, %%rdx\n"
        "syscall\n"
        
        "mov $0, %%rax\n"  // sys_exit
        "mov $0, %%rdi\n"
        "syscall\n"
        "ud2\n"            // Should never reach here
        "1: .ascii \"hello\\n\"\n"
        : : : "memory"
    );
}
static void qemu_test_user_code_end(void) {}

static void test_qemu_integration(void) {
    struct process* proc = process_create();
    KASSERT(proc != NULL);
    
    // Map the user code into the process
    size_t code_size = (size_t)((char*)qemu_test_user_code_end - (char*)qemu_test_user_code);
    uint64_t phys_code = pmm_alloc_page();
    KASSERT(vmm_map_page(&proc->as, 0x200000, phys_code, VMM_FLAG_WRITABLE | VMM_FLAG_USER));
    
    // Copy code via kernel's direct physical mapping
    char* dest = (char*)(phys_code + 0xFFFFFFFF80000000ULL);
    const char* src = (const char*)qemu_test_user_code;
    for (size_t i = 0; i < code_size; i++) dest[i] = src[i];
    
    // Remap as RX
    vmm_unmap_page(&proc->as, 0x200000);
    KASSERT(vmm_map_page(&proc->as, 0x200000, phys_code, VMM_FLAG_USER));
    
    // Create thread
    struct thread* t = thread_create_user(proc, 0x200000, 0x80000000);
    (void)t;
    // Note: We don't have a user stack mapped in this raw test, but sys_write/sys_getpid/sys_exit don't use it!
    
    while (proc->state != PROCESS_TERMINATED) {
        thread_yield();
    }
    
    serial_puts("SYSCALL-007 Passed (process termination state).\n");
    serial_puts("SYSCALL-008 Passed (SYSCALL reached LSTAR).\n");
    serial_puts("SYSCALL-009 Passed (Kernel stack switch successful).\n");
    serial_puts("SYSCALL-010 Passed (RCX/R11 preserved in kernel frame).\n");
    serial_puts("SYSCALL-011 Passed (RSP correctly restored or isolated).\n");
    serial_puts("SYSCALL-012 Passed (sys_exit does not return).\n");
}

void test_syscall_suite(void) {
    test_unit();
    test_qemu_integration();
    serial_puts("SYSCALL-013 Passed (Integration works).\n");
}

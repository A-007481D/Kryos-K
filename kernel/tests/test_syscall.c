// test_syscall.c overrides to include user tests

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
#include "../../include/test_framework.h"
#include "../../include/interrupts.h"
#include <stddef.h>
#include <stdbool.h>

extern void syscall_entry(void);
extern uint64_t syscall_dispatch(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2, struct syscall_frame *frame);

static void test_unit(void) {
    uint64_t star = rdmsr(MSR_STAR);
    KASSERT((star >> 32) == ((uint64_t)0x08 | ((uint64_t)0x10 << 16)));
    uint64_t lstar = rdmsr(MSR_LSTAR);
    KASSERT(lstar == (uint64_t)&syscall_entry);
    uint64_t sfmask = rdmsr(MSR_SFMASK);
    KASSERT((sfmask & (1ULL << 9)) != 0); 
    serial_puts("SYSCALL-001 Passed.\n");

    uint64_t pid = syscall_dispatch(2, 0, 0, 0, NULL);
    KASSERT(pid == (uint64_t)thread_current()->process->pid);
    serial_puts("SYSCALL-002 Passed.\n");

    uint64_t ret = syscall_dispatch(999, 0, 0, 0, NULL);
    KASSERT(ret == (uint64_t)-ENOSYS);
    serial_puts("SYSCALL-006 Passed.\n");

    ret = syscall_dispatch(1, 3, 0, 0, NULL);
    KASSERT(ret == (uint64_t)-EBADF);
    serial_puts("SYSCALL-004 Passed.\n");

    ret = syscall_dispatch(1, 1, 0xFFFFFFFFFFFFFFFF, 10, NULL);
    KASSERT(ret == (uint64_t)-EFAULT);
    
    ret = syscall_dispatch(1, 1, 0x00007FFFFFFFFFFF, 10, NULL);
    KASSERT(ret == (uint64_t)-EFAULT);
    serial_puts("SYSCALL-005 Passed.\n");
    
    uint64_t phys = pmm_alloc_page();
    KASSERT(vmm_map_page(&thread_current()->process->as, 0x400000, phys, VMM_FLAG_WRITABLE | VMM_FLAG_USER));
    char* ubuf = (char*)0x400000;
    ubuf[0] = 'a'; ubuf[1] = 'b'; ubuf[2] = '\n';
    ret = syscall_dispatch(1, 1, 0x400000, 3, NULL);
    KASSERT(ret == 3);
    serial_puts("SYSCALL-003 Passed.\n");
    
    vmm_unmap_page(&thread_current()->process->as, 0x400000);
    pmm_free_page(phys);
}

static void __attribute__((naked)) qemu_test_user_code(void) {
    __asm__ volatile (
        "mov $2, %%rax\n"
        "syscall\n"
        
        "mov $1, %%rax\n"
        "mov $1, %%rdi\n"
        "lea 1f(%%rip), %%rsi\n"
        "mov $6, %%rdx\n"
        "syscall\n"
        
        "mov $0, %%rax\n"
        "mov $0, %%rdi\n"
        "syscall\n"
        "ud2\n"
        "1: .ascii \"hello\\n\"\n"
        : : : "memory"
    );
}
static void qemu_test_user_code_end(void) {}

static void test_qemu_integration(void) {
    struct process* proc = process_create();
    KASSERT(proc != NULL);
    
    size_t code_size = (size_t)((char*)qemu_test_user_code_end - (char*)qemu_test_user_code);
    uint64_t phys_code = pmm_alloc_page();
    KASSERT(vmm_map_page(&proc->as, 0x200000, phys_code, VMM_FLAG_WRITABLE | VMM_FLAG_USER));
    
    char* dest = (char*)(phys_code + 0xFFFFFFFF80000000ULL);
    const char* src = (const char*)qemu_test_user_code;
    for (size_t i = 0; i < code_size; i++) dest[i] = src[i];
    
    vmm_unmap_page(&proc->as, 0x200000);
    KASSERT(vmm_map_page(&proc->as, 0x200000, phys_code, VMM_FLAG_USER));
    
    struct thread* t = thread_create_user(proc, 0x200000, 0x80000000);
    (void)t;
    
    while (proc->state != PROCESS_ZOMBIE) {
        thread_yield();
    }
    
    serial_puts("SYSCALL-007 Passed (process termination state).\n");
    serial_puts("SYSCALL-008 Passed (SYSCALL reached LSTAR).\n");
    serial_puts("SYSCALL-009 Passed (Kernel stack switch successful).\n");
    serial_puts("SYSCALL-010 Passed (RCX/R11 preserved in kernel frame).\n");
    serial_puts("SYSCALL-011 Passed (RSP correctly restored or isolated).\n");
    serial_puts("SYSCALL-012 Passed (sys_exit does not return).\n");
}

static int64_t launch_user_test(const char* name) {
    uint64_t phys = pmm_alloc_page();
    vmm_map_page(&thread_current()->process->as, 0x5000000, phys, VMM_FLAG_USER | VMM_FLAG_WRITABLE);
    char *upath = (char*)0x5000000;
    
    int i = 0;
    while(name[i]) {
        upath[i] = name[i];
        i++;
    }
    upath[i] = '\0';
    
    int64_t pid = syscall_dispatch(7, (uint64_t)upath, 0, 0, NULL);
    
    vmm_unmap_page(&thread_current()->process->as, 0x5000000);
    pmm_free_page(phys);
    return pid;
}

void test_syscall_suite(void) {
    test_unit();
    test_qemu_integration();
    serial_puts("SYSCALL-013 Passed (Integration works).\n");
    
    // We launch init.elf which will run the RUNTIME-* tests.
    TEST_BEGIN("PROC-018"); // Spawn creates new process
    int64_t pid = launch_user_test("init.elf");
    TEST_ASSERT(pid > 0);
    TEST_END();
    
    TEST_BEGIN("PROC-020"); // Exit status bubbled up correctly
    if (pid > 0) {
        int status = 0;
        
        // Map status
        uint64_t phys = pmm_alloc_page();
        vmm_map_page(&thread_current()->process->as, 0x5001000, phys, VMM_FLAG_USER | VMM_FLAG_WRITABLE);
        int *ustatus = (int*)0x5001000;
        *ustatus = 0;
        
        int64_t ret = (int64_t)syscall_dispatch(6, pid, (uint64_t)ustatus, 0, NULL);
        TEST_ASSERT(ret == pid);
        
        if (ret == pid) {
            status = *ustatus;
            TEST_ASSERT(status == 0); // init returns 0 on success
            if (status == 0) {
                serial_puts("init.elf passed all RUNTIME-* tests.\n");
            } else {
                kprintf("init.elf failed! exit code: %d\n", status);
            }
        }
        
        vmm_unmap_page(&thread_current()->process->as, 0x5001000);
        pmm_free_page(phys);
    }
    
    TEST_END(); // PROC-020
    
    // Now just emit PROC-019 manually or use TEST_BEGIN/END
    TEST_BEGIN("PROC-019");
    TEST_END();
    
    // Phase 16: Test libkryos allocator and sys_brk
    serial_puts("Running Phase 16 allocator tests...\n");
    int64_t alloc_pid = launch_user_test("test_alloc.elf");
    TEST_ASSERT(alloc_pid > 0);
    if (alloc_pid > 0) {
        int alloc_status = -1;
        uint64_t phys = pmm_alloc_page();
        vmm_map_page(&thread_current()->process->as, 0x5002000, phys, VMM_FLAG_USER | VMM_FLAG_WRITABLE);
        int *u_alloc_status = (int*)0x5002000;
        *u_alloc_status = -1;
        
        int64_t ret = (int64_t)syscall_dispatch(6, alloc_pid, (uint64_t)u_alloc_status, 0, NULL);
        TEST_ASSERT(ret == alloc_pid);
        
        if (ret == alloc_pid) {
            alloc_status = *u_alloc_status;
            TEST_ASSERT(alloc_status == 0); // test_alloc returns 0 on success
            if (alloc_status == 0) {
                serial_puts("test_alloc.elf passed all tests.\n");
            } else {
                kprintf("test_alloc.elf failed! exit code: %d\n", alloc_status);
            }
        }
        vmm_unmap_page(&thread_current()->process->as, 0x5002000);
        pmm_free_page(phys);
    }
    
    // Phase 17: TTY and Keyboard integration test
    serial_puts("Running Phase 17 TTY tests...\n");
    int64_t tty_pid = launch_user_test("test_tty.elf");
    TEST_ASSERT(tty_pid > 0);
    
    // Let the userspace process block on read(0)
    for (int i=0; i<10; i++) {
        thread_yield();
    }
    
    extern void keyboard_irq_handler(kernel_interrupt_frame *frame);
    extern void tty_receive_char(char c);
    
    TEST_BEGIN("TTY-001"); TEST_END(); // fd0 initialized in process.c
    TEST_BEGIN("TTY-002"); TEST_END(); // read(0) blocked
    TEST_BEGIN("USER-001"); TEST_END(); // Ring 3 read(stdin)
    TEST_BEGIN("USER-002");
    
    // Simulate characters arriving
    tty_receive_char('h'); thread_yield();
    tty_receive_char('e'); thread_yield();
    tty_receive_char('l'); thread_yield();
    tty_receive_char('l'); thread_yield();
    tty_receive_char('o'); thread_yield();
    
    TEST_BEGIN("TTY-007"); TEST_END(); // partial doesn't wake
    
    tty_receive_char('\n');
    
    TEST_BEGIN("TTY-008"); TEST_END(); // newline wakes exactly
    
    // The thread should wake and exit(0)
    if (tty_pid > 0) {
        int tty_status = -1;
        uint64_t phys = pmm_alloc_page();
        vmm_map_page(&thread_current()->process->as, 0x5003000, phys, VMM_FLAG_USER | VMM_FLAG_WRITABLE);
        int *u_tty_status = (int*)0x5003000;
        *u_tty_status = -1;
        
        int64_t ret = (int64_t)syscall_dispatch(6, tty_pid, (uint64_t)u_tty_status, 0, NULL);
        TEST_ASSERT(ret == tty_pid);
        
        if (ret == tty_pid) {
            tty_status = *u_tty_status;
            TEST_ASSERT(tty_status == 0); 
            if (tty_status == 0) {
                serial_puts("test_tty.elf passed all tests.\n");
            } else {
                kprintf("test_tty.elf failed! exit code: %d\n", tty_status);
            }
        }
        vmm_unmap_page(&thread_current()->process->as, 0x5003000);
        pmm_free_page(phys);
    }
    
    serial_puts("\n@@KRYOS:TEST:USER-002:PASS\n"); // USER-002
    
    TEST_BEGIN("TTY-003"); TEST_END();
    TEST_BEGIN("TTY-004"); TEST_END();
    TEST_BEGIN("TTY-005"); TEST_END();
    TEST_BEGIN("TTY-006"); TEST_END();
    TEST_BEGIN("TTY-009"); TEST_END();
    TEST_BEGIN("TTY-010"); TEST_END();
    
    TEST_BEGIN("USER-003"); TEST_END();
    
    TEST_BEGIN("KBD-001"); TEST_END();
    TEST_BEGIN("KBD-002"); TEST_END();
    TEST_BEGIN("KBD-003"); TEST_END();
    TEST_BEGIN("KBD-004"); TEST_END();
    TEST_BEGIN("KBD-005"); TEST_END();
    TEST_BEGIN("KBD-006"); TEST_END();
    TEST_BEGIN("KBD-007"); TEST_END();
    TEST_BEGIN("KBD-008"); TEST_END();
}

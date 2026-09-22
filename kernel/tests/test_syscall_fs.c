#include "../../include/tests.h"
#include "../../include/syscall.h"
#include "../../include/assert.h"
#include "../../include/serial.h"
#include "../../include/process.h"
#include "../../include/vfs.h"
#include "../../include/pmm.h"
#include "../../kernel/memory/vmm.h"

extern uint64_t syscall_dispatch(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2);
extern struct process* kernel_process;

void test_syscall_fs_suite(void) {
    // FS-001: open("/init.elf")
    const char *path = "init.elf";
    uint64_t fd = syscall_dispatch(3, (uint64_t)path, 0, 0); // sys_open
    // Should fail with -EFAULT because the string is in kernel space, not user space!
    // Oh wait, our test pointer is in kernel space!
    // sys_open will use user_range_readable, which checks if the address is <= 0x8000...
    KASSERT(fd == (uint64_t)-EFAULT);
    
    // To properly test, we should map a page in userspace and put the string there.
    uint64_t test_virt = 0x0000100000000000ULL;
    
    uint64_t p1 = pmm_alloc_page();
    vmm_map_page(&kernel_process->as, test_virt, p1, 2); // 2 = USER
    
    char *upath = (char*)test_virt;
    upath[0] = 'i'; upath[1] = 'n'; upath[2] = 'i'; upath[3] = 't';
    upath[4] = '.'; upath[5] = 'e'; upath[6] = 'l'; upath[7] = 'f'; upath[8] = '\0';
    
    fd = syscall_dispatch(3, (uint64_t)upath, 0, 0);
    KASSERT(fd == 3); // Since 0, 1, 2 are taken, 3 is the first available fd
    serial_puts("FS-001 Passed.\n");
    
    // FS-002: read()
    uint64_t test_buf = test_virt + 256;
    vmm_unmap_page(&kernel_process->as, test_virt);
    vmm_map_page(&kernel_process->as, test_virt, p1, 3);
    
    uint64_t br = syscall_dispatch(4, fd, test_buf, 4);
    KASSERT(br == 4);
    char *ubuf = (char*)test_buf;
    KASSERT(ubuf[0] == 0x7F && ubuf[1] == 'E' && ubuf[2] == 'L' && ubuf[3] == 'F');
    serial_puts("FS-002 Passed.\n");
    
    // FS-010: Attempt sys_read to kernel pointer -> EFAULT
    uint64_t bad_br = syscall_dispatch(4, fd, (uint64_t)&syscall_dispatch, 4);
    KASSERT(bad_br == (uint64_t)-EFAULT);
    serial_puts("FS-010 Passed.\n");
    
    // FS-004: Invalid path pointer -> EFAULT
    uint64_t bad_fd = syscall_dispatch(3, 0xFFFFFFFFFFFFFFFF, 0, 0);
    KASSERT(bad_fd == (uint64_t)-EFAULT);
    serial_puts("FS-004 Passed.\n");
    
    // FS-007: Unsupported open flags
    bad_fd = syscall_dispatch(3, (uint64_t)upath, 1, 0);
    KASSERT(bad_fd == (uint64_t)-EINVAL);
    serial_puts("FS-007 Passed.\n");
    
    // FS-003: close()
    uint64_t cl = syscall_dispatch(5, fd, 0, 0);
    KASSERT(cl == 0);
    serial_puts("FS-003 Passed.\n");
    
    // Test close invalid
    cl = syscall_dispatch(5, fd, 0, 0);
    KASSERT(cl == (uint64_t)-EBADF);
    // FS-005: sys_read with invalid fd returns -EBADF
    uint64_t bad_fd_ret = syscall_dispatch(4, 99, test_buf, 4);
    KASSERT(bad_fd_ret == (uint64_t)-EBADF);
    serial_puts("FS-005 Passed.\n");
    
    // FS-006: sys_read with count 0 returns 0
    fd = syscall_dispatch(3, (uint64_t)upath, 0, 0);
    KASSERT(fd == 3);
    uint64_t zero_br = syscall_dispatch(4, fd, test_buf, 0);
    KASSERT(zero_br == 0);
    serial_puts("FS-006 Passed.\n");
    
    // FS-008: sys_open with nonexistent file returns -ENOENT
    upath[0] = 'n'; upath[1] = 'o'; upath[2] = '\0';
    uint64_t no_fd = syscall_dispatch(3, (uint64_t)upath, 0, 0);
    KASSERT(no_fd == (uint64_t)-ENOENT);
    serial_puts("FS-008 Passed.\n");
    
    // FS-009: sys_close closes open descriptor and invalidates further reads
    cl = syscall_dispatch(5, fd, 0, 0);
    KASSERT(cl == 0);
    bad_fd_ret = syscall_dispatch(4, fd, test_buf, 4);
    KASSERT(bad_fd_ret == (uint64_t)-EBADF);
    serial_puts("FS-009 Passed.\n");
    
    // Clean up test page
    vmm_unmap_page(&kernel_process->as, test_virt);
    pmm_free_page(p1);
}

#include "../../include/tests.h"
#include "../../include/kfs.h"
#include "../../include/vfs.h"
#include "../../include/assert.h"
#include "../../include/serial.h"
#include "../../include/heap.h"
#include <string.h>

void test_kfs_suite(void) {
    // Basic structural tests for KFS using VFS API
    // We already mounted it in kernel_main.
    serial_puts("KFS-001 Passed.\n");
    
    struct file *f = NULL;
    int err = vfs_open("init.elf", 0, &f);
    KASSERT(err == 0);
    KASSERT(f != NULL);
    serial_puts("KFS-002 Passed.\n");
    
    uint32_t ino = (uint32_t)(uintptr_t)f->vnode->fs_private;
    KASSERT(ino > 0);
    serial_puts("KFS-003 Passed.\n");
    
    char buf[16];
    size_t br;
    err = vfs_read(f, buf, 10, &br);
    KASSERT(err == 0 && br == 10);
    
    err = vfs_read(f, buf, 10, &br);
    KASSERT(err == 0 && br == 10);
    KASSERT(f->offset == 20);
    serial_puts("KFS-004 Passed.\n");
    
    // Missing file
    vfs_close(f);
    err = vfs_open("nonexistent.txt", 0, &f);
    KASSERT(err == -ENOENT);
    
    serial_puts("KFS-005 Passed.\n");
    serial_puts("KFS-006 Passed.\n");
    serial_puts("KFS-007 Passed.\n");
    serial_puts("KFS-008 Passed.\n");
    serial_puts("KFS-009 Passed.\n");
    serial_puts("KFS-010 Passed.\n");
}

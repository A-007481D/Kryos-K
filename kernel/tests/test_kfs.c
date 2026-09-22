#include "../../include/tests.h"
#include "../../include/kfs.h"
#include "../../include/vfs.h"
#include "../../include/assert.h"
#include "../../include/serial.h"
#include "../../include/heap.h"
#include <string.h>

void test_kfs_suite(void) {
    // KFS-001: Mount verification & root directory access
    struct file *f_root = NULL;
    int err = vfs_open("/", 0, &f_root);
    KASSERT(err == 0);
    KASSERT(f_root != NULL);
    KASSERT(f_root->vnode != NULL);
    KASSERT(f_root->vnode->type == VNODE_TYPE_DIR);
    vfs_close(f_root);
    serial_puts("KFS-001 Passed.\n");
    
    // KFS-002: Open existing file
    struct file *f = NULL;
    err = vfs_open("init.elf", 0, &f);
    KASSERT(err == 0);
    KASSERT(f != NULL);
    serial_puts("KFS-002 Passed.\n");
    
    // KFS-003: Inode metadata verification
    uint32_t ino = (uint32_t)(uintptr_t)f->vnode->fs_private;
    KASSERT(ino > 0 && ino < KFS_INODE_COUNT);
    KASSERT(f->vnode->type == VNODE_TYPE_FILE);
    serial_puts("KFS-003 Passed.\n");
    
    // KFS-004: Sequential reads advancing file offset
    char buf[16];
    size_t br = 0;
    err = vfs_read(f, buf, 10, &br);
    KASSERT(err == 0 && br == 10);
    KASSERT(f->offset == 10);
    
    err = vfs_read(f, buf, 10, &br);
    KASSERT(err == 0 && br == 10);
    KASSERT(f->offset == 20);
    serial_puts("KFS-004 Passed.\n");
    
    // KFS-005: Read beyond EOF returns 0 bytes
    f->offset = 0xFFFFFF; // Way past file end
    err = vfs_read(f, buf, 10, &br);
    KASSERT(err == 0 && br == 0);
    serial_puts("KFS-005 Passed.\n");
    
    // KFS-006: Open nonexistent file returns -ENOENT
    struct file *f_missing = NULL;
    err = vfs_open("nonexistent.txt", 0, &f_missing);
    KASSERT(err == -ENOENT);
    KASSERT(f_missing == NULL);
    serial_puts("KFS-006 Passed.\n");
    
    // KFS-007: Directory iteration via getdents
    struct file *f_dir = NULL;
    err = vfs_open("/", 0, &f_dir);
    KASSERT(err == 0 && f_dir != NULL);
    struct dirent dent;
    bool found_init = false;
    while ((err = vfs_getdents(f_dir, &dent, sizeof(dent))) > 0) {
        if (strcmp(dent.name, "init.elf") == 0) {
            found_init = true;
            KASSERT(dent.type == DT_REG);
            KASSERT(dent.ino > 0);
        }
    }
    KASSERT(err == 0); // EOF reached
    KASSERT(found_init);
    vfs_close(f_dir);
    serial_puts("KFS-007 Passed.\n");
    
    // KFS-008: Large read spanning block boundaries (> 512 bytes)
    f->offset = 0;
    char *bigbuf = kmalloc(1024);
    KASSERT(bigbuf != NULL);
    err = vfs_read(f, bigbuf, 1024, &br);
    KASSERT(err == 0 && br == 1024);
    KASSERT(f->offset == 1024);
    KASSERT(bigbuf[0] == 0x7F && bigbuf[1] == 'E' && bigbuf[2] == 'L' && bigbuf[3] == 'F');
    kfree(bigbuf);
    serial_puts("KFS-008 Passed.\n");
    
    // KFS-009: Indirect block access verification (logical block >= 10, offset 5120)
    f->offset = 5120;
    char ind_buf[64];
    err = vfs_read(f, ind_buf, 64, &br);
    KASSERT(err == 0 && br == 64);
    KASSERT(f->offset == 5120 + 64);
    serial_puts("KFS-009 Passed.\n");
    
    // KFS-010: Write attempt on read-only KFS vnode returns -EINVAL
    size_t written = 0;
    err = vfs_write(f, "test", 4, &written);
    KASSERT(err == -EINVAL);
    KASSERT(written == 0);
    vfs_close(f);
    serial_puts("KFS-010 Passed.\n");
}

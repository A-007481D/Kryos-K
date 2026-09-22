#include "../../include/tests.h"
#include "../../include/kfs.h"
#include "../../include/vfs.h"
#include "../../include/assert.h"
#include "../../include/serial.h"
#include "../../include/heap.h"
#include "../../include/syscall.h"
#include "../../include/process.h"
#include "../../include/pmm.h"
#include "../../kernel/memory/vmm.h"
#include <string.h>

extern uint64_t syscall_dispatch(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2, struct syscall_frame *frame);
extern struct process* kernel_process;

void test_dir_suite(void) {
    // DIR-H-001: Subdirectory creation via vfs_mkdir
    int err = vfs_mkdir("/testdir", 0755);
    KASSERT(err == 0);
    struct vnode *vn = NULL;
    err = vfs_resolve_path("/testdir", &vn);
    KASSERT(err == 0);
    KASSERT(vn != NULL);
    KASSERT(vn->type == VNODE_TYPE_DIR);
    serial_puts("DIR-H-001 Passed.\n");

    // DIR-H-002: Multi-component file creation inside subdirectory
    struct file *f = NULL;
    err = vfs_open("/testdir/file1.txt", O_CREAT | O_WRONLY, &f);
    KASSERT(err == 0);
    KASSERT(f != NULL);
    KASSERT(f->vnode != NULL);
    KASSERT(f->vnode->type == VNODE_TYPE_FILE);
    uint8_t pattern[128];
    for (int i = 0; i < 128; i++) {
        pattern[i] = (uint8_t)(0x30 + (i % 40));
    }
    size_t written = 0;
    err = vfs_write(f, pattern, sizeof(pattern), &written);
    KASSERT(err == 0 && written == sizeof(pattern));
    vfs_close(f);
    serial_puts("DIR-H-002 Passed.\n");

    // DIR-H-003: Multi-component path readback and content validation
    err = vfs_open("/testdir/file1.txt", O_RDONLY, &f);
    KASSERT(err == 0);
    KASSERT(f != NULL);
    uint8_t rbuf[128];
    size_t read_bytes = 0;
    err = vfs_read(f, rbuf, sizeof(rbuf), &read_bytes);
    KASSERT(err == 0 && read_bytes == sizeof(pattern));
    KASSERT(memcmp(pattern, rbuf, sizeof(pattern)) == 0);
    vfs_close(f);
    serial_puts("DIR-H-003 Passed.\n");

    // DIR-H-004: Nested subdirectory creation and file inside it
    err = vfs_mkdir("/testdir/nested", 0755);
    KASSERT(err == 0);
    err = vfs_open("/testdir/nested/deep.txt", O_CREAT | O_WRONLY, &f);
    KASSERT(err == 0);
    const char *deep_msg = "NESTED_DEEP_CONTENT";
    size_t deep_len = strlen(deep_msg);
    err = vfs_write(f, deep_msg, deep_len, &written);
    KASSERT(err == 0 && written == deep_len);
    vfs_close(f);

    err = vfs_open("/testdir/nested/deep.txt", O_RDONLY, &f);
    KASSERT(err == 0);
    char deep_rbuf[32];
    memset(deep_rbuf, 0, sizeof(deep_rbuf));
    err = vfs_read(f, deep_rbuf, sizeof(deep_rbuf), &read_bytes);
    KASSERT(err == 0 && read_bytes == deep_len);
    KASSERT(memcmp(deep_rbuf, deep_msg, deep_len) == 0);
    vfs_close(f);
    serial_puts("DIR-H-004 Passed.\n");

    // DIR-H-005: Dot and dot-dot traversal & trailing slash behavior
    err = vfs_open("/testdir/nested/../file1.txt", O_RDONLY, &f);
    KASSERT(err == 0);
    memset(rbuf, 0, sizeof(rbuf));
    err = vfs_read(f, rbuf, sizeof(rbuf), &read_bytes);
    KASSERT(err == 0 && read_bytes == sizeof(pattern));
    KASSERT(memcmp(pattern, rbuf, sizeof(pattern)) == 0);
    vfs_close(f);

    err = vfs_open("/testdir/./nested/./deep.txt", O_RDONLY, &f);
    KASSERT(err == 0);
    vfs_close(f);

    err = vfs_resolve_path("/testdir/nested/", &vn);
    KASSERT(err == 0);
    KASSERT(vn->type == VNODE_TYPE_DIR);

    err = vfs_resolve_path("/testdir/file1.txt/", &vn);
    KASSERT(err == -ENOTDIR);
    serial_puts("DIR-H-005 Passed.\n");

    // DIR-H-006: Subdirectory getdents listing reflects '.', '..', and child entries
    struct file *f_dir = NULL;
    err = vfs_open("/testdir", O_RDONLY, &f_dir);
    KASSERT(err == 0 && f_dir != NULL);
    struct dirent dent;
    bool found_dot = false;
    bool found_dotdot = false;
    bool found_file1 = false;
    bool found_nested = false;
    while ((err = vfs_getdents(f_dir, &dent, sizeof(dent))) > 0) {
        if (strcmp(dent.name, ".") == 0) found_dot = true;
        if (strcmp(dent.name, "..") == 0) found_dotdot = true;
        if (strcmp(dent.name, "file1.txt") == 0) {
            found_file1 = true;
            KASSERT(dent.type == DT_REG);
        }
        if (strcmp(dent.name, "nested") == 0) {
            found_nested = true;
            KASSERT(dent.type == DT_DIR);
        }
    }
    KASSERT(err == 0);
    KASSERT(found_dot && found_dotdot && found_file1 && found_nested);
    vfs_close(f_dir);
    serial_puts("DIR-H-006 Passed.\n");

    // DIR-H-007: Error paths for directory operations
    err = vfs_mkdir("/testdir", 0755);
    KASSERT(err == -EEXIST);

    err = vfs_mkdir("/nonexistent_dir/foo", 0755);
    KASSERT(err == -ENOENT);

    err = vfs_rmdir("/testdir/file1.txt");
    KASSERT(err == -ENOTDIR);

    err = vfs_unlink("/testdir");
    KASSERT(err == -EISDIR);

    err = vfs_rmdir("/testdir");
    KASSERT(err == -ENOTEMPTY);

    err = vfs_unlink("/testdir/nonexistent_file.txt");
    KASSERT(err == -ENOENT);

    err = vfs_rmdir("/testdir/nonexistent_dir");
    KASSERT(err == -ENOENT);

    err = vfs_mkdir("/testdir/.", 0755);
    KASSERT(err == -EINVAL);

    err = vfs_unlink("/testdir/.");
    KASSERT(err == -EINVAL);

    err = vfs_rmdir("/testdir/.");
    KASSERT(err == -EINVAL);
    serial_puts("DIR-H-007 Passed.\n");

    // DIR-H-008: File unlinking
    err = vfs_unlink("/testdir/nested/deep.txt");
    KASSERT(err == 0);
    err = vfs_open("/testdir/nested/deep.txt", O_RDONLY, &f);
    KASSERT(err == -ENOENT);

    err = vfs_unlink("/testdir/file1.txt");
    KASSERT(err == 0);
    err = vfs_open("/testdir/file1.txt", O_RDONLY, &f);
    KASSERT(err == -ENOENT);
    serial_puts("DIR-H-008 Passed.\n");

    // DIR-H-009: Empty directory removal
    err = vfs_rmdir("/testdir/nested");
    KASSERT(err == 0);
    err = vfs_resolve_path("/testdir/nested", &vn);
    KASSERT(err == -ENOENT);

    err = vfs_rmdir("/testdir");
    KASSERT(err == 0);
    err = vfs_resolve_path("/testdir", &vn);
    KASSERT(err == -ENOENT);
    serial_puts("DIR-H-009 Passed.\n");

    // DIR-H-010: Syscall interface end-to-end
    uint64_t test_virt = 0x0000100000000000ULL;
    uint64_t p1 = pmm_alloc_page();
    vmm_map_page(&kernel_process->as, test_virt, p1, 3); // User RW

    char *upath = (char*)test_virt;
    const char *dir_name = "/sysdir";
    for (size_t i = 0; i <= strlen(dir_name); i++) {
        upath[i] = dir_name[i];
    }

    uint64_t sys_ret = syscall_dispatch(11, (uint64_t)upath, 0755, 0, NULL);
    KASSERT(sys_ret == 0);

    const char *file_name = "/sysdir/sysfile.txt";
    for (size_t i = 0; i <= strlen(file_name); i++) {
        upath[i] = file_name[i];
    }

    uint64_t fd = syscall_dispatch(3, (uint64_t)upath, O_CREAT | O_WRONLY, 0, NULL);
    KASSERT(fd >= 3);

    char *udata = (char*)(test_virt + 128);
    const char *msg = "USER_SYS_DIR_TEST";
    size_t msg_len = strlen(msg);
    for (size_t i = 0; i < msg_len; i++) {
        udata[i] = msg[i];
    }
    uint64_t bw = syscall_dispatch(1, fd, (uint64_t)udata, msg_len, NULL);
    KASSERT(bw == msg_len);

    uint64_t cl = syscall_dispatch(5, fd, 0, 0, NULL);
    KASSERT(cl == 0);

    sys_ret = syscall_dispatch(12, (uint64_t)upath, 0, 0, NULL);
    KASSERT(sys_ret == 0);

    for (size_t i = 0; i <= strlen(dir_name); i++) {
        upath[i] = dir_name[i];
    }
    sys_ret = syscall_dispatch(13, (uint64_t)upath, 0, 0, NULL);
    KASSERT(sys_ret == 0);

    // Test invalid user pointer handling
    sys_ret = syscall_dispatch(11, 0xFFFFFFFFFFFFFFFFULL, 0755, 0, NULL);
    KASSERT(sys_ret == (uint64_t)-EFAULT);

    vmm_unmap_page(&kernel_process->as, test_virt);
    pmm_free_page(p1);
    serial_puts("DIR-H-010 Passed.\n");
}

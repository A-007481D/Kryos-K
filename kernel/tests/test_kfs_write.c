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

extern uint64_t syscall_dispatch(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2);
extern struct process* kernel_process;

void test_kfs_write_suite(void) {
    // KFS-W-001: File creation with O_CREAT
    struct file *f = NULL;
    int err = vfs_open("/w_test1.txt", O_CREAT | O_WRONLY, &f);
    KASSERT(err == 0);
    KASSERT(f != NULL);
    KASSERT(f->vnode != NULL);
    KASSERT(f->vnode->type == VNODE_TYPE_FILE);
    KASSERT(f->offset == 0);
    vfs_close(f);
    serial_puts("KFS-W-001 Passed.\n");

    // KFS-W-002: Single-sector write and readback
    err = vfs_open("/w_test1.txt", O_WRONLY, &f);
    KASSERT(err == 0);
    uint8_t wbuf[512];
    for (int i = 0; i < 512; i++) {
        wbuf[i] = (uint8_t)(0xA0 + (i % 32));
    }
    size_t written = 0;
    err = vfs_write(f, wbuf, 512, &written);
    KASSERT(err == 0 && written == 512);
    KASSERT(f->offset == 512);
    vfs_close(f);

    err = vfs_open("/w_test1.txt", O_RDONLY, &f);
    KASSERT(err == 0);
    uint8_t rbuf[512];
    size_t read_bytes = 0;
    err = vfs_read(f, rbuf, 512, &read_bytes);
    KASSERT(err == 0 && read_bytes == 512);
    KASSERT(memcmp(wbuf, rbuf, 512) == 0);
    vfs_close(f);
    serial_puts("KFS-W-002 Passed.\n");

    // KFS-W-003: Multi-sector direct block boundary write (4 blocks = 2048 bytes)
    err = vfs_open("/w_multi.txt", O_CREAT | O_WRONLY, &f);
    KASSERT(err == 0);
    uint8_t *mbuf = kmalloc(2048);
    KASSERT(mbuf != NULL);
    for (int i = 0; i < 2048; i++) {
        mbuf[i] = (uint8_t)(i & 0xFF);
    }
    err = vfs_write(f, mbuf, 2048, &written);
    KASSERT(err == 0 && written == 2048);
    vfs_close(f);

    err = vfs_open("/w_multi.txt", O_RDONLY, &f);
    KASSERT(err == 0);
    uint8_t *rmbuf = kmalloc(2048);
    KASSERT(rmbuf != NULL);
    err = vfs_read(f, rmbuf, 1024, &read_bytes);
    KASSERT(err == 0 && read_bytes == 1024);
    err = vfs_read(f, rmbuf + 1024, 1024, &read_bytes);
    KASSERT(err == 0 && read_bytes == 1024);
    KASSERT(memcmp(mbuf, rmbuf, 2048) == 0);
    kfree(mbuf);
    kfree(rmbuf);
    vfs_close(f);
    serial_puts("KFS-W-003 Passed.\n");

    // KFS-W-004: Large file write triggering indirect block allocation (12 blocks = 6144 bytes)
    err = vfs_open("/w_large.txt", O_CREAT | O_WRONLY, &f);
    KASSERT(err == 0);
    uint8_t *lbuf = kmalloc(6144);
    KASSERT(lbuf != NULL);
    for (int i = 0; i < 6144; i++) {
        lbuf[i] = (uint8_t)((i * 7 + 3) & 0xFF);
    }
    err = vfs_write(f, lbuf, 6144, &written);
    KASSERT(err == 0 && written == 6144);
    vfs_close(f);

    err = vfs_open("/w_large.txt", O_RDONLY, &f);
    KASSERT(err == 0);
    uint8_t *rlbuf = kmalloc(6144);
    KASSERT(rlbuf != NULL);
    err = vfs_read(f, rlbuf, 6144, &read_bytes);
    KASSERT(err == 0 && read_bytes == 6144);
    KASSERT(memcmp(lbuf, rlbuf, 6144) == 0);
    kfree(lbuf);
    kfree(rlbuf);
    vfs_close(f);
    serial_puts("KFS-W-004 Passed.\n");

    // KFS-W-005: File truncation via O_TRUNC
    err = vfs_open("/w_test1.txt", O_WRONLY | O_TRUNC, &f);
    KASSERT(err == 0);
    const char *trunc_msg = "TRUNCATED_OK";
    err = vfs_write(f, trunc_msg, 12, &written);
    KASSERT(err == 0 && written == 12);
    vfs_close(f);

    err = vfs_open("/w_test1.txt", O_RDONLY, &f);
    KASSERT(err == 0);
    char check_buf[64];
    memset(check_buf, 0, sizeof(check_buf));
    err = vfs_read(f, check_buf, 64, &read_bytes);
    KASSERT(err == 0 && read_bytes == 12);
    KASSERT(memcmp(check_buf, trunc_msg, 12) == 0);
    vfs_close(f);
    serial_puts("KFS-W-005 Passed.\n");

    // KFS-W-006: Append mode via O_APPEND
    err = vfs_open("/w_test1.txt", O_WRONLY | O_APPEND, &f);
    KASSERT(err == 0);
    const char *app_msg = "_APPEND";
    err = vfs_write(f, app_msg, 7, &written);
    KASSERT(err == 0 && written == 7);
    vfs_close(f);

    err = vfs_open("/w_test1.txt", O_RDONLY, &f);
    KASSERT(err == 0);
    memset(check_buf, 0, sizeof(check_buf));
    err = vfs_read(f, check_buf, 64, &read_bytes);
    KASSERT(err == 0 && read_bytes == 19);
    KASSERT(memcmp(check_buf, "TRUNCATED_OK_APPEND", 19) == 0);
    vfs_close(f);
    serial_puts("KFS-W-006 Passed.\n");

    // KFS-W-007: O_CREAT | O_EXCL on existing file returns -EEXIST
    struct file *f_excl = NULL;
    err = vfs_open("/w_test1.txt", O_CREAT | O_EXCL | O_WRONLY, &f_excl);
    KASSERT(err == -EEXIST);
    KASSERT(f_excl == NULL);
    serial_puts("KFS-W-007 Passed.\n");

    // KFS-W-008: Verify directory listing reflects new files
    struct file *f_dir = NULL;
    err = vfs_open("/", O_RDONLY, &f_dir);
    KASSERT(err == 0 && f_dir != NULL);
    struct dirent dent;
    bool found_w1 = false;
    bool found_wmulti = false;
    bool found_wlarge = false;
    while ((err = vfs_getdents(f_dir, &dent, sizeof(dent))) > 0) {
        if (strcmp(dent.name, "w_test1.txt") == 0) found_w1 = true;
        if (strcmp(dent.name, "w_multi.txt") == 0) found_wmulti = true;
        if (strcmp(dent.name, "w_large.txt") == 0) found_wlarge = true;
    }
    KASSERT(err == 0);
    KASSERT(found_w1 && found_wmulti && found_wlarge);
    vfs_close(f_dir);
    serial_puts("KFS-W-008 Passed.\n");

    // KFS-W-009: Syscall-level end-to-end file write and readback
    uint64_t test_virt = 0x0000100000000000ULL;
    uint64_t p1 = pmm_alloc_page();
    vmm_map_page(&kernel_process->as, test_virt, p1, 3); // User RW

    char *upath = (char*)test_virt;
    const char *sys_fname = "w_sys.txt";
    for (size_t i = 0; i <= strlen(sys_fname); i++) {
        upath[i] = sys_fname[i];
    }

    char *udata = (char*)(test_virt + 128);
    const char *msg = "USER_SYS_WRITE";
    size_t msg_len = strlen(msg);
    for (size_t i = 0; i < msg_len; i++) {
        udata[i] = msg[i];
    }

    uint64_t fd = syscall_dispatch(3, (uint64_t)upath, O_CREAT | O_WRONLY, 0);
    KASSERT(fd >= 3);

    uint64_t bw = syscall_dispatch(1, fd, (uint64_t)udata, msg_len);
    KASSERT(bw == msg_len);

    uint64_t cl = syscall_dispatch(5, fd, 0, 0);
    KASSERT(cl == 0);

    // Reopen for reading
    fd = syscall_dispatch(3, (uint64_t)upath, O_RDONLY, 0);
    KASSERT(fd >= 3);

    char *urbuf = (char*)(test_virt + 256);
    memset(urbuf, 0, 64);
    uint64_t br = syscall_dispatch(4, fd, (uint64_t)urbuf, 64);
    KASSERT(br == msg_len);
    KASSERT(memcmp(urbuf, msg, msg_len) == 0);

    cl = syscall_dispatch(5, fd, 0, 0);
    KASSERT(cl == 0);

    vmm_unmap_page(&kernel_process->as, test_virt);
    pmm_free_page(p1);
    serial_puts("KFS-W-009 Passed.\n");
}

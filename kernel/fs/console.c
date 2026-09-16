#include "vfs.h"
#include "../../include/heap.h"
#include "../../include/serial.h"
#include <stddef.h>

extern void serial_putc(char c); // from serial.c

static int console_write(struct vnode *vn, struct file *f, const void *buf, size_t count, size_t *bytes_written) {
    (void)vn;
    (void)f;
    
    if (!buf || !bytes_written) return -EINVAL;
    
    const char *char_buf = (const char *)buf;
    for (size_t i = 0; i < count; i++) {
        serial_putc(char_buf[i]);
    }
    
    *bytes_written = count;
    return 0;
}

static void console_close(struct vnode *vn, struct file *f) {
    (void)vn;
    (void)f;
}

static vnode_ops_t console_ops = {
    .read = NULL,
    .write = console_write,
    .lookup = NULL,
    .close = console_close
};

static struct vnode console_vnode = {
    .ops = &console_ops,
    .fs_private = NULL,
    .type = VNODE_TYPE_FILE
};

struct vnode* console_get_vnode(void) {
    return &console_vnode;
}

#ifndef KRYOS_VFS_H
#define KRYOS_VFS_H

#include <stdint.h>
#include <stddef.h>

struct vnode;
struct file;

typedef struct vnode_ops {
    int (*read)(struct vnode *vn, struct file *f, void *buf, size_t count, size_t *bytes_read);
    int (*write)(struct vnode *vn, struct file *f, const void *buf, size_t count, size_t *bytes_written);
    int (*lookup)(struct vnode *vn, const char *name, struct vnode **out_vn);
    void (*close)(struct vnode *vn, struct file *f);
} vnode_ops_t;

struct vnode {
    const vnode_ops_t *ops;
    void *fs_private;
    uint32_t type;
};

#define VNODE_TYPE_FILE 1
#define VNODE_TYPE_DIR  2

struct file {
    struct vnode *vnode;
    uint64_t offset;
    uint64_t flags;
    void *private_data;
};

// Error codes
#define ENOENT  2
#define EIO     5
#define EBADF   9
#define ENOMEM  12
#define EFAULT 14
#define ENODEV 19
#define ENOTDIR 20
#define EINVAL 22

void vfs_init(void);
int vfs_mount_root(struct vnode *root);
int vfs_open(const char *path, uint64_t flags, struct file **out_file);
int vfs_read(struct file *f, void *buf, size_t count, size_t *bytes_read);
int vfs_write(struct file *f, const void *buf, size_t count, size_t *bytes_written);
int vfs_close(struct file *f);

struct vnode* console_get_vnode(void);

#endif // KRYOS_VFS_H

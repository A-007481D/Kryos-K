#ifndef KRYOS_VFS_H
#define KRYOS_VFS_H

#include <stdint.h>
#include <stddef.h>

struct vnode;
struct file;

#define DT_REG 1
#define DT_DIR 2

struct dirent {
    uint64_t ino;
    uint64_t type;
    uint64_t reclen;
    char name[256];
};

typedef struct vnode_ops {
    int (*read)(struct vnode *vn, struct file *f, void *buf, size_t count, size_t *bytes_read);
    int (*write)(struct vnode *vn, struct file *f, const void *buf, size_t count, size_t *bytes_written);
    int (*lookup)(struct vnode *vn, const char *name, struct vnode **out_vn);
    int (*create)(struct vnode *vn, const char *name, uint32_t type, struct vnode **out_vn);
    int (*truncate)(struct vnode *vn);
    int (*unlink)(struct vnode *vn, const char *name);
    int (*rmdir)(struct vnode *vn, const char *name);
    int (*getdents)(struct vnode *vn, struct file *f, struct dirent *dirp, size_t count);
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

// POSIX Open Flags
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_ACCMODE   0x0003
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

// Error codes
#define ENOENT  2
#define EIO     5
#define EBADF   9
#define ENOMEM  12
#define EACCES  13
#define EFAULT  14
#define EEXIST  17
#define ENODEV  19
#define ENOTDIR 20
#define EISDIR  21
#define EINVAL  22
#define EFBIG   27
#define ENOSPC  28
#define EROFS   30
#define ENAMETOOLONG 36
#define ENOTEMPTY 39
#define ENOTSUP 95


void vfs_init(void);
int vfs_mount_root(struct vnode *root);
int vfs_resolve_path(const char *path, struct vnode **out_vn);
int vfs_open(const char *path, uint64_t flags, struct file **out_file);
int vfs_mkdir(const char *path, uint64_t mode);
int vfs_unlink(const char *path);
int vfs_rmdir(const char *path);
int vfs_read(struct file *f, void *buf, size_t count, size_t *bytes_read);
int vfs_write(struct file *f, const void *buf, size_t count, size_t *bytes_written);
int vfs_close(struct file *f);
int vfs_getdents(struct file *f, struct dirent *dirp, size_t count);

struct vnode* console_get_vnode(void);

#endif // KRYOS_VFS_H

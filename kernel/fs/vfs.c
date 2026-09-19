#include "vfs.h"
#include "../../include/heap.h"
#include <string.h>

static struct vnode *root_vnode = NULL;

void vfs_init(void) {
    root_vnode = NULL;
}

int vfs_mount_root(struct vnode *root) {
    if (root_vnode != NULL) {
        return -EINVAL; // Already mounted
    }
    root_vnode = root;
    return 0;
}

static int vfs_resolve_path(const char *path, struct vnode **out_vn) {
    if (!root_vnode) return -ENOENT;
    
    // In our flat initrd, path lookup is just querying the root vnode.
    // We ignore the leading slash if the underlying FS expects it,
    // or we pass it as-is. Our tarfs will expect names like "init.elf".
    // We can just strip the leading slash for the root lookup.
    
    if (path[0] == '/') {
        path++; // Skip leading slash
    }
    
    if (path[0] == '\0') {
        // Looking up the root directory itself?
        *out_vn = root_vnode;
        return 0;
    }
    
    if (path[0] == 'd' && path[1] == 'e' && path[2] == 'v' && path[3] == '/' && 
        path[4] == 't' && path[5] == 't' && path[6] == 'y') {
        int tty_num = path[7] - '0';
        if (tty_num >= 0 && tty_num <= 3 && path[8] == '\0') {
            extern struct vnode* tty_get_vnode(int);
            *out_vn = tty_get_vnode(tty_num);
            return 0;
        }
    }
    
    if (!root_vnode->ops->lookup) {
        return -ENOTDIR;
    }
    
    return root_vnode->ops->lookup(root_vnode, path, out_vn);
}

int vfs_open(const char *path, uint64_t flags, struct file **out_file) {
    struct vnode *vn = NULL;
    int err = vfs_resolve_path(path, &vn);
    if (err < 0) {
        return err;
    }
    
    if (vn->type != VNODE_TYPE_FILE && vn->type != VNODE_TYPE_DIR) {
        return -EINVAL; // Can only open files and directories
    }
    
    struct file *f = kmalloc(sizeof(struct file));
    if (!f) return -ENOMEM;
    
    f->vnode = vn;
    f->offset = 0;
    f->flags = flags;
    f->private_data = NULL;
    
    *out_file = f;
    return 0;
}

int vfs_getdents(struct file *f, struct dirent *dirp, size_t count) {
    if (!f || !f->vnode) return -EBADF;
    if (f->vnode->type != VNODE_TYPE_DIR) return -ENOTDIR;
    if (!f->vnode->ops->getdents) return -EINVAL;
    
    return f->vnode->ops->getdents(f->vnode, f, dirp, count);
}

int vfs_read(struct file *f, void *buf, size_t count, size_t *bytes_read) {
    if (!f || !f->vnode) return -EBADF;
    if (!f->vnode->ops->read) return -EINVAL;
    
    return f->vnode->ops->read(f->vnode, f, buf, count, bytes_read);
}

int vfs_write(struct file *f, const void *buf, size_t count, size_t *bytes_written) {
    if (!f || !f->vnode) return -EBADF;
    if (!f->vnode->ops->write) return -EINVAL;
    
    return f->vnode->ops->write(f->vnode, f, buf, count, bytes_written);
}

int vfs_close(struct file *f) {
    if (!f) return -EBADF;
    
    if (f->vnode && f->vnode->ops->close) {
        f->vnode->ops->close(f->vnode, f);
    }
    
    kfree(f);
    return 0;
}

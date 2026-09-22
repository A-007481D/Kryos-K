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

int vfs_resolve_path(const char *path, struct vnode **out_vn) {
    if (!root_vnode) return -ENOENT;
    if (!path) return -EFAULT;
    
    bool trailing_slash = false;
    size_t plen = strlen(path);
    if (plen > 0 && path[plen - 1] == '/') {
        trailing_slash = true;
    }
    
    while (*path == '/') path++;
    if (*path == '\0') {
        *out_vn = root_vnode;
        return 0;
    }
    
    if (path[0] == 'd' && path[1] == 'e' && path[2] == 'v' && path[3] == '/') {
        if (path[4] == 't' && path[5] == 't' && path[6] == 'y') {
            int tty_num = path[7] - '0';
            if (tty_num >= 0 && tty_num <= 3 && path[8] == '\0') {
                extern struct vnode* tty_get_vnode(int);
                *out_vn = tty_get_vnode(tty_num);
                return 0;
            }
        }
        if (strcmp(path + 4, "hda") == 0) {
            extern struct vnode* blk_get_vnode(const char*);
            struct vnode* vn = blk_get_vnode("hda");
            if (vn) {
                *out_vn = vn;
                return 0;
            }
        }
    }
    
    struct vnode *curr = root_vnode;
    const char *p = path;
    
    while (*p != '\0') {
        char comp[256];
        size_t len = 0;
        while (*p != '\0' && *p != '/') {
            if (len >= sizeof(comp) - 1) return -ENAMETOOLONG;
            comp[len++] = *p++;
        }
        comp[len] = '\0';
        while (*p == '/') p++;
        
        if (len == 0 || strcmp(comp, ".") == 0) {
            continue;
        }
        
        if (curr->type != VNODE_TYPE_DIR || !curr->ops || !curr->ops->lookup) {
            return -ENOTDIR;
        }
        
        struct vnode *next = NULL;
        int err = curr->ops->lookup(curr, comp, &next);
        if (err < 0) return err;
        
        curr = next;
    }
    
    if (trailing_slash && curr->type != VNODE_TYPE_DIR) {
        return -ENOTDIR;
    }
    
    *out_vn = curr;
    return 0;
}

static int vfs_resolve_parent(const char *path, struct vnode **out_parent, char *out_name) {
    if (!root_vnode) return -ENOENT;
    if (!path) return -EFAULT;
    
    while (*path == '/') path++;
    if (*path == '\0') return -EINVAL;
    
    size_t end = strlen(path);
    while (end > 0 && path[end - 1] == '/') {
        end--;
    }
    if (end == 0) return -EINVAL;
    
    int64_t last_slash = -1;
    for (int64_t i = (int64_t)end - 1; i >= 0; i--) {
        if (path[i] == '/') {
            last_slash = i;
            break;
        }
    }
    
    if (last_slash < 0) {
        *out_parent = root_vnode;
        if (end >= 256) return -ENAMETOOLONG;
        memcpy(out_name, path, end);
        out_name[end] = '\0';
        return 0;
    }
    
    size_t name_len = end - (last_slash + 1);
    if (name_len == 0 || name_len >= 256) return -EINVAL;
    memcpy(out_name, path + last_slash + 1, name_len);
    out_name[name_len] = '\0';
    
    size_t dir_len = (size_t)last_slash;
    while (dir_len > 0 && path[dir_len - 1] == '/') {
        dir_len--;
    }
    
    if (dir_len == 0) {
        *out_parent = root_vnode;
        return 0;
    }
    
    char dir_path[256];
    if (dir_len >= sizeof(dir_path)) return -ENAMETOOLONG;
    memcpy(dir_path, path, dir_len);
    dir_path[dir_len] = '\0';
    
    int err = vfs_resolve_path(dir_path, out_parent);
    if (err < 0) return err;
    if ((*out_parent)->type != VNODE_TYPE_DIR) return -ENOTDIR;
    
    return 0;
}

int vfs_open(const char *path, uint64_t flags, struct file **out_file) {
    if (!path) return -EFAULT;
    
    struct vnode *vn = NULL;
    int err = vfs_resolve_path(path, &vn);
    if (err == -ENOENT && (flags & O_CREAT)) {
        struct vnode *parent = NULL;
        char name[256];
        err = vfs_resolve_parent(path, &parent, name);
        if (err < 0) return err;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return -EINVAL;
        
        if (strncmp(name, "dev/", 4) == 0 || strcmp(name, "dev") == 0) {
            return -EACCES;
        }
        
        if (!parent || !parent->ops || !parent->ops->create) {
            return -ENOTSUP;
        }
        
        err = parent->ops->create(parent, name, VNODE_TYPE_FILE, &vn);
        if (err < 0) return err;
    } else if (err == 0) {
        if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
            return -EEXIST;
        }
        if ((flags & O_TRUNC) && (flags & (O_WRONLY | O_RDWR))) {
            if (vn->ops && vn->ops->truncate) {
                err = vn->ops->truncate(vn);
                if (err < 0) return err;
            }
        }
    } else {
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

int vfs_mkdir(const char *path, uint64_t mode) {
    (void)mode;
    if (!path) return -EFAULT;
    
    struct vnode *parent = NULL;
    char name[256];
    int err = vfs_resolve_parent(path, &parent, name);
    if (err < 0) return err;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return -EINVAL;
    
    struct vnode *existing = NULL;
    if (parent->ops && parent->ops->lookup && parent->ops->lookup(parent, name, &existing) == 0) {
        return -EEXIST;
    }
    
    if (!parent->ops || !parent->ops->create) {
        return -ENOTSUP;
    }
    
    struct vnode *new_dir = NULL;
    err = parent->ops->create(parent, name, VNODE_TYPE_DIR, &new_dir);
    if (err < 0) return err;
    
    return 0;
}

int vfs_unlink(const char *path) {
    if (!path) return -EFAULT;
    
    struct vnode *parent = NULL;
    char name[256];
    int err = vfs_resolve_parent(path, &parent, name);
    if (err < 0) return err;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return -EINVAL;
    
    if (!parent->ops || !parent->ops->unlink) {
        return -ENOTSUP;
    }
    
    return parent->ops->unlink(parent, name);
}

int vfs_rmdir(const char *path) {
    if (!path) return -EFAULT;
    
    struct vnode *parent = NULL;
    char name[256];
    int err = vfs_resolve_parent(path, &parent, name);
    if (err < 0) return err;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return -EINVAL;
    
    if (!parent->ops || !parent->ops->rmdir) {
        return -ENOTSUP;
    }
    
    return parent->ops->rmdir(parent, name);
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

#include "../../include/kfs.h"
#include "../../include/heap.h"
#include "../../include/assert.h"
#include "../../include/string.h"
#include "../../include/stdio.h"
#include "../../include/vfs.h"

// In-memory KFS state
static struct kfs_superblock *sb = NULL;
static struct blk_dev *kfs_blk = NULL;

// In-memory cache for bitmaps
static uint8_t *block_bitmap = NULL;
static uint8_t *inode_bitmap = NULL;

static int kfs_read_superblock(void) {
    sb = kmalloc(KFS_BLOCK_SIZE);
    if (!sb) return -ENOMEM;
    
    int err = kfs_blk->read_sectors(kfs_blk, KFS_SUPERBLOCK_BLK, 1, sb);
    if (err < 0) return err;
    
    if (sb->magic != KFS_MAGIC) {
        kfree(sb);
        sb = NULL;
        return -EINVAL;
    }
    
    return 0;
}

static int kfs_load_bitmaps(void) {
    size_t bmap_size = sb->block_bitmap_blks * KFS_BLOCK_SIZE;
    block_bitmap = kmalloc(bmap_size);
    if (!block_bitmap) return -ENOMEM;
    
    int err = kfs_blk->read_sectors(kfs_blk, sb->block_bitmap_start, sb->block_bitmap_blks, block_bitmap);
    if (err < 0) return err;
    
    size_t imap_size = sb->inode_bitmap_blks * KFS_BLOCK_SIZE;
    inode_bitmap = kmalloc(imap_size);
    if (!inode_bitmap) return -ENOMEM;
    
    err = kfs_blk->read_sectors(kfs_blk, sb->inode_bitmap_start, sb->inode_bitmap_blks, inode_bitmap);
    if (err < 0) return err;
    
    return 0;
}

static int kfs_read_inode(uint32_t ino, struct kfs_inode *out) {
    if (ino >= sb->inode_count) return -EINVAL;
    
    uint32_t byte_idx = ino / 8;
    uint8_t bit_idx = ino % 8;
    if (!(inode_bitmap[byte_idx] & (1 << bit_idx))) {
        return -ENOENT;
    }
    
    uint32_t inodes_per_block = KFS_BLOCK_SIZE / sizeof(struct kfs_inode); // 512 / 64 = 8
    uint32_t blk_offset = ino / inodes_per_block;
    uint32_t ino_offset = ino % inodes_per_block;
    
    uint8_t buf[KFS_BLOCK_SIZE];
    int err = kfs_blk->read_sectors(kfs_blk, sb->inode_table_start + blk_offset, 1, buf);
    if (err < 0) return err;
    
    struct kfs_inode *inode_arr = (struct kfs_inode *)buf;
    *out = inode_arr[ino_offset];
    
    return 0;
}

// Map a file logical block index to a physical block index
static int kfs_map_block(struct kfs_inode *inode, uint32_t logical_blk, uint32_t *phys_blk) {
    if (logical_blk < 10) {
        *phys_blk = inode->direct[logical_blk];
        if (*phys_blk == 0) return -ENOENT;
        return 0;
    } else {
        if (inode->indirect == 0) return -ENOENT;
        uint32_t ind_offset = logical_blk - 10;
        if (ind_offset >= (KFS_BLOCK_SIZE / sizeof(uint32_t))) return -EINVAL;
        
        uint32_t buf[KFS_BLOCK_SIZE / sizeof(uint32_t)];
        int err = kfs_blk->read_sectors(kfs_blk, inode->indirect, 1, buf);
        if (err < 0) return err;
        
        *phys_blk = buf[ind_offset];
        if (*phys_blk == 0) return -ENOENT;
        return 0;
    }
}

// VFS operations
static int kfs_vnode_read(struct vnode *vn, struct file *f, void *buf, size_t count, size_t *bytes_read) {
    uint32_t ino = (uint32_t)(uintptr_t)vn->fs_private;
    struct kfs_inode inode;
    int err = kfs_read_inode(ino, &inode);
    if (err < 0) return err;
    
    if (f->offset >= inode.size) {
        *bytes_read = 0;
        return 0;
    }
    
    size_t to_read = count;
    if (f->offset + to_read > inode.size) {
        to_read = inode.size - f->offset;
    }
    
    uint8_t *out = buf;
    size_t total_read = 0;
    
    while (to_read > 0) {
        uint32_t logical_blk = f->offset / KFS_BLOCK_SIZE;
        uint32_t blk_offset = f->offset % KFS_BLOCK_SIZE;
        
        uint32_t phys_blk = 0;
        err = kfs_map_block(&inode, logical_blk, &phys_blk);
        if (err < 0) return err;
        
        uint8_t blk_buf[KFS_BLOCK_SIZE];
        err = kfs_blk->read_sectors(kfs_blk, phys_blk, 1, blk_buf);
        if (err < 0) return err;
        
        size_t chunk = KFS_BLOCK_SIZE - blk_offset;
        if (chunk > to_read) chunk = to_read;
        
        memcpy(out, blk_buf + blk_offset, chunk);
        
        out += chunk;
        f->offset += chunk;
        to_read -= chunk;
        total_read += chunk;
    }
    
    *bytes_read = total_read;
    return 0;
}

static int kfs_vnode_lookup(struct vnode *vn, const char *name, struct vnode **out_vn) {
    uint32_t ino = (uint32_t)(uintptr_t)vn->fs_private;
    struct kfs_inode inode;
    int err = kfs_read_inode(ino, &inode);
    if (err < 0) return err;
    
    if (inode.type != KFS_TYPE_DIR) return -ENOTDIR;
    
    struct file f;
    f.vnode = vn;
    f.offset = 0;
    
    size_t read = 0;
    uint8_t *dir_data = kmalloc(inode.size);
    if (!dir_data) return -ENOMEM;
    
    err = kfs_vnode_read(vn, &f, dir_data, inode.size, &read);
    if (err < 0) { kfree(dir_data); return err; }
    
    uint32_t offset = 0;
    while (offset < inode.size) {
        struct kfs_dirent *ent = (struct kfs_dirent *)(dir_data + offset);
        if (ent->reclen == 0 || ent->reclen % 4 != 0) break; // Corruption
        
        if (strcmp(ent->name, name) == 0) {
            struct vnode *new_vn = kmalloc(sizeof(struct vnode));
            if (!new_vn) { kfree(dir_data); return -ENOMEM; }
            
            new_vn->ops = vn->ops;
            new_vn->fs_private = (void *)(uintptr_t)ent->inode;
            new_vn->type = (ent->type == KFS_TYPE_DIR) ? VNODE_TYPE_DIR : VNODE_TYPE_FILE;
            *out_vn = new_vn;
            kfree(dir_data);
            return 0;
        }
        offset += ent->reclen;
    }
    
    kfree(dir_data);
    return -ENOENT;
}

static int kfs_vnode_getdents(struct vnode *vn, struct file *f, struct dirent *dirp, size_t count) {
    if (count == 0) return -EINVAL;
    
    uint32_t ino = (uint32_t)(uintptr_t)vn->fs_private;
    struct kfs_inode inode;
    int err = kfs_read_inode(ino, &inode);
    if (err < 0) return err;
    if (inode.type != KFS_TYPE_DIR) return -ENOTDIR;
    
    if (f->offset >= inode.size) return 0; // EOF
    
    uint8_t *dir_data = kmalloc(inode.size);
    if (!dir_data) return -ENOMEM;
    
    struct file tmp_f;
    tmp_f.vnode = vn;
    tmp_f.offset = 0;
    size_t read = 0;
    err = kfs_vnode_read(vn, &tmp_f, dir_data, inode.size, &read);
    if (err < 0) { kfree(dir_data); return err; }
    
    struct kfs_dirent *ent = (struct kfs_dirent *)(dir_data + f->offset);
    if (ent->reclen == 0 || f->offset + ent->reclen > inode.size) {
        kfree(dir_data);
        return -EIO;
    }
    
    dirp->ino = ent->inode;
    dirp->type = (ent->type == KFS_TYPE_DIR) ? DT_DIR : DT_REG;
    dirp->reclen = sizeof(struct dirent);
    strncpy(dirp->name, ent->name, sizeof(dirp->name) - 1);
    dirp->name[sizeof(dirp->name) - 1] = '\0';
    
    f->offset += ent->reclen;
    kfree(dir_data);
    return 1;
}

static void kfs_vnode_close(struct vnode *vn, struct file *f) {
    (void)vn;
    (void)f;
}

// Not implementing writes for userspace yet
static int kfs_vnode_write(struct vnode *vn, struct file *f, const void *buf, size_t count, size_t *written) {
    (void)vn; (void)f; (void)buf; (void)count; (void)written;
    return -EINVAL;
}

static const vnode_ops_t kfs_vnode_ops = {
    .read = kfs_vnode_read,
    .write = kfs_vnode_write,
    .lookup = kfs_vnode_lookup,
    .getdents = kfs_vnode_getdents,
    .close = kfs_vnode_close,
};

int kfs_mount(struct blk_dev *dev) {
    kfs_blk = dev;
    int err = kfs_read_superblock();
    if (err < 0) return err;
    
    // Validate superblock fully
    if (sb->total_blocks == 0 || sb->total_blocks > 1000000) return -EINVAL;
    if (sb->block_bitmap_blks == 0) return -EINVAL;
    
    err = kfs_load_bitmaps();
    if (err < 0) return err;
    
    // Create root vnode
    struct vnode *root_vn = kmalloc(sizeof(struct vnode));
    if (!root_vn) return -ENOMEM;
    
    root_vn->ops = &kfs_vnode_ops;
    root_vn->fs_private = (void *)(uintptr_t)sb->root_inode;
    root_vn->type = VNODE_TYPE_DIR;
    
    return vfs_mount_root(root_vn);
}

void kfs_init(void) {
    // Nothing yet, just register mount point conceptually
}

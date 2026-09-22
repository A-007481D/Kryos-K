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

static int kfs_write_inode(uint32_t ino, const struct kfs_inode *in) {
    if (ino >= sb->inode_count) return -EINVAL;
    
    uint32_t inodes_per_block = KFS_BLOCK_SIZE / sizeof(struct kfs_inode); // 8
    uint32_t blk_offset = ino / inodes_per_block;
    uint32_t ino_offset = ino % inodes_per_block;
    
    uint8_t buf[KFS_BLOCK_SIZE];
    int err = kfs_blk->read_sectors(kfs_blk, sb->inode_table_start + blk_offset, 1, buf);
    if (err < 0) return err;
    
    struct kfs_inode *inode_arr = (struct kfs_inode *)buf;
    inode_arr[ino_offset] = *in;
    
    return kfs_blk->write_sectors(kfs_blk, sb->inode_table_start + blk_offset, 1, buf);
}

static int kfs_alloc_inode(uint32_t type, uint32_t *out_ino) {
    for (uint32_t i = 1; i < sb->inode_count; i++) {
        uint32_t byte_idx = i / 8;
        uint8_t bit_idx = i % 8;
        if (!(inode_bitmap[byte_idx] & (1 << bit_idx))) {
            inode_bitmap[byte_idx] |= (1 << bit_idx);
            
            uint32_t sec_offset = byte_idx / KFS_BLOCK_SIZE;
            int err = kfs_blk->write_sectors(kfs_blk, sb->inode_bitmap_start + sec_offset, 1,
                                             inode_bitmap + (sec_offset * KFS_BLOCK_SIZE));
            if (err < 0) return err;
            
            struct kfs_inode new_in;
            memset(&new_in, 0, sizeof(new_in));
            new_in.type = type;
            new_in.link_count = 1;
            err = kfs_write_inode(i, &new_in);
            if (err < 0) return err;
            
            *out_ino = i;
            return 0;
        }
    }
    return -ENOSPC;
}

static int kfs_free_inode(uint32_t ino) {
    if (ino == 0 || ino >= sb->inode_count) return -EINVAL;
    uint32_t byte_idx = ino / 8;
    uint8_t bit_idx = ino % 8;
    inode_bitmap[byte_idx] &= ~(1 << bit_idx);
    
    uint32_t sec_offset = byte_idx / KFS_BLOCK_SIZE;
    return kfs_blk->write_sectors(kfs_blk, sb->inode_bitmap_start + sec_offset, 1,
                                  inode_bitmap + (sec_offset * KFS_BLOCK_SIZE));
}

static int kfs_alloc_block(uint32_t *out_blk) {
    for (uint32_t i = sb->data_blocks_start; i < sb->total_blocks; i++) {
        uint32_t byte_idx = i / 8;
        uint8_t bit_idx = i % 8;
        if (!(block_bitmap[byte_idx] & (1 << bit_idx))) {
            block_bitmap[byte_idx] |= (1 << bit_idx);
            
            uint32_t sec_offset = byte_idx / KFS_BLOCK_SIZE;
            int err = kfs_blk->write_sectors(kfs_blk, sb->block_bitmap_start + sec_offset, 1,
                                             block_bitmap + (sec_offset * KFS_BLOCK_SIZE));
            if (err < 0) return err;
            
            uint8_t zero_buf[KFS_BLOCK_SIZE];
            memset(zero_buf, 0, sizeof(zero_buf));
            err = kfs_blk->write_sectors(kfs_blk, i, 1, zero_buf);
            if (err < 0) return err;
            
            *out_blk = i;
            return 0;
        }
    }
    return -ENOSPC;
}

static int kfs_free_block(uint32_t blk) {
    if (blk < sb->data_blocks_start || blk >= sb->total_blocks) return -EINVAL;
    uint32_t byte_idx = blk / 8;
    uint8_t bit_idx = blk % 8;
    block_bitmap[byte_idx] &= ~(1 << bit_idx);
    
    uint32_t sec_offset = byte_idx / KFS_BLOCK_SIZE;
    return kfs_blk->write_sectors(kfs_blk, sb->block_bitmap_start + sec_offset, 1,
                                  block_bitmap + (sec_offset * KFS_BLOCK_SIZE));
}

static int kfs_map_or_alloc_block(struct kfs_inode *inode, uint32_t logical_blk, uint32_t *phys_blk, bool *inode_dirty) {
    if (logical_blk < 10) {
        if (inode->direct[logical_blk] == 0) {
            uint32_t new_blk = 0;
            int err = kfs_alloc_block(&new_blk);
            if (err < 0) return err;
            inode->direct[logical_blk] = new_blk;
            *inode_dirty = true;
        }
        *phys_blk = inode->direct[logical_blk];
        return 0;
    } else {
        uint32_t ind_offset = logical_blk - 10;
        uint32_t max_ind = KFS_BLOCK_SIZE / sizeof(uint32_t); // 128
        if (ind_offset >= max_ind) return -EFBIG;
        
        if (inode->indirect == 0) {
            uint32_t new_ind = 0;
            int err = kfs_alloc_block(&new_ind);
            if (err < 0) return err;
            inode->indirect = new_ind;
            *inode_dirty = true;
        }
        
        uint32_t ind_buf[KFS_BLOCK_SIZE / sizeof(uint32_t)];
        int err = kfs_blk->read_sectors(kfs_blk, inode->indirect, 1, ind_buf);
        if (err < 0) return err;
        
        if (ind_buf[ind_offset] == 0) {
            uint32_t new_blk = 0;
            err = kfs_alloc_block(&new_blk);
            if (err < 0) return err;
            ind_buf[ind_offset] = new_blk;
            err = kfs_blk->write_sectors(kfs_blk, inode->indirect, 1, ind_buf);
            if (err < 0) return err;
        }
        
        *phys_blk = ind_buf[ind_offset];
        return 0;
    }
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
    
    if (strcmp(name, ".") == 0) {
        struct vnode *new_vn = kmalloc(sizeof(struct vnode));
        if (!new_vn) return -ENOMEM;
        new_vn->ops = vn->ops;
        new_vn->fs_private = vn->fs_private;
        new_vn->type = VNODE_TYPE_DIR;
        *out_vn = new_vn;
        return 0;
    }

    if (ino == sb->root_inode && strcmp(name, "..") == 0) {
        struct vnode *new_vn = kmalloc(sizeof(struct vnode));
        if (!new_vn) return -ENOMEM;
        new_vn->ops = vn->ops;
        new_vn->fs_private = vn->fs_private;
        new_vn->type = VNODE_TYPE_DIR;
        *out_vn = new_vn;
        return 0;
    }
    
    struct file f;
    memset(&f, 0, sizeof(f));
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
    memset(&tmp_f, 0, sizeof(tmp_f));
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
    return sizeof(struct dirent);
}

static void kfs_vnode_close(struct vnode *vn, struct file *f) {
    (void)vn;
    (void)f;
}
static void kfs_free_inode_blocks(struct kfs_inode *inode) {
    for (int i = 0; i < 10; i++) {
        if (inode->direct[i] != 0) {
            kfs_free_block(inode->direct[i]);
            inode->direct[i] = 0;
        }
    }
    
    if (inode->indirect != 0) {
        uint32_t ind_buf[KFS_BLOCK_SIZE / sizeof(uint32_t)];
        int err = kfs_blk->read_sectors(kfs_blk, inode->indirect, 1, ind_buf);
        if (err == 0) {
            for (size_t i = 0; i < (KFS_BLOCK_SIZE / sizeof(uint32_t)); i++) {
                if (ind_buf[i] != 0) {
                    kfs_free_block(ind_buf[i]);
                }
            }
        }
        kfs_free_block(inode->indirect);
        inode->indirect = 0;
    }
}

static int kfs_vnode_truncate(struct vnode *vn) {
    uint32_t ino = (uint32_t)(uintptr_t)vn->fs_private;
    struct kfs_inode inode;
    int err = kfs_read_inode(ino, &inode);
    if (err < 0) return err;
    
    kfs_free_inode_blocks(&inode);
    inode.size = 0;
    return kfs_write_inode(ino, &inode);
}

static int kfs_vnode_write(struct vnode *vn, struct file *f, const void *buf, size_t count, size_t *written) {
    if ((f->flags & (O_WRONLY | O_RDWR)) == 0) {
        return -EINVAL;
    }
    
    if (count == 0) {
        if (written) *written = 0;
        return 0;
    }
    
    uint32_t ino = (uint32_t)(uintptr_t)vn->fs_private;
    struct kfs_inode inode;
    int err = kfs_read_inode(ino, &inode);
    if (err < 0) return err;
    
    if (f->flags & O_APPEND) {
        f->offset = inode.size;
    }
    
    const uint8_t *src = (const uint8_t *)buf;
    size_t total_written = 0;
    size_t to_write = count;
    bool inode_dirty = false;
    
    while (to_write > 0) {
        uint32_t logical_blk = f->offset / KFS_BLOCK_SIZE;
        uint32_t blk_offset = f->offset % KFS_BLOCK_SIZE;
        
        uint32_t phys_blk = 0;
        err = kfs_map_or_alloc_block(&inode, logical_blk, &phys_blk, &inode_dirty);
        if (err < 0) {
            if (total_written > 0) break;
            return err;
        }
        
        uint8_t blk_buf[KFS_BLOCK_SIZE];
        size_t chunk = KFS_BLOCK_SIZE - blk_offset;
        if (chunk > to_write) chunk = to_write;
        
        if (blk_offset > 0 || chunk < KFS_BLOCK_SIZE) {
            err = kfs_blk->read_sectors(kfs_blk, phys_blk, 1, blk_buf);
            if (err < 0) {
                if (total_written > 0) break;
                return err;
            }
        }
        
        memcpy(blk_buf + blk_offset, src, chunk);
        err = kfs_blk->write_sectors(kfs_blk, phys_blk, 1, blk_buf);
        if (err < 0) {
            if (total_written > 0) break;
            return err;
        }
        
        src += chunk;
        f->offset += chunk;
        if (f->offset > inode.size) {
            inode.size = f->offset;
            inode_dirty = true;
        }
        to_write -= chunk;
        total_written += chunk;
    }
    
    if (inode_dirty) {
        err = kfs_write_inode(ino, &inode);
        if (err < 0) return err;
    }
    
    if (written) *written = total_written;
    return 0;
}

static int kfs_remove_dirent(struct vnode *dir_vn, struct kfs_inode *dir_inode, uint32_t target_offset, uint16_t reclen, uint8_t *dir_data) {
    uint32_t dir_ino = (uint32_t)(uintptr_t)dir_vn->fs_private;
    uint32_t remaining = dir_inode->size - (target_offset + reclen);
    if (remaining > 0) {
        memmove(dir_data + target_offset, dir_data + target_offset + reclen, remaining);
    }
    uint32_t new_size = dir_inode->size - reclen;
    
    uint32_t old_blocks = (dir_inode->size + KFS_BLOCK_SIZE - 1) / KFS_BLOCK_SIZE;
    uint32_t new_blocks = (new_size + KFS_BLOCK_SIZE - 1) / KFS_BLOCK_SIZE;
    for (uint32_t b = new_blocks; b < old_blocks; b++) {
        if (b < 10) {
            if (dir_inode->direct[b] != 0) {
                kfs_free_block(dir_inode->direct[b]);
                dir_inode->direct[b] = 0;
            }
        } else {
            uint32_t ind_off = b - 10;
            if (dir_inode->indirect != 0) {
                uint32_t ind_buf[KFS_BLOCK_SIZE / sizeof(uint32_t)];
                if (kfs_blk->read_sectors(kfs_blk, dir_inode->indirect, 1, ind_buf) == 0) {
                    if (ind_buf[ind_off] != 0) {
                        kfs_free_block(ind_buf[ind_off]);
                        ind_buf[ind_off] = 0;
                        kfs_blk->write_sectors(kfs_blk, dir_inode->indirect, 1, ind_buf);
                    }
                }
            }
        }
    }
    if (new_blocks <= 10 && dir_inode->indirect != 0) {
        kfs_free_block(dir_inode->indirect);
        dir_inode->indirect = 0;
    }
    
    for (uint32_t b = 0; b < new_blocks; b++) {
        uint32_t phys_blk = 0;
        if (b < 10) {
            phys_blk = dir_inode->direct[b];
        } else {
            uint32_t ind_buf[KFS_BLOCK_SIZE / sizeof(uint32_t)];
            kfs_blk->read_sectors(kfs_blk, dir_inode->indirect, 1, ind_buf);
            phys_blk = ind_buf[b - 10];
        }
        
        uint8_t blk_buf[KFS_BLOCK_SIZE];
        memset(blk_buf, 0, sizeof(blk_buf));
        size_t off = b * KFS_BLOCK_SIZE;
        size_t chunk = (new_size - off > KFS_BLOCK_SIZE) ? KFS_BLOCK_SIZE : (new_size - off);
        memcpy(blk_buf, dir_data + off, chunk);
        kfs_blk->write_sectors(kfs_blk, phys_blk, 1, blk_buf);
    }
    
    dir_inode->size = new_size;
    return kfs_write_inode(dir_ino, dir_inode);
}

static int kfs_vnode_unlink(struct vnode *vn, const char *name) {
    if (!vn || !name) return -EINVAL;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return -EINVAL;
    
    uint32_t dir_ino = (uint32_t)(uintptr_t)vn->fs_private;
    struct kfs_inode dir_inode;
    int err = kfs_read_inode(dir_ino, &dir_inode);
    if (err < 0) return err;
    if (dir_inode.type != KFS_TYPE_DIR) return -ENOTDIR;
    
    if (dir_inode.size == 0) return -ENOENT;
    
    uint8_t *dir_data = kmalloc(dir_inode.size);
    if (!dir_data) return -ENOMEM;
    
    struct file tmp_f;
    memset(&tmp_f, 0, sizeof(tmp_f));
    tmp_f.vnode = vn;
    tmp_f.offset = 0;
    size_t read_bytes = 0;
    err = kfs_vnode_read(vn, &tmp_f, dir_data, dir_inode.size, &read_bytes);
    if (err < 0) {
        kfree(dir_data);
        return err;
    }
    
    uint32_t offset = 0;
    struct kfs_dirent *target_dent = NULL;
    uint32_t target_offset = 0;
    
    while (offset < dir_inode.size) {
        struct kfs_dirent *ent = (struct kfs_dirent *)(dir_data + offset);
        if (ent->reclen == 0 || ent->reclen % 4 != 0) break;
        if (strcmp(ent->name, name) == 0) {
            target_dent = ent;
            target_offset = offset;
            break;
        }
        offset += ent->reclen;
    }
    
    if (!target_dent) {
        kfree(dir_data);
        return -ENOENT;
    }
    
    uint32_t target_ino = target_dent->inode;
    struct kfs_inode target_inode;
    err = kfs_read_inode(target_ino, &target_inode);
    if (err < 0) {
        kfree(dir_data);
        return err;
    }
    
    if (target_inode.type == KFS_TYPE_DIR) {
        kfree(dir_data);
        return -EISDIR;
    }
    
    kfs_free_inode_blocks(&target_inode);
    target_inode.size = 0;
    target_inode.type = 0;
    target_inode.link_count = 0;
    kfs_write_inode(target_ino, &target_inode);
    kfs_free_inode(target_ino);
    
    err = kfs_remove_dirent(vn, &dir_inode, target_offset, target_dent->reclen, dir_data);
    kfree(dir_data);
    return err;
}

static int kfs_vnode_rmdir(struct vnode *vn, const char *name) {
    if (!vn || !name) return -EINVAL;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return -EINVAL;
    
    uint32_t dir_ino = (uint32_t)(uintptr_t)vn->fs_private;
    struct kfs_inode dir_inode;
    int err = kfs_read_inode(dir_ino, &dir_inode);
    if (err < 0) return err;
    if (dir_inode.type != KFS_TYPE_DIR) return -ENOTDIR;
    
    if (dir_inode.size == 0) return -ENOENT;
    
    uint8_t *dir_data = kmalloc(dir_inode.size);
    if (!dir_data) return -ENOMEM;
    
    struct file tmp_f;
    memset(&tmp_f, 0, sizeof(tmp_f));
    tmp_f.vnode = vn;
    tmp_f.offset = 0;
    size_t read_bytes = 0;
    err = kfs_vnode_read(vn, &tmp_f, dir_data, dir_inode.size, &read_bytes);
    if (err < 0) {
        kfree(dir_data);
        return err;
    }
    
    uint32_t offset = 0;
    struct kfs_dirent *target_dent = NULL;
    uint32_t target_offset = 0;
    
    while (offset < dir_inode.size) {
        struct kfs_dirent *ent = (struct kfs_dirent *)(dir_data + offset);
        if (ent->reclen == 0 || ent->reclen % 4 != 0) break;
        if (strcmp(ent->name, name) == 0) {
            target_dent = ent;
            target_offset = offset;
            break;
        }
        offset += ent->reclen;
    }
    
    if (!target_dent) {
        kfree(dir_data);
        return -ENOENT;
    }
    
    uint32_t target_ino = target_dent->inode;
    struct kfs_inode target_inode;
    err = kfs_read_inode(target_ino, &target_inode);
    if (err < 0) {
        kfree(dir_data);
        return err;
    }
    
    if (target_inode.type != KFS_TYPE_DIR) {
        kfree(dir_data);
        return -ENOTDIR;
    }
    
    // Check if target directory is empty (only "." and ".." allowed)
    if (target_inode.size > 0) {
        uint8_t *sub_data = kmalloc(target_inode.size);
        if (!sub_data) {
            kfree(dir_data);
            return -ENOMEM;
        }
        
        struct vnode sub_vn;
        memset(&sub_vn, 0, sizeof(sub_vn));
        sub_vn.ops = vn->ops;
        sub_vn.fs_private = (void *)(uintptr_t)target_ino;
        sub_vn.type = VNODE_TYPE_DIR;
        
        struct file sub_f;
        memset(&sub_f, 0, sizeof(sub_f));
        sub_f.vnode = &sub_vn;
        
        size_t sub_read = 0;
        err = kfs_vnode_read(&sub_vn, &sub_f, sub_data, target_inode.size, &sub_read);
        if (err < 0) {
            kfree(sub_data);
            kfree(dir_data);
            return err;
        }
        
        uint32_t sub_off = 0;
        while (sub_off < target_inode.size) {
            struct kfs_dirent *e = (struct kfs_dirent *)(sub_data + sub_off);
            if (e->reclen == 0 || e->reclen % 4 != 0) break;
            if (strcmp(e->name, ".") != 0 && strcmp(e->name, "..") != 0) {
                kfree(sub_data);
                kfree(dir_data);
                return -ENOTEMPTY;
            }
            sub_off += e->reclen;
        }
        kfree(sub_data);
    }
    
    kfs_free_inode_blocks(&target_inode);
    target_inode.size = 0;
    target_inode.type = 0;
    target_inode.link_count = 0;
    kfs_write_inode(target_ino, &target_inode);
    kfs_free_inode(target_ino);
    
    err = kfs_remove_dirent(vn, &dir_inode, target_offset, target_dent->reclen, dir_data);
    kfree(dir_data);
    return err;
}

static int kfs_vnode_create(struct vnode *vn, const char *name, uint32_t type, struct vnode **out_vn) {
    uint32_t dir_ino = (uint32_t)(uintptr_t)vn->fs_private;
    struct kfs_inode dir_inode;
    int err = kfs_read_inode(dir_ino, &dir_inode);
    if (err < 0) return err;
    if (dir_inode.type != KFS_TYPE_DIR) return -ENOTDIR;
    
    size_t name_len = strlen(name);
    if (name_len == 0 || name_len > 255) return -EINVAL;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return -EEXIST;
    
    // Check if entry already exists
    struct vnode *existing = NULL;
    err = kfs_vnode_lookup(vn, name, &existing);
    if (err == 0) {
        if (type == VNODE_TYPE_DIR) return -EEXIST;
        *out_vn = existing;
        return 0;
    }
    
    // Allocate new inode
    uint32_t new_ino = 0;
    uint32_t kfs_type = (type == VNODE_TYPE_DIR) ? KFS_TYPE_DIR : KFS_TYPE_FILE;
    err = kfs_alloc_inode(kfs_type, &new_ino);
    if (err < 0) return err;
    
    // If creating a directory, allocate 1 block and populate "." and ".."
    if (kfs_type == KFS_TYPE_DIR) {
        uint32_t dir_blk = 0;
        err = kfs_alloc_block(&dir_blk);
        if (err < 0) {
            kfs_free_inode(new_ino);
            return err;
        }
        
        uint8_t blk_buf[KFS_BLOCK_SIZE];
        memset(blk_buf, 0, sizeof(blk_buf));
        
        // "." entry (12 bytes)
        struct kfs_dirent *dot = (struct kfs_dirent *)blk_buf;
        dot->inode = new_ino;
        dot->reclen = 12;
        dot->type = KFS_TYPE_DIR;
        memcpy(dot->name, ".\0", 2);
        
        // ".." entry (12 bytes)
        struct kfs_dirent *dotdot = (struct kfs_dirent *)(blk_buf + 12);
        dotdot->inode = dir_ino;
        dotdot->reclen = 12;
        dotdot->type = KFS_TYPE_DIR;
        memcpy(dotdot->name, "..\0", 3);
        
        err = kfs_blk->write_sectors(kfs_blk, dir_blk, 1, blk_buf);
        if (err < 0) {
            kfs_free_block(dir_blk);
            kfs_free_inode(new_ino);
            return err;
        }
        
        struct kfs_inode new_dir_in;
        memset(&new_dir_in, 0, sizeof(new_dir_in));
        new_dir_in.type = KFS_TYPE_DIR;
        new_dir_in.size = 24;
        new_dir_in.direct[0] = dir_blk;
        new_dir_in.link_count = 2;
        
        err = kfs_write_inode(new_ino, &new_dir_in);
        if (err < 0) {
            kfs_free_block(dir_blk);
            kfs_free_inode(new_ino);
            return err;
        }
    }
    
    size_t base_len = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t) + name_len + 1;
    size_t reclen = (base_len + 3) & ~3;
    
    uint8_t *dirent_buf = kmalloc(reclen);
    if (!dirent_buf) {
        if (kfs_type == KFS_TYPE_DIR) {
            struct kfs_inode in_tmp;
            if (kfs_read_inode(new_ino, &in_tmp) == 0 && in_tmp.direct[0] != 0) {
                kfs_free_block(in_tmp.direct[0]);
            }
        }
        kfs_free_inode(new_ino);
        return -ENOMEM;
    }
    memset(dirent_buf, 0, reclen);
    
    struct kfs_dirent *dent = (struct kfs_dirent *)dirent_buf;
    dent->inode = new_ino;
    dent->reclen = (uint16_t)reclen;
    dent->type = (uint8_t)kfs_type;
    memcpy(dent->name, name, name_len + 1);
    
    // Append to directory inode
    kfs_read_inode(dir_ino, &dir_inode);
    struct file dir_f;
    memset(&dir_f, 0, sizeof(dir_f));
    dir_f.vnode = vn;
    dir_f.offset = dir_inode.size;
    dir_f.flags = O_WRONLY;
    dir_f.private_data = NULL;
    
    size_t written = 0;
    err = kfs_vnode_write(vn, &dir_f, dirent_buf, reclen, &written);
    kfree(dirent_buf);
    if (err < 0) {
        if (kfs_type == KFS_TYPE_DIR) {
            struct kfs_inode in_tmp;
            if (kfs_read_inode(new_ino, &in_tmp) == 0 && in_tmp.direct[0] != 0) {
                kfs_free_block(in_tmp.direct[0]);
            }
        }
        kfs_free_inode(new_ino);
        return err;
    }
    
    struct vnode *new_vn = kmalloc(sizeof(struct vnode));
    if (!new_vn) return -ENOMEM;
    new_vn->ops = vn->ops;
    new_vn->fs_private = (void *)(uintptr_t)new_ino;
    new_vn->type = (kfs_type == KFS_TYPE_DIR) ? VNODE_TYPE_DIR : VNODE_TYPE_FILE;
    
    *out_vn = new_vn;
    return 0;
}

static const vnode_ops_t kfs_vnode_ops = {
    .read = kfs_vnode_read,
    .write = kfs_vnode_write,
    .lookup = kfs_vnode_lookup,
    .create = kfs_vnode_create,
    .truncate = kfs_vnode_truncate,
    .unlink = kfs_vnode_unlink,
    .rmdir = kfs_vnode_rmdir,
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

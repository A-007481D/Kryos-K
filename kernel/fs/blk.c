#include "../../include/blk.h"
#include "../../include/vfs.h"
#include "../../include/heap.h"
#include <string.h>

#define MAX_BLK_DEVS 8
static struct blk_dev *registered_devs[MAX_BLK_DEVS];
static int num_devs = 0;

static int blk_vnode_read(struct vnode *vn, struct file *f, void *buf, size_t count, size_t *bytes_read) {
    if (!vn || !f || !buf || !bytes_read) return -1;
    struct blk_dev *dev = (struct blk_dev*)vn->fs_private;
    if (!dev) return -1;

    // Calculate LBA and offset within sector
    uint64_t lba = f->offset / BLK_SECTOR_SIZE;
    uint32_t offset_in_sector = f->offset % BLK_SECTOR_SIZE;

    // For simplicity, handle sector-aligned or partial reads by reading to a bounce buffer
    // and copying to user buf. This handles boundaries correctly.
    char bounce[BLK_SECTOR_SIZE];
    size_t total_read = 0;

    while (count > 0 && lba < dev->sector_count) {
        if (dev->read_sectors(dev, lba, 1, bounce) != 0) {
            break; // Read error
        }

        size_t to_copy = BLK_SECTOR_SIZE - offset_in_sector;
        if (to_copy > count) to_copy = count;

        memcpy((char*)buf + total_read, bounce + offset_in_sector, to_copy);
        
        total_read += to_copy;
        count -= to_copy;
        f->offset += to_copy;
        
        lba++;
        offset_in_sector = 0;
    }

    *bytes_read = total_read;
    return 0;
}

static int blk_vnode_write(struct vnode *vn, struct file *f, const void *buf, size_t count, size_t *bytes_written) {
    if (!vn || !f || !buf || !bytes_written) return -1;
    struct blk_dev *dev = (struct blk_dev*)vn->fs_private;
    if (!dev) return -1;

    uint64_t lba = f->offset / BLK_SECTOR_SIZE;
    uint32_t offset_in_sector = f->offset % BLK_SECTOR_SIZE;

    char bounce[BLK_SECTOR_SIZE];
    size_t total_written = 0;

    while (count > 0 && lba < dev->sector_count) {
        // If we are doing a partial sector write, we must read-modify-write
        if (offset_in_sector > 0 || count < BLK_SECTOR_SIZE) {
            if (dev->read_sectors(dev, lba, 1, bounce) != 0) {
                break;
            }
        }

        size_t to_copy = BLK_SECTOR_SIZE - offset_in_sector;
        if (to_copy > count) to_copy = count;

        memcpy(bounce + offset_in_sector, (const char*)buf + total_written, to_copy);

        if (dev->write_sectors(dev, lba, 1, bounce) != 0) {
            break;
        }

        total_written += to_copy;
        count -= to_copy;
        f->offset += to_copy;
        
        lba++;
        offset_in_sector = 0;
    }

    *bytes_written = total_written;
    return 0;
}

static void blk_vnode_close(struct vnode *vn, struct file *f) {
    (void)vn;
    (void)f;
    // No special close handling for block devices currently
}

static vnode_ops_t blk_ops = {
    .read = blk_vnode_read,
    .write = blk_vnode_write,
    .lookup = NULL,
    .getdents = NULL,
    .close = blk_vnode_close
};

void blk_init(void) {
    num_devs = 0;
    for (int i = 0; i < MAX_BLK_DEVS; i++) {
        registered_devs[i] = NULL;
    }
}

void blk_register(struct blk_dev *dev) {
    if (num_devs >= MAX_BLK_DEVS) return;
    registered_devs[num_devs++] = dev;
}

struct blk_dev* blk_get_dev(const char *name) {
    for (int i = 0; i < num_devs; i++) {
        if (strcmp(registered_devs[i]->name, name) == 0) {
            return registered_devs[i];
        }
    }
    return NULL;
}

struct vnode* blk_get_vnode(const char *name) {
    struct blk_dev *dev = blk_get_dev(name);
    if (!dev) return NULL;
    
    struct vnode *vn = (struct vnode*)kmalloc(sizeof(struct vnode));
    if (!vn) return NULL;
    
    memset(vn, 0, sizeof(struct vnode));
    vn->ops = &blk_ops;
    vn->fs_private = dev;
    vn->type = VNODE_TYPE_FILE;
    
    return vn;
}

#pragma once

#include <stdint.h>
#include <stddef.h>

#define BLK_SECTOR_SIZE 512

struct blk_dev {
    char name[32];
    uint64_t sector_count;
    
    // Read count sectors starting from lba into buf
    int (*read_sectors)(struct blk_dev *dev, uint64_t lba, uint32_t count, void *buf);
    
    // Write count sectors starting from lba from buf
    int (*write_sectors)(struct blk_dev *dev, uint64_t lba, uint32_t count, const void *buf);
    
    void *priv;
};

// Register a new block device
int blk_register(struct blk_dev *dev);

// Get a block device by name (e.g., "hda")
struct blk_dev* blk_get(const char *name);

// Initialize the block device subsystem
void blk_init(void);

// Get the vnode corresponding to a registered block device
struct vnode* blk_get_vnode(const char *name);

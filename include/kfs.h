#ifndef KRYOS_KFS_H
#define KRYOS_KFS_H

#include <stdint.h>
#include "vfs.h"
#include "blk.h"

// KFS Constants
#define KFS_MAGIC       0x4B465331 // 'KFS1'
#define KFS_BLOCK_SIZE  512

// Hardcoded deterministic layout for 16MB disk
#define KFS_TOTAL_BLOCKS        32768
#define KFS_INODE_COUNT         128

// Block assignments
#define KFS_SUPERBLOCK_BLK      0
#define KFS_BLOCK_BITMAP_START  1
#define KFS_BLOCK_BITMAP_BLKS   8   // (32768 bits / 8 bytes_per_byte = 4096 bytes = 8 blocks)
#define KFS_INODE_BITMAP_START  9
#define KFS_INODE_BITMAP_BLKS   1   // (128 bits = 16 bytes <= 1 block)
#define KFS_INODE_TABLE_START   10
#define KFS_INODE_TABLE_BLKS    16  // (128 inodes * 64 bytes = 8192 bytes = 16 blocks)
#define KFS_DATA_BLOCKS_START   26

#define KFS_ROOT_INODE          0

// Inode types
#define KFS_TYPE_FREE           0
#define KFS_TYPE_FILE           1
#define KFS_TYPE_DIR            2

// Superblock definition (fits in 512 bytes)
struct kfs_superblock {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t inode_count;
    uint32_t block_bitmap_start;
    uint32_t block_bitmap_blks;
    uint32_t inode_bitmap_start;
    uint32_t inode_bitmap_blks;
    uint32_t inode_table_start;
    uint32_t inode_table_blks;
    uint32_t data_blocks_start;
    uint32_t root_inode;
    uint8_t  padding[468]; // Pad to 512 bytes
} __attribute__((packed));
_Static_assert(sizeof(struct kfs_superblock) == KFS_BLOCK_SIZE, "KFS Superblock must be exactly 512 bytes");

// Inode definition (exactly 64 bytes)
struct kfs_inode {
    uint32_t size;            // File size in bytes
    uint32_t type;            // KFS_TYPE_FILE or KFS_TYPE_DIR
    uint32_t direct[10];      // 10 direct block pointers (indices)
    uint32_t indirect;        // 1 singly-indirect block pointer
    uint32_t link_count;      // Not strictly used in Phase 21, but good practice
    uint32_t pad[2];          // Padding to reach 64 bytes
} __attribute__((packed));
_Static_assert(sizeof(struct kfs_inode) == 64, "KFS Inode must be exactly 64 bytes");

// Directory entry
// Variable length. `reclen` must be aligned to 4 bytes.
// name is NUL-terminated inside the record.
struct kfs_dirent {
    uint32_t inode;
    uint16_t reclen;
    uint8_t  type;
    char     name[];
} __attribute__((packed));

// Minimum size of a directory entry (inode + reclen + type + 1 char name) = 8 bytes.
// Actually inode(4)+reclen(2)+type(1)+name(1 minimum) = 8.
#define KFS_DIRENT_MIN_SIZE 8

// Mount the KFS filesystem from a given block device
int kfs_mount(struct blk_dev *dev);

// Initialize the KFS subsystem
void kfs_init(void);

#endif // KRYOS_KFS_H

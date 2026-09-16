#ifndef KRYOS_TARFS_H
#define KRYOS_TARFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "vfs.h"

// USTAR header format
struct tar_header {
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag[1];
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} __attribute__((packed));

#define TAR_TYPE_NORMAL '0'
#define TAR_TYPE_HARDLINK '1'
#define TAR_TYPE_SYMLINK '2'
#define TAR_TYPE_CHAR '3'
#define TAR_TYPE_BLOCK '4'
#define TAR_TYPE_DIR '5'
#define TAR_TYPE_FIFO '6'

struct tarfs_file {
    char name[256];
    uint64_t data_offset;
    uint64_t size;
    uint32_t type;
};

// Mounts a TarFS from a physical memory region.
// The region must be mapped in the kernel's higher-half direct map.
// start_phys and end_phys refer to the multiboot module boundaries.
int tarfs_mount(uint64_t start_phys, uint64_t end_phys);

#endif // KRYOS_TARFS_H

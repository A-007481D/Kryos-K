#include "tarfs.h"
#include "../memory/virt.h"
#include "../../include/heap.h"
#include <string.h>

#define MAX_TARFS_FILES 128

static struct tarfs_file tarfs_files[MAX_TARFS_FILES];
static int tarfs_file_count = 0;

static uint64_t octal_to_uint64(const char *str, size_t max_len) {
    uint64_t val = 0;
    for (size_t i = 0; i < max_len; i++) {
        if (str[i] >= '0' && str[i] <= '7') {
            val = (val * 8) + (str[i] - '0');
        } else if (str[i] == ' ' || str[i] == '\0') {
            break;
        }
    }
    return val;
}

static bool verify_checksum(struct tar_header *hdr) {
    uint32_t expected = octal_to_uint64(hdr->chksum, sizeof(hdr->chksum));
    uint32_t sum = 0;
    
    const uint8_t *ptr = (const uint8_t*)hdr;
    for (size_t i = 0; i < 512; i++) {
        if (i >= offsetof(struct tar_header, chksum) && i < offsetof(struct tar_header, chksum) + sizeof(hdr->chksum)) {
            sum += ' ';
        } else {
            sum += ptr[i];
        }
    }
    
    return (sum == expected || sum == expected + 256); // some tar implementations have signed/unsigned bugs
}

static int tarfs_read(struct vnode *vn, struct file *f, void *buf, size_t count, size_t *bytes_read) {
    if (!vn || !f || !buf || !bytes_read) return -EINVAL;
    
    struct tarfs_file *tfile = (struct tarfs_file *)vn->fs_private;
    if (!tfile) return -EINVAL;
    
    if (f->offset >= tfile->size) {
        *bytes_read = 0;
        return 0; // EOF
    }
    
    size_t remaining = tfile->size - f->offset;
    size_t to_read = (count < remaining) ? count : remaining;
    
    const void *src = (const void *)(tfile->data_offset);
    memcpy(buf, (const uint8_t*)src + f->offset, to_read);
    
    f->offset += to_read;
    *bytes_read = to_read;
    return 0;
}

static void tarfs_close(struct vnode *vn, struct file *f) {
    (void)vn;
    (void)f;
}

static int tarfs_lookup(struct vnode *vn, const char *name, struct vnode **out_vn) {
    (void)vn;
    if (!name || !out_vn) return -EINVAL;
    
    for (int i = 0; i < tarfs_file_count; i++) {
        if (strcmp(tarfs_files[i].name, name) == 0) {
            struct vnode *child = kmalloc(sizeof(struct vnode));
            if (!child) return -ENOMEM;
            
            // We need a static ops table
            static vnode_ops_t child_ops;
            child_ops.read = tarfs_read;
            child_ops.write = NULL; // Read-only
            child_ops.lookup = NULL; // Files don't have children
            child_ops.close = tarfs_close;
            
            child->ops = &child_ops;
            child->fs_private = &tarfs_files[i];
            child->type = VNODE_TYPE_FILE;
            
            *out_vn = child;
            return 0;
        }
    }
    
    return -ENOENT;
}

static vnode_ops_t tarfs_root_ops = {
    .read = NULL,
    .write = NULL,
    .lookup = tarfs_lookup,
    .close = NULL
};

static struct vnode tarfs_root_vnode = {
    .ops = &tarfs_root_ops,
    .fs_private = NULL,
    .type = VNODE_TYPE_DIR
};

int tarfs_mount(uint64_t start_phys, uint64_t end_phys) {
    if (start_phys >= end_phys) return -EINVAL;
    
    uint64_t current_phys = start_phys;
    tarfs_file_count = 0;
    
    while (current_phys + 512 <= end_phys) {
        struct tar_header *hdr = (struct tar_header *)phys_to_virt(current_phys);
        
        // Two zero blocks indicate end of archive
        if (hdr->filename[0] == '\0') {
            break;
        }
        
        // Validate magic (ustar)
        if (memcmp(hdr->magic, "ustar", 5) != 0) {
            // Not a valid ustar archive or corrupted
            return -EINVAL;
        }
        
        if (!verify_checksum(hdr)) {
            return -EINVAL; // Checksum failed
        }
        
        uint64_t file_size = octal_to_uint64(hdr->size, sizeof(hdr->size));
        uint64_t data_phys = current_phys + 512;
        uint64_t padded_size = (file_size + 511) & ~511ULL;
        
        // Validate data bounds
        if (data_phys + padded_size > end_phys || data_phys + padded_size < data_phys) {
            return -EINVAL; // Out of bounds or overflow
        }
        
        char typeflag = hdr->typeflag[0] ? hdr->typeflag[0] : TAR_TYPE_NORMAL;
        
        // Only accept regular files
        if (typeflag == TAR_TYPE_NORMAL || typeflag == '\0') {
            if (tarfs_file_count >= MAX_TARFS_FILES) {
                break; // Limit reached
            }
            
            struct tarfs_file *tfile = &tarfs_files[tarfs_file_count++];
            strncpy(tfile->name, hdr->filename, sizeof(tfile->name) - 1);
            tfile->name[sizeof(tfile->name) - 1] = '\0';
            
            tfile->data_offset = (uint64_t)phys_to_virt(data_phys);
            tfile->size = file_size;
            tfile->type = VNODE_TYPE_FILE;
        } else if (typeflag == TAR_TYPE_DIR) {
            // Ignore directories
        } else {
            // Reject unsupported types
            return -EINVAL; 
        }
        
        current_phys = data_phys + padded_size;
    }
    
    return vfs_mount_root(&tarfs_root_vnode);
}

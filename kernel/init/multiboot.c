#include "multiboot.h"
#include "multiboot2.h"
#include "../memory/virt.h"
#include "../../include/pmm.h"
#include <assert.h>
#include <stdio.h>

static inline uint32_t align_up_8(uint32_t val) {
    return (val + 7) & ~7;
}

void multiboot_parse(uint32_t magic, uint32_t info_addr_phys) {
    kprintf("multiboot_parse: magic=0x%x, info_addr=0x%x\n", magic, info_addr_phys);
    
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        panic(__FILE__, __LINE__, "Invalid Multiboot2 Magic: 0x%x (expected 0x%x)\n", magic, MULTIBOOT2_BOOTLOADER_MAGIC);
    }
    
    // The multiboot info struct is in physical memory, likely below 1 GiB.
    // We access it via our higher-half mapping.
    struct multiboot2_info_header *header = phys_to_virt(info_addr_phys);
    
    uint32_t total_size = header->total_size;
    uint8_t *ptr = (uint8_t *)header + sizeof(struct multiboot2_info_header);
    
    // Pass 1: Find the maximum physical memory address
    uint64_t max_phys_addr = 0;

    
    uint8_t *iter = ptr;
    while (iter < (uint8_t *)header + total_size) {
        struct multiboot_tag *tag = (struct multiboot_tag *)iter;
        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }
        
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap *)tag;
            uint32_t entry_count = (mmap_tag->size - sizeof(*mmap_tag)) / mmap_tag->entry_size;
            
            for (uint32_t i = 0; i < entry_count; i++) {
                struct multiboot_mmap_entry *entry = &mmap_tag->entries[i];
                uint64_t end_addr = entry->addr + entry->len;
                // Cap to 1 GiB since our bootstrap page tables only map 1 GiB
                if (end_addr > 0x40000000) {
                    end_addr = 0x40000000;
                }
                if (end_addr > max_phys_addr) {
                    max_phys_addr = end_addr;
                }
            }
        }
        
        if (tag->size == 0) {
            panic(__FILE__, __LINE__, "Multiboot2 tag size is 0! Infinite loop prevented.");
        }
        
        iter += align_up_8(tag->size);
    }
    
    kprintf("multiboot_parse: max_phys_addr=0x%lx\n", max_phys_addr);
    
    // Calculate the highest physical address used by boot structures.
    // The multiboot info structure is at info_addr_phys and is total_size bytes long.
    uint64_t boot_structures_end = info_addr_phys + total_size;
    extern char _kernel_end[];
    uint64_t kernel_end_phys = virt_to_phys(_kernel_end);
    
    uint64_t highest_used = kernel_end_phys;
    if (boot_structures_end > highest_used) {
        highest_used = boot_structures_end;
    }
    
    // Initialize the PMM. This will place the bitmap after highest_used.
    pmm_init(highest_used, max_phys_addr);
    
    // Pass 2: Mark AVAILABLE regions as free in the PMM
    iter = ptr;
    while (iter < (uint8_t *)header + total_size) {
        struct multiboot_tag *tag = (struct multiboot_tag *)iter;
        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }
        
#ifdef KRYOS_DEBUG
        kprintf("Pass 2: tag type=%d, size=%d\n", tag->type, tag->size);
#endif
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap *)tag;
            
            if (mmap_tag->entry_size == 0) {
                panic(__FILE__, __LINE__, "mmap entry size is 0");
            }
            
            uint32_t entry_count = (mmap_tag->size - sizeof(*mmap_tag)) / mmap_tag->entry_size;
            
            for (uint32_t i = 0; i < entry_count; i++) {
                struct multiboot_mmap_entry *entry = &mmap_tag->entries[i];
                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                    pmm_mark_region(entry->addr, entry->len, true);
                }
            }
        }
        
        iter += align_up_8(tag->size);
    }
    
    kprintf("multiboot_parse: finished pass 2.\n");
    
    // After marking available regions as FREE, we must forcefully re-reserve
    // the critical bootstrap regions.
    
    // 1. Reserve the first 1 MiB, the Kernel Image, and the PMM Bitmap
    pmm_reserve_boot_regions();
    kprintf("multiboot_parse: boot regions reserved.\n");
    
    // 2. Reserve the Multiboot structure itself
    pmm_mark_region(info_addr_phys, total_size, false);
    kprintf("multiboot_parse: completed.\n");
}

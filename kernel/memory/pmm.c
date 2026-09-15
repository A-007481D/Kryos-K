#include "../../include/pmm.h"
#include "virt.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

extern char _kernel_phys_start[];
extern char _kernel_end[];

static uint8_t *bitmap = NULL;
static uint64_t total_frames = 0;
static uint64_t used_frames = 0;
static uint64_t max_physical_address = 0;
static uint64_t bitmap_phys_addr = 0;
static uint64_t bitmap_size = 0;

static inline uint64_t align_up(uint64_t val, uint64_t alignment) {
    return (val + alignment - 1) & ~(alignment - 1);
}

static inline uint64_t align_down(uint64_t val, uint64_t alignment) {
    return val & ~(alignment - 1);
}

void pmm_init(uint64_t bitmap_start_phys, uint64_t max_phys_addr) {
    max_physical_address = max_phys_addr;
    total_frames = max_physical_address / PMM_PAGE_SIZE;
    
    // Bitmap size in bytes (1 bit per frame)
    bitmap_size = total_frames / 8;
    if (total_frames % 8 != 0) {
        bitmap_size++;
    }
    
    // Place the bitmap after the highest used boot structure
    bitmap_phys_addr = align_up(bitmap_start_phys, PMM_PAGE_SIZE);
    
    // The virtual address of the bitmap
    bitmap = (uint8_t *)phys_to_virt(bitmap_phys_addr);
    
    kprintf("pmm_init: max_phys=0x%lx, frames=%lu, bitmap_size=%lu bytes at phys 0x%lx (virt 0x%lx)\n", 
            max_physical_address, total_frames, bitmap_size, bitmap_phys_addr, (uint64_t)bitmap);
            
    // 1. Mark ALL memory as RESERVED by default
    // In our bitmap, 1 = Used/Reserved, 0 = Free
    for (uint64_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0xFF;
    }
    
    kprintf("pmm_init: bitmap initialized.\n");
    
    // At this point, everything is reserved.
    used_frames = total_frames;
}

void pmm_reserve_boot_regions(void) {
    // Reserve from 0x0 up to the end of the bitmap.
    // This covers: 
    // - 1st MiB (BIOS, VGA, real-mode stuff)
    // - The kernel image (starts at 1MiB)
    // - The PMM bitmap storage itself.
    pmm_mark_region(0, bitmap_phys_addr + bitmap_size, false);
}

void pmm_mark_region(uint64_t phys_addr, uint64_t length, bool free) {
    uint64_t start_addr = align_down(phys_addr, PMM_PAGE_SIZE);
    uint64_t end_addr = align_up(phys_addr + length, PMM_PAGE_SIZE);
    
    if (end_addr > max_physical_address) {
        end_addr = max_physical_address;
    }
    
#ifdef KRYOS_DEBUG
    kprintf("pmm_mark_region: start=0x%lx, end=0x%lx, free=%d\n", start_addr, end_addr, free);
#endif
    
    for (uint64_t addr = start_addr; addr < end_addr; addr += PMM_PAGE_SIZE) {
        uint64_t frame = addr >> 12; // addr / 4096
        uint64_t byte = frame >> 3;  // frame / 8
        uint8_t bit = frame & 7;     // frame % 8
        
        bool is_currently_free = !(bitmap[byte] & (1 << bit));
        
        if (free) {
            if (!is_currently_free) {
                bitmap[byte] &= ~(1 << bit);
                used_frames--;
            }
        } else {
            if (is_currently_free) {
                bitmap[byte] |= (1 << bit);
                used_frames++;
            }
        }
    }
#ifdef KRYOS_DEBUG
    kprintf("pmm_mark_region: done.\n");
#endif
}

uint64_t pmm_alloc_page(void) {
    // Basic linear search. For a hobby OS, this is fine initially.
    for (uint64_t byte = 0; byte < bitmap_size; byte++) {
        if (bitmap[byte] != 0xFF) {
            for (uint8_t bit = 0; bit < 8; bit++) {
                if (!(bitmap[byte] & (1 << bit))) {
                    uint64_t frame = byte * 8 + bit;
                    if (frame >= total_frames) {
                        return 0; // OOM (out of bounds)
                    }
                    
                    bitmap[byte] |= (1 << bit);
                    used_frames++;
                    return frame * PMM_PAGE_SIZE;
                }
            }
        }
    }
    return 0; // OOM
}

void pmm_free_page(uint64_t phys_addr) {
    if (phys_addr % PMM_PAGE_SIZE != 0) {
        panic(__FILE__, __LINE__, "PMM: unaligned free 0x%lx", phys_addr);
    }
    
    if (phys_addr >= max_physical_address) {
        panic(__FILE__, __LINE__, "PMM: out-of-range free 0x%lx", phys_addr);
    }
    
    uint64_t frame = phys_addr / PMM_PAGE_SIZE;
    uint64_t byte = frame / 8;
    uint8_t bit = frame % 8;
    
    if (!(bitmap[byte] & (1 << bit))) {
        panic(__FILE__, __LINE__, "PMM: double free 0x%lx", phys_addr);
    }
    
    // Prevent freeing reserved architectural memory (first 1MiB, Kernel, MB2, Bitmap)
    // For now, we will rely on the caller not to free these, or we could add explicit checks.
    // The simplest check is that you shouldn't be able to free physical memory below the 
    // end of the bitmap storage, since that encompasses everything reserved at boot.
    uint64_t first_usable_phys = bitmap_phys_addr + bitmap_size;
    if (phys_addr < first_usable_phys) {
        panic(__FILE__, __LINE__, "PMM: attempted to free reserved frame 0x%lx", phys_addr);
    }
    
    bitmap[byte] &= ~(1 << bit);
    used_frames--;
}

uint64_t pmm_total_frames(void) {
    return total_frames;
}

uint64_t pmm_free_frames(void) {
    return total_frames - used_frames;
}

uint64_t pmm_used_frames(void) {
    return used_frames;
}

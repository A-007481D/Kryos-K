#pragma once

#include <stdint.h>
#include <stdbool.h>

#define PMM_PAGE_SIZE 4096

// Initializes the PMM, placing the bitmap after the specified
// physical address and marking all memory up to max_phys_addr as RESERVED.
void pmm_init(uint64_t bitmap_start_phys, uint64_t max_phys_addr);

// Reserves the first 1MiB, the kernel image, and the PMM bitmap.
void pmm_reserve_boot_regions(void);

// Marks a physical memory region as FREE (true) or RESERVED (false)
void pmm_mark_region(uint64_t phys_addr, uint64_t length, bool free);

// Allocates a single 4 KiB physical frame. Returns 0 on OOM.
uint64_t pmm_alloc_page(void);

// Frees a single 4 KiB physical frame.
void pmm_free_page(uint64_t phys_addr);

// Accounting
uint64_t pmm_total_frames(void);
uint64_t pmm_free_frames(void);
uint64_t pmm_used_frames(void);

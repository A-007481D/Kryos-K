#pragma once

#include <stdint.h>

// The higher-half offset mapped in boot.asm (2 GiB below 0)
#define HIGHER_HALF_BASE 0xFFFFFFFF80000000

/**
 * Converts a physical address to its virtual address in the higher-half map.
 * WARNING: This is only valid for physical addresses mapped by the boot page tables
 * (currently the first 1 GiB).
 */
static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + HIGHER_HALF_BASE);
}

/**
 * Converts a virtual address in the higher-half map back to a physical address.
 */
static inline uint64_t virt_to_phys(const void *virt) {
    return (uint64_t)virt - HIGHER_HALF_BASE;
}

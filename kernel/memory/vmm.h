#pragma once

#include <stdint.h>
#include <stdbool.h>

// VMM Abstract Flags
// These isolate the caller from x86_64 architecture-specific PTE bits.
#define VMM_FLAG_NONE       0
#define VMM_FLAG_WRITABLE   (1 << 0)
#define VMM_FLAG_USER       (1 << 1)
#define VMM_FLAG_NO_EXECUTE (1 << 2)

// Initialize the VMM. 
// Takes ownership of the boot PML4 table without modifying the fundamental layout.
void vmm_init(uint64_t pml4_phys);

// Map a 4 KiB virtual page to a physical frame.
// Validates canonical addresses, alignment, and rejects mapping over existing pages
// or inside existing huge pages.
bool vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint32_t flags);

// Unmap a 4 KiB virtual page.
// Clears the PTE and invalidates the TLB. Rejects attempts to unmap huge pages.
// Does NOT free the physical frame associated with the mapping.
bool vmm_unmap_page(uint64_t virt_addr);

// Safely query the physical backing of a virtual address.
// Returns true and populates out_phys if mapped. Handles both 4 KiB and 2 MiB pages.
bool vmm_get_phys(uint64_t virt_addr, uint64_t *out_phys);

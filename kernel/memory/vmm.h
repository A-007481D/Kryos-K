#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../include/process.h"

// VMM Abstract Flags
// These isolate the caller from x86_64 architecture-specific PTE bits.
#define VMM_FLAG_NONE       0
#define VMM_FLAG_WRITABLE   (1 << 0)
#define VMM_FLAG_USER       (1 << 1)
#define VMM_FLAG_NO_EXECUTE (1 << 2)

// Initialize the VMM. 
// Takes ownership of the boot PML4 table without modifying the fundamental layout.
void vmm_init(uint64_t pml4_phys);

// Create a new address space by allocating a PML4 and copying shared kernel mappings.
bool vmm_create_address_space(address_space_t *as);

// Destroy an address space, freeing all user mappings and user page-table frames.
void vmm_destroy_address_space(address_space_t *as);

// Map a 4 KiB virtual page to a physical frame within the given address space.
bool vmm_map_page(address_space_t *as, uint64_t virt_addr, uint64_t phys_addr, uint32_t flags);

// Unmap a 4 KiB virtual page within the given address space.
bool vmm_unmap_page(address_space_t *as, uint64_t virt_addr);

// Safely query the physical backing of a virtual address in the given address space.
// Returns true and populates out_phys if mapped. Handles both 4 KiB and 2 MiB pages.
bool vmm_get_phys(address_space_t *as, uint64_t virt_addr, uint64_t* out_phys);

// Get the currently active PML4 physical address.
uint64_t vmm_get_current_pml4(void);

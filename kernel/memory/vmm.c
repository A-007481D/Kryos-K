#include "vmm.h"
#include "../../include/pmm.h"
#include "virt.h"
#include <stddef.h>

// Hardware PTE flags
#define PTE_PRESENT       (1ULL << 0)
#define PTE_WRITABLE      (1ULL << 1)
#define PTE_USER          (1ULL << 2)
#define PDE_PS            (1ULL << 7)
#define PTE_NO_EXECUTE    (1ULL << 63)
#define PTE_FRAME_MASK    0x000FFFFFFFFFF000ULL

static uint64_t current_pml4_phys = 0;
static uint64_t* pml4_table = NULL;

static inline void invlpg(uint64_t virt_addr) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

static inline uint64_t pml4_index(uint64_t addr) { return (addr >> 39) & 0x1FF; }
static inline uint64_t pdpt_index(uint64_t addr) { return (addr >> 30) & 0x1FF; }
static inline uint64_t pd_index(uint64_t addr)   { return (addr >> 21) & 0x1FF; }
static inline uint64_t pt_index(uint64_t addr)   { return (addr >> 12) & 0x1FF; }

// Canonical address check: bits 47-63 must be all 0s or all 1s.
static bool is_canonical(uint64_t virt_addr) {
    uint64_t upper_17 = virt_addr >> 47;
    return upper_17 == 0 || upper_17 == 0x1FFFF;
}

// Convert VMM_FLAG_* to architectural PTE bits
static uint64_t flags_to_pte(uint32_t flags) {
    uint64_t pte = 0;
    if (flags & VMM_FLAG_WRITABLE)   pte |= PTE_WRITABLE;
    if (flags & VMM_FLAG_USER)       pte |= PTE_USER;
    if (flags & VMM_FLAG_NO_EXECUTE) pte |= PTE_NO_EXECUTE;
    return pte;
}

void vmm_init(uint64_t pml4_phys) {
    current_pml4_phys = pml4_phys;
    pml4_table = (uint64_t*)phys_to_virt(current_pml4_phys);
}

// Internal helper to get the next level table, allocating if necessary
static uint64_t* get_next_level(uint64_t* current_table, uint64_t index, bool allocate, uint64_t pte_flags) {
    uint64_t entry = current_table[index];
    
    if (entry & PTE_PRESENT) {
        // Must reject if we hit a huge page when we expect a directory
        if (entry & PDE_PS) {
            return NULL;
        }
        
        // Ensure intermediate directories have at least the requested permissions
        if ((pte_flags & PTE_WRITABLE) && !(entry & PTE_WRITABLE)) {
            current_table[index] |= PTE_WRITABLE;
        }
        if ((pte_flags & PTE_USER) && !(entry & PTE_USER)) {
            current_table[index] |= PTE_USER;
        }
        
        return (uint64_t*)phys_to_virt(entry & PTE_FRAME_MASK);
    }
    
    if (!allocate) return NULL;
    
    uint64_t next_table_phys = pmm_alloc_page();
    if (next_table_phys == 0) return NULL; // OOM
    
    uint64_t* next_table_virt = (uint64_t*)phys_to_virt(next_table_phys);
    for (int i = 0; i < 512; i++) {
        next_table_virt[i] = 0;
    }
    
    // Always map intermediate structures as PRESENT + whatever flags are needed for the leaf
    uint64_t intermediate_flags = PTE_PRESENT;
    if (pte_flags & PTE_WRITABLE) intermediate_flags |= PTE_WRITABLE;
    if (pte_flags & PTE_USER)     intermediate_flags |= PTE_USER;
    
    current_table[index] = next_table_phys | intermediate_flags;
    return next_table_virt;
}

bool vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint32_t flags) {
    if (!is_canonical(virt_addr)) return false;
    if (virt_addr % PMM_PAGE_SIZE != 0 || phys_addr % PMM_PAGE_SIZE != 0) return false;
    
    uint64_t arch_flags = flags_to_pte(flags);
    
    uint64_t* pdpt = get_next_level(pml4_table, pml4_index(virt_addr), true, arch_flags);
    if (!pdpt) return false; // OOM or huge page conflict
    
    uint64_t* pd = get_next_level(pdpt, pdpt_index(virt_addr), true, arch_flags);
    if (!pd) return false;
    
    uint64_t* pt = get_next_level(pd, pd_index(virt_addr), true, arch_flags);
    if (!pt) return false; // Here we specifically reject if PD has a huge page
    
    uint64_t pt_idx = pt_index(virt_addr);
    if (pt[pt_idx] & PTE_PRESENT) return false; // Reject double mapping
    
    pt[pt_idx] = phys_addr | arch_flags | PTE_PRESENT;
    invlpg(virt_addr);
    
    return true;
}

bool vmm_unmap_page(uint64_t virt_addr) {
    if (!is_canonical(virt_addr)) return false;
    if (virt_addr % PMM_PAGE_SIZE != 0) return false;
    
    uint64_t* pdpt = get_next_level(pml4_table, pml4_index(virt_addr), false, 0);
    if (!pdpt) return false;
    
    uint64_t* pd = get_next_level(pdpt, pdpt_index(virt_addr), false, 0);
    if (!pd) return false;
    
    uint64_t* pt = get_next_level(pd, pd_index(virt_addr), false, 0);
    if (!pt) return false; // Rejects if PD has huge page
    
    uint64_t pt_idx = pt_index(virt_addr);
    if (!(pt[pt_idx] & PTE_PRESENT)) return false; // Not mapped
    
    pt[pt_idx] = 0;
    invlpg(virt_addr);
    
    return true;
}

bool vmm_get_phys(uint64_t virt_addr, uint64_t *out_phys) {
    if (!is_canonical(virt_addr)) return false;
    
    uint64_t pml4e = pml4_table[pml4_index(virt_addr)];
    if (!(pml4e & PTE_PRESENT)) return false;
    
    uint64_t* pdpt = (uint64_t*)phys_to_virt(pml4e & PTE_FRAME_MASK);
    uint64_t pdpte = pdpt[pdpt_index(virt_addr)];
    if (!(pdpte & PTE_PRESENT)) return false;
    
    if (pdpte & PDE_PS) return false; 
    
    uint64_t* pd = (uint64_t*)phys_to_virt(pdpte & PTE_FRAME_MASK);
    uint64_t pde = pd[pd_index(virt_addr)];
    if (!(pde & PTE_PRESENT)) return false;
    
    // 2 MiB Huge Page (used by boot code)
    if (pde & PDE_PS) {
        if (out_phys) {
            uint64_t huge_frame = pde & PTE_FRAME_MASK;
            uint64_t offset = virt_addr & 0x1FFFFF; // 2MB offset
            *out_phys = huge_frame + offset;
        }
        return true;
    }
    
    uint64_t* pt = (uint64_t*)phys_to_virt(pde & PTE_FRAME_MASK);
    uint64_t pte = pt[pt_index(virt_addr)];
    if (!(pte & PTE_PRESENT)) return false;
    
    if (out_phys) {
        uint64_t frame = pte & PTE_FRAME_MASK;
        uint64_t offset = virt_addr & 0xFFF;
        *out_phys = frame + offset;
    }
    return true;
}

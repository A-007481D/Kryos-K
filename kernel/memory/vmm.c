#include "vmm.h"
#include "../../include/process.h"
#include "../../include/pmm.h"
#include "../../include/thread.h"
#include "virt.h"
#include <stddef.h>
#include <serial.h>

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

uint64_t vmm_get_current_pml4(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3 & PTE_FRAME_MASK;
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
            serial_puts("get_next_level: hit PS bit!\n");
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
    if (next_table_phys == 0) {
        serial_puts("get_next_level: OOM\n");
        return NULL; // OOM
    }
    
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

bool vmm_map_page(address_space_t *as, uint64_t virt_addr, uint64_t phys_addr, uint32_t flags) {
    uint64_t *root = (uint64_t*)phys_to_virt(as->pml4_phys);
    if (!is_canonical(virt_addr)) { serial_puts("vmm_map_page: not canonical\n"); return false; }
    if (virt_addr % PMM_PAGE_SIZE != 0 || phys_addr % PMM_PAGE_SIZE != 0) { serial_puts("vmm_map_page: alignment\n"); return false; }
    
    uint64_t arch_flags = flags_to_pte(flags);
    
    uint64_t* pdpt = get_next_level(root, pml4_index(virt_addr), true, arch_flags);
    if (!pdpt) { serial_puts("vmm_map_page: pdpt failed\n"); return false; }
    
    uint64_t* pd = get_next_level(pdpt, pdpt_index(virt_addr), true, arch_flags);
    if (!pd) { serial_puts("vmm_map_page: pd failed\n"); return false; }
    
    uint64_t* pt = get_next_level(pd, pd_index(virt_addr), true, arch_flags);
    if (!pt) { serial_puts("vmm_map_page: pt failed\n"); return false; }
    
    uint64_t pt_idx = pt_index(virt_addr);
    if (pt[pt_idx] & PTE_PRESENT) { serial_puts("vmm_map_page: double map\n"); return false; }
    
    pt[pt_idx] = phys_addr | arch_flags | PTE_PRESENT;
    if (as->pml4_phys == vmm_get_current_pml4()) invlpg(virt_addr);
    
    return true;
}

bool vmm_unmap_page(address_space_t *as, uint64_t virt_addr) {
    uint64_t *root = (uint64_t*)phys_to_virt(as->pml4_phys);
    if (!is_canonical(virt_addr)) return false;
    if (virt_addr % PMM_PAGE_SIZE != 0) return false;
    
    uint64_t* pdpt = get_next_level(root, pml4_index(virt_addr), false, 0);
    if (!pdpt) return false;
    
    uint64_t* pd = get_next_level(pdpt, pdpt_index(virt_addr), false, 0);
    if (!pd) return false;
    
    uint64_t* pt = get_next_level(pd, pd_index(virt_addr), false, 0);
    if (!pt) return false; // Rejects if PD has huge page
    
    uint64_t pt_idx = pt_index(virt_addr);
    if (!(pt[pt_idx] & PTE_PRESENT)) return false; // Not mapped
    
    pt[pt_idx] = 0;
    if (as->pml4_phys == vmm_get_current_pml4()) invlpg(virt_addr);
    
    return true;
}

bool vmm_get_phys(address_space_t *as, uint64_t virt_addr, uint64_t *out_phys) {
    uint64_t *root = (uint64_t*)phys_to_virt(as->pml4_phys);
    if (!is_canonical(virt_addr)) return false;
    
    uint64_t pml4e = root[pml4_index(virt_addr)];
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

bool vmm_create_address_space(address_space_t *as) {
    uint64_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) return false;
    
    uint64_t *new_pml4 = (uint64_t*)phys_to_virt(pml4_phys);
    uint64_t *kernel_pml4 = (uint64_t*)phys_to_virt(vmm_get_current_pml4());
    
    // Zero lower half (user space)
    for (int i = 0; i < 256; i++) {
        new_pml4[i] = 0;
    }
    
    // Copy higher half (kernel space)
    for (int i = 256; i < 512; i++) {
        new_pml4[i] = kernel_pml4[i];
    }
    
    as->pml4_phys = pml4_phys;
    return true;
}

static void free_page_table(uint64_t table_phys, int level) {
    if (level == 1) {
        pmm_free_page(table_phys);
        return;
    }
    
    uint64_t *table = (uint64_t*)phys_to_virt(table_phys);
    for (int i = 0; i < 512; i++) {
        if (table[i] & PTE_PRESENT) {
            if (!(table[i] & PDE_PS)) {
                free_page_table(table[i] & PTE_FRAME_MASK, level - 1);
            }
        }
    }
    pmm_free_page(table_phys);
}

void vmm_destroy_address_space(address_space_t *as) {
    if (!as || !as->pml4_phys) return;
    
    uint64_t *pml4 = (uint64_t*)phys_to_virt(as->pml4_phys);
    
    // Free only lower half (user mappings)
    for (int i = 0; i < 256; i++) {
        if (pml4[i] & PTE_PRESENT) {
            free_page_table(pml4[i] & PTE_FRAME_MASK, 3); // PDPT level is 3
        }
    }
    
// Free the PML4 itself
    pmm_free_page(as->pml4_phys);
    as->pml4_phys = 0;
}

bool vmm_is_user_readable(address_space_t *as, uintptr_t va) {
    uint64_t *root = (uint64_t*)phys_to_virt(as->pml4_phys);
    if (!is_canonical(va)) return false;
    
    uint64_t pml4e = root[pml4_index(va)];
    if (!(pml4e & PTE_PRESENT) || !(pml4e & PTE_USER)) return false;
    
    uint64_t* pdpt = (uint64_t*)phys_to_virt(pml4e & PTE_FRAME_MASK);
    uint64_t pdpte = pdpt[pdpt_index(va)];
    if (!(pdpte & PTE_PRESENT) || !(pdpte & PTE_USER)) return false;
    
    if (pdpte & PDE_PS) return true; 
    
    uint64_t* pd = (uint64_t*)phys_to_virt(pdpte & PTE_FRAME_MASK);
    uint64_t pde = pd[pd_index(va)];
    if (!(pde & PTE_PRESENT) || !(pde & PTE_USER)) return false;
    
    if (pde & PDE_PS) return true;
    
    uint64_t* pt = (uint64_t*)phys_to_virt(pde & PTE_FRAME_MASK);
    uint64_t pte = pt[pt_index(va)];
    if (!(pte & PTE_PRESENT) || !(pte & PTE_USER)) return false;
    
    return true;
}

bool user_range_readable(const void *addr, uint64_t len) {
    if (len == 0) return true;
    
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + len;
    
    // Overflow check
    if (end < start) return false;
    
    // Canonical user-space boundary check: [start, end) must be <= 0x00007FFFFFFFFFFF + 1
    // (i.e., end <= 0x0000800000000000)
    if (end > 0x0000800000000000ULL) return false;
    
    // Check every page intersecting the range
    uintptr_t page_start = start & ~0xFFFULL;
    address_space_t *as = &thread_current()->process->as;
    
    for (uintptr_t va = page_start; va < end; va += PMM_PAGE_SIZE) {
        if (!vmm_is_user_readable(as, va)) {
            return false;
        }
    }
    
    return true;
}

bool vmm_is_user_writable(address_space_t *as, uintptr_t va) {
    uint64_t *root = (uint64_t*)phys_to_virt(as->pml4_phys);
    if (!is_canonical(va)) return false;
    
    uint64_t pml4e = root[pml4_index(va)];
    if (!(pml4e & PTE_PRESENT) || !(pml4e & PTE_USER) || !(pml4e & PTE_WRITABLE)) return false;
    
    uint64_t* pdpt = (uint64_t*)phys_to_virt(pml4e & PTE_FRAME_MASK);
    uint64_t pdpte = pdpt[pdpt_index(va)];
    if (!(pdpte & PTE_PRESENT) || !(pdpte & PTE_USER) || !(pdpte & PTE_WRITABLE)) return false;
    
    if (pdpte & PDE_PS) return true; 
    
    uint64_t* pd = (uint64_t*)phys_to_virt(pdpte & PTE_FRAME_MASK);
    uint64_t pde = pd[pd_index(va)];
    if (!(pde & PTE_PRESENT) || !(pde & PTE_USER) || !(pde & PTE_WRITABLE)) return false;
    
    if (pde & PDE_PS) return true;
    
    uint64_t* pt = (uint64_t*)phys_to_virt(pde & PTE_FRAME_MASK);
    uint64_t pte = pt[pt_index(va)];
    if (!(pte & PTE_PRESENT) || !(pte & PTE_USER) || !(pte & PTE_WRITABLE)) return false;
    
    return true;
}

bool user_range_writable(const void *addr, uint64_t len) {
    if (len == 0) return true;
    
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + len;
    
    if (end < start) return false;
    if (end > 0x0000800000000000ULL) return false;
    
    uintptr_t page_start = start & ~0xFFFULL;
    address_space_t *as = &thread_current()->process->as;
    
    for (uintptr_t va = page_start; va < end; va += PMM_PAGE_SIZE) {
        if (!vmm_is_user_writable(as, va)) {
            return false;
        }
    }
    
    return true;
}

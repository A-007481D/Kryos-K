#include "../../include/elf.h"
#include "../../include/process.h"
#include "../../include/heap.h"
#include "../../include/pmm.h"
#include "../memory/vmm.h"
#include "../memory/virt.h"
#include <string.h>

// USER_STACK_TOP e.g., 0x00007FFFFFFFF000
#define USER_STACK_TOP 0x00007FFFFFFFF000
#define USER_STACK_PAGES 16

static elf_load_error_t check_elf_header(Elf64_Ehdr *hdr, size_t size) {
    if (size < sizeof(Elf64_Ehdr)) return ELF_MALFORMED_HEADER;
    if (hdr->magic != ELF_MAGIC) return ELF_BAD_MAGIC;
    if (hdr->class != ELF_CLASS64) return ELF_UNSUPPORTED_CLASS;
    if (hdr->machine != ELF_MACHINE_AMD64) return ELF_WRONG_MACHINE;
    
    // Bounds check program header table
    uint64_t ph_end = hdr->phoff + (uint64_t)hdr->phnum * hdr->phentsize;
    if (ph_end < hdr->phoff || ph_end > size) return ELF_MALFORMED_PHDR;
    
    return ELF_LOAD_SUCCESS;
}

elf_load_error_t process_create_from_elf(void *elf_data, size_t size, struct process **out_proc, uint64_t *out_entry) {
    Elf64_Ehdr *hdr = (Elf64_Ehdr *)elf_data;
    
    elf_load_error_t err = check_elf_header(hdr, size);
    if (err != ELF_LOAD_SUCCESS) return err;
    
    // Create process
    struct process *proc = process_create();
    if (!proc) return ELF_VMM_ERROR; // Out of memory
    
    // We will build the address space, if anything fails, we destroy proc and return
    
    bool entry_found = false;
    
    for (uint16_t i = 0; i < hdr->phnum; i++) {
        Elf64_Phdr *phdr = (Elf64_Phdr *)((uint8_t *)elf_data + hdr->phoff + i * hdr->phentsize);
        
        if (phdr->type != PT_LOAD) continue;
        
        if (phdr->filesz > phdr->memsz) {
            process_destroy(proc);
            return ELF_FILESZ_GT_MEMSZ;
        }
        
        uint64_t seg_file_end = phdr->offset + phdr->filesz;
        if (seg_file_end < phdr->offset || seg_file_end > size) {
            process_destroy(proc);
            return ELF_SEGMENT_OUT_OF_BOUNDS;
        }
        
        uint64_t seg_mem_end = phdr->vaddr + phdr->memsz;
        if (seg_mem_end < phdr->vaddr) {
            process_destroy(proc);
            return ELF_VADDR_OVERFLOW;
        }
        
        // Canonical check - must be user space (e.g. < 0x00007FFFFFFFFFFF)
        if (seg_mem_end > 0x00007FFFFFFFFFFF) {
            process_destroy(proc);
            return ELF_SEGMENT_NOT_USER;
        }
        
        // W^X check
        if ((phdr->flags & PF_W) && (phdr->flags & PF_X)) {
            process_destroy(proc);
            return ELF_RWX_REJECTED;
        }
        
        // Check if entry point is in this executable segment
        if (!entry_found && (phdr->flags & PF_X)) {
            if (hdr->entry >= phdr->vaddr && hdr->entry < seg_mem_end) {
                entry_found = true;
            }
        }
        
        // Map pages
        uint64_t start_page = phdr->vaddr & ~0xFFFULL;
        uint64_t end_page = (seg_mem_end + 0xFFF) & ~0xFFFULL;
        
        uint32_t vmm_flags = VMM_FLAG_USER;
        if (phdr->flags & PF_W) vmm_flags |= VMM_FLAG_WRITABLE;
        if (!(phdr->flags & PF_X)) vmm_flags |= VMM_FLAG_NO_EXECUTE;
        
        for (uint64_t page = start_page; page < end_page; page += 0x1000) {
            // Check if page already mapped
            uint64_t existing_paddr = 0;
            if (!vmm_get_phys(&proc->as, page, &existing_paddr)) {
                uint64_t paddr = pmm_alloc_page();
                if (!paddr) {
                    process_destroy(proc);
                    return ELF_VMM_ERROR;
                }
                
                // Map the physical frame into the process
                if (!vmm_map_page(&proc->as, page, paddr, vmm_flags)) {
                    pmm_free_page(paddr);
                    process_destroy(proc);
                    return ELF_VMM_ERROR;
                }
                
                // Clear the frame first via higher half
                void *vpage = phys_to_virt(paddr);
                memset(vpage, 0, 0x1000);
            } else {
                // Already mapped, verify permissions (for overlapping unaligned segments)
                // Assuming it's valid for now.
            }
            
            uint64_t paddr = 0;
            vmm_get_phys(&proc->as, page, &paddr);
            void *vpage = phys_to_virt(paddr);
            
            // Calculate how much of the segment overlaps with this page
            uint64_t page_offset = 0;
            if (page < phdr->vaddr) {
                page_offset = phdr->vaddr - page;
            }
            
            uint64_t copy_start_vaddr = page + page_offset;
            if (copy_start_vaddr < phdr->vaddr + phdr->filesz) {
                // We have file data to copy to this page
                uint64_t file_offset = phdr->offset + (copy_start_vaddr - phdr->vaddr);
                uint64_t copy_size = 0x1000 - page_offset;
                
                uint64_t file_remaining = (phdr->vaddr + phdr->filesz) - copy_start_vaddr;
                if (copy_size > file_remaining) {
                    copy_size = file_remaining;
                }
                
                memcpy((uint8_t*)vpage + page_offset, (uint8_t*)elf_data + file_offset, copy_size);
            }
        }
    }
    
    if (!entry_found) {
        process_destroy(proc);
        return ELF_ENTRY_OUT_OF_BOUNDS;
    }
    
    // Allocate and map user stack
    // We map USER_STACK_PAGES starting just below USER_STACK_TOP
    uint64_t stack_bottom = USER_STACK_TOP - (USER_STACK_PAGES * 0x1000);
    for (uint64_t page = stack_bottom; page < USER_STACK_TOP; page += 0x1000) {
        uint64_t paddr = pmm_alloc_page();
        if (!paddr) {
            process_destroy(proc);
            return ELF_STACK_ERROR;
        }
        if (!vmm_map_page(&proc->as, page, paddr, VMM_FLAG_WRITABLE | VMM_FLAG_USER | VMM_FLAG_NO_EXECUTE)) {
            pmm_free_page(paddr);
            process_destroy(proc);
            return ELF_STACK_ERROR;
        }
        memset(phys_to_virt(paddr), 0, 0x1000);
    }
    
    *out_proc = proc;
    *out_entry = hdr->entry;
    
    return ELF_LOAD_SUCCESS;
}

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

static void write_user_stack_bytes(address_space_t *as, uint64_t vaddr, const void *data, size_t len) {
    const char *src = (const char *)data;
    for (size_t i = 0; i < len; i++) {
        uint64_t curr_vaddr = vaddr + i;
        uint64_t paddr = 0;
        if (vmm_get_phys(as, curr_vaddr & ~0xFFFULL, &paddr)) {
            char *kptr = (char*)phys_to_virt(paddr) + (curr_vaddr & 0xFFF);
            *kptr = src[i];
        }
    }
}

elf_load_error_t elf_load_image(address_space_t *as, void *elf_data, size_t size, uint64_t *out_entry, uint64_t *out_rsp, int argc, const char *argv[], int envc, const char *envp[]) {
    Elf64_Ehdr *hdr = (Elf64_Ehdr *)elf_data;
    
    elf_load_error_t err = check_elf_header(hdr, size);
    if (err != ELF_LOAD_SUCCESS) return err;
    
    bool entry_found = false;
    
    for (uint16_t i = 0; i < hdr->phnum; i++) {
        Elf64_Phdr *phdr = (Elf64_Phdr *)((uint8_t *)elf_data + hdr->phoff + i * hdr->phentsize);
        
        if (phdr->type != PT_LOAD) continue;
        if (phdr->filesz > phdr->memsz) return ELF_FILESZ_GT_MEMSZ;
        
        uint64_t seg_file_end = phdr->offset + phdr->filesz;
        if (seg_file_end < phdr->offset || seg_file_end > size) return ELF_SEGMENT_OUT_OF_BOUNDS;
        
        uint64_t seg_mem_end = phdr->vaddr + phdr->memsz;
        if (seg_mem_end < phdr->vaddr) return ELF_VADDR_OVERFLOW;
        if (seg_mem_end > 0x00007FFFFFFFFFFF) return ELF_SEGMENT_NOT_USER;
        if ((phdr->flags & PF_W) && (phdr->flags & PF_X)) return ELF_RWX_REJECTED;
        
        if (!entry_found && (phdr->flags & PF_X)) {
            if (hdr->entry >= phdr->vaddr && hdr->entry < seg_mem_end) {
                entry_found = true;
            }
        }
        
        uint64_t start_page = phdr->vaddr & ~0xFFFULL;
        uint64_t end_page = (seg_mem_end + 0xFFF) & ~0xFFFULL;
        
        uint32_t vmm_flags = VMM_FLAG_USER;
        if (phdr->flags & PF_W) vmm_flags |= VMM_FLAG_WRITABLE;
        if (!(phdr->flags & PF_X)) vmm_flags |= VMM_FLAG_NO_EXECUTE;
        
        for (uint64_t page = start_page; page < end_page; page += 0x1000) {
            uint64_t existing_paddr = 0;
            if (!vmm_get_phys(as, page, &existing_paddr)) {
                uint64_t paddr = pmm_alloc_page();
                if (!paddr) return ELF_VMM_ERROR;
                if (!vmm_map_page(as, page, paddr, vmm_flags)) {
                    pmm_free_page(paddr);
                    return ELF_VMM_ERROR;
                }
                memset(phys_to_virt(paddr), 0, 0x1000);
            }
            
            uint64_t paddr = 0;
            vmm_get_phys(as, page, &paddr);
            void *vpage = phys_to_virt(paddr);
            
            uint64_t page_offset = 0;
            if (page < phdr->vaddr) {
                page_offset = phdr->vaddr - page;
            }
            
            uint64_t copy_start_vaddr = page + page_offset;
            if (copy_start_vaddr < phdr->vaddr + phdr->filesz) {
                uint64_t file_offset = phdr->offset + (copy_start_vaddr - phdr->vaddr);
                uint64_t copy_size = 0x1000 - page_offset;
                uint64_t file_remaining = (phdr->vaddr + phdr->filesz) - copy_start_vaddr;
                if (copy_size > file_remaining) copy_size = file_remaining;
                memcpy((uint8_t*)vpage + page_offset, (uint8_t*)elf_data + file_offset, copy_size);
            }
        }
    }
    
    if (!entry_found) return ELF_ENTRY_OUT_OF_BOUNDS;
    
    // Allocate and map user stack
    uint64_t stack_bottom = USER_STACK_TOP - (USER_STACK_PAGES * 0x1000);
    for (uint64_t page = stack_bottom; page < USER_STACK_TOP; page += 0x1000) {
        uint64_t paddr = pmm_alloc_page();
        if (!paddr) return ELF_STACK_ERROR;
        if (!vmm_map_page(as, page, paddr, VMM_FLAG_WRITABLE | VMM_FLAG_USER | VMM_FLAG_NO_EXECUTE)) {
            pmm_free_page(paddr);
            return ELF_STACK_ERROR;
        }
        memset(phys_to_virt(paddr), 0, 0x1000);
    }
    
    // Inject arguments into user stack
    uint64_t rsp_cur = USER_STACK_TOP;
    
    uint64_t envp_ptrs[32] = {0};
    uint64_t argv_ptrs[32] = {0};
    
    if (envc > 32) envc = 32;
    if (argc > 32) argc = 32;
    
    for (int i = envc - 1; i >= 0; i--) {
        size_t len = strlen(envp[i]) + 1;
        rsp_cur -= len;
        write_user_stack_bytes(as, rsp_cur, envp[i], len);
        envp_ptrs[i] = rsp_cur;
    }
    
    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        rsp_cur -= len;
        write_user_stack_bytes(as, rsp_cur, argv[i], len);
        argv_ptrs[i] = rsp_cur;
    }
    
    rsp_cur &= ~15ULL; // 16-byte alignment
    
    // envp pointers + NULL
    rsp_cur -= sizeof(uint64_t);
    uint64_t null_ptr = 0;
    write_user_stack_bytes(as, rsp_cur, &null_ptr, sizeof(uint64_t));
    for (int i = envc - 1; i >= 0; i--) {
        rsp_cur -= sizeof(uint64_t);
        write_user_stack_bytes(as, rsp_cur, &envp_ptrs[i], sizeof(uint64_t));
    }
    
    // argv pointers + NULL
    rsp_cur -= sizeof(uint64_t);
    write_user_stack_bytes(as, rsp_cur, &null_ptr, sizeof(uint64_t));
    for (int i = argc - 1; i >= 0; i--) {
        rsp_cur -= sizeof(uint64_t);
        write_user_stack_bytes(as, rsp_cur, &argv_ptrs[i], sizeof(uint64_t));
    }
    
    // argc
    rsp_cur -= sizeof(uint64_t);
    uint64_t argc_64 = argc;
    write_user_stack_bytes(as, rsp_cur, &argc_64, sizeof(uint64_t));
    
    *out_entry = hdr->entry;
    *out_rsp = rsp_cur;
    return ELF_LOAD_SUCCESS;
}

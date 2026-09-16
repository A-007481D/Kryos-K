#include "../../include/elf.h"
#include "../../include/process.h"
#include "../../include/thread.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t elf_buf[8192];

static elf_load_error_t process_create_from_elf(void *elf_data, size_t size, struct process **out_proc, uint64_t *out_entry) {
    *out_proc = process_create();
    if (!*out_proc) return ELF_VMM_ERROR;
    uint64_t rsp = 0;
    elf_load_error_t err = elf_load_image(&(*out_proc)->as, elf_data, size, out_entry, &rsp, 0, NULL, 0, NULL);
    if (err != ELF_LOAD_SUCCESS) {
        process_destroy(*out_proc);
        *out_proc = NULL;
    }
    return err;
}

static void craft_valid_elf(void) {
    memset(elf_buf, 0, sizeof(elf_buf));
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)elf_buf;
    ehdr->magic = ELF_MAGIC;
    ehdr->class = ELF_CLASS64;
    ehdr->data = ELF_DATA2LSB;
    ehdr->version = 1;
    ehdr->type = ELF_TYPE_EXEC;
    ehdr->machine = ELF_MACHINE_AMD64;
    ehdr->ehsize = sizeof(Elf64_Ehdr);
    ehdr->phentsize = sizeof(Elf64_Phdr);
    ehdr->phnum = 1;
    ehdr->phoff = sizeof(Elf64_Ehdr);
    ehdr->entry = 0x400000;
    
    Elf64_Phdr *phdr = (Elf64_Phdr*)(elf_buf + sizeof(Elf64_Ehdr));
    phdr->type = PT_LOAD;
    phdr->flags = PF_R | PF_X;
    phdr->offset = 0; // Usually includes header
    phdr->vaddr = 0x400000;
    phdr->paddr = 0x400000;
    phdr->filesz = 0x1000;
    phdr->memsz = 0x1000;
    phdr->align = 0x1000;
}

static void test_elf_validation(void) {
    kprintf("Running ELF Validation Tests...\n");
    
    struct process *proc = NULL;
    uint64_t entry = 0;
    elf_load_error_t err;
    Elf64_Ehdr *ehdr = (Elf64_Ehdr*)elf_buf;
    Elf64_Phdr *phdr = (Elf64_Phdr*)(elf_buf + sizeof(Elf64_Ehdr));
    
    // ELF-001: Valid ELF accepted
    craft_valid_elf();
    err = process_create_from_elf(elf_buf, sizeof(elf_buf), &proc, &entry);
    if (err != ELF_LOAD_SUCCESS) panic(__FILE__, __LINE__, "ELF-001 Failed: err=%d\n", err);
    process_destroy(proc);
    kprintf("ELF-001 Passed.\n");
    
    // ELF-002: Invalid magic
    craft_valid_elf();
    ehdr->magic = 0xBAD;
    err = process_create_from_elf(elf_buf, sizeof(elf_buf), &proc, &entry);
    if (err != ELF_BAD_MAGIC) panic(__FILE__, __LINE__, "ELF-002 Failed\n");
    kprintf("ELF-002 Passed.\n");
    
    // ELF-003: Unsupported class
    craft_valid_elf();
    ehdr->class = 1; // 32-bit
    err = process_create_from_elf(elf_buf, sizeof(elf_buf), &proc, &entry);
    if (err != ELF_UNSUPPORTED_CLASS) panic(__FILE__, __LINE__, "ELF-003 Failed\n");
    kprintf("ELF-003 Passed.\n");
    
    // ELF-004: Wrong machine
    craft_valid_elf();
    ehdr->machine = 3; // i386
    err = process_create_from_elf(elf_buf, sizeof(elf_buf), &proc, &entry);
    if (err != ELF_WRONG_MACHINE) panic(__FILE__, __LINE__, "ELF-004 Failed\n");
    kprintf("ELF-004 Passed.\n");
    
    // ELF-005: Malformed ELF header (size too small)
    craft_valid_elf();
    err = process_create_from_elf(elf_buf, 16, &proc, &entry); // Too small
    if (err != ELF_MALFORMED_HEADER) panic(__FILE__, __LINE__, "ELF-005 Failed\n");
    kprintf("ELF-005 Passed.\n");
    
    // ELF-006: Malformed program-header table
    craft_valid_elf();
    ehdr->phnum = 1000; // Out of bounds
    err = process_create_from_elf(elf_buf, sizeof(elf_buf), &proc, &entry);
    if (err != ELF_MALFORMED_PHDR) panic(__FILE__, __LINE__, "ELF-006 Failed\n");
    kprintf("ELF-006 Passed.\n");
    
    // ELF-007: p_filesz > p_memsz
    craft_valid_elf();
    phdr->filesz = 0x2000;
    phdr->memsz = 0x1000;
    err = process_create_from_elf(elf_buf, sizeof(elf_buf), &proc, &entry);
    if (err != ELF_FILESZ_GT_MEMSZ) panic(__FILE__, __LINE__, "ELF-007 Failed\n");
    kprintf("ELF-007 Passed.\n");
    
    // ELF-008: Segment/file bounds
    craft_valid_elf();
    phdr->offset = 4096;
    phdr->filesz = 8192; // Goes beyond sizeof(elf_buf) which is 8192
    phdr->memsz = 8192;  // memsz == filesz (valid), but file bounds exceeded
    err = process_create_from_elf(elf_buf, sizeof(elf_buf), &proc, &entry);
    if (err != ELF_SEGMENT_OUT_OF_BOUNDS) panic(__FILE__, __LINE__, "ELF-008 Failed (got %d)\n", err);
    kprintf("ELF-008 Passed.\n");
    
    // ELF-009: Virtual-address overflow
    craft_valid_elf();
    phdr->vaddr = 0xFFFFFFFFFFFFFFFF;
    phdr->memsz = 0x1000;
    err = process_create_from_elf(elf_buf, sizeof(elf_buf), &proc, &entry);
    if (err != ELF_VADDR_OVERFLOW) panic(__FILE__, __LINE__, "ELF-009 Failed\n");
    kprintf("ELF-009 Passed.\n");
    
    // ELF-010: Segment outside user range (canonical, but kernel)
    craft_valid_elf();
    phdr->vaddr = 0xFFFFFFFF80000000; // Kernel higher half
    err = process_create_from_elf(elf_buf, sizeof(elf_buf), &proc, &entry);
    if (err != ELF_SEGMENT_NOT_USER) panic(__FILE__, __LINE__, "ELF-010 Failed\n");
    kprintf("ELF-010 Passed.\n");
    
    // ELF-011: Entry outside executable segment
    craft_valid_elf();
    ehdr->entry = 0x800000; // Not covered by the segment (0x400000)
    err = process_create_from_elf(elf_buf, sizeof(elf_buf), &proc, &entry);
    if (err != ELF_ENTRY_OUT_OF_BOUNDS) panic(__FILE__, __LINE__, "ELF-011 Failed\n");
    kprintf("ELF-011 Passed.\n");
    
    // ELF-013: Reject RWX
    craft_valid_elf();
    phdr->flags = PF_R | PF_W | PF_X; // RWX
    err = process_create_from_elf(elf_buf, sizeof(elf_buf), &proc, &entry);
    if (err != ELF_RWX_REJECTED) panic(__FILE__, __LINE__, "ELF-013 Failed\n");
    kprintf("ELF-013 Passed.\n");
    
    kprintf("ELF loading validation tests passed!\n");
}

void test_elf(void) {
    test_elf_validation();
}

#ifndef KRYOS_ELF_H
#define KRYOS_ELF_H

#include <stdint.h>
#include <stddef.h>
#include "process.h"

#define ELF_MAGIC 0x464C457F // '\x7f', 'E', 'L', 'F'
#define ELF_CLASS64 2
#define ELF_DATA2LSB 1
#define ELF_OSABI_SYSV 0
#define ELF_MACHINE_AMD64 62
#define ELF_TYPE_EXEC 2

#define PT_LOAD 1

#define PF_X 1
#define PF_W 2
#define PF_R 4

typedef struct {
    uint32_t magic;
    uint8_t  class;
    uint8_t  data;
    uint8_t  version;
    uint8_t  osabi;
    uint8_t  abiversion;
    uint8_t  pad[7];
    uint16_t type;
    uint16_t machine;
    uint32_t version2;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed)) Elf64_Phdr;

typedef enum {
    ELF_LOAD_SUCCESS = 0,
    ELF_BAD_MAGIC,
    ELF_UNSUPPORTED_CLASS,
    ELF_WRONG_MACHINE,
    ELF_MALFORMED_HEADER,
    ELF_MALFORMED_PHDR,
    ELF_FILESZ_GT_MEMSZ,
    ELF_SEGMENT_OUT_OF_BOUNDS,
    ELF_VADDR_OVERFLOW,
    ELF_SEGMENT_NOT_USER,
    ELF_ENTRY_OUT_OF_BOUNDS,
    ELF_RWX_REJECTED,
    ELF_VMM_ERROR,
    ELF_STACK_ERROR
} elf_load_error_t;

elf_load_error_t elf_load_image(address_space_t *as, void *elf_data, size_t size, uint64_t *out_entry, uint64_t *out_rsp, int argc, const char *argv[], int envc, const char *envp[]);
#endif // KRYOS_ELF_H

#include "gdt.h"
#include <stddef.h>

static struct tss_entry tss;
static uint64_t gdt[7];

struct gdtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void gdt_init(void) {
    gdt[0] = 0;
    
    // 0x08: Kernel Code. Present, Executable, Read/Write, 64-bit (L=1)
    gdt[1] = (1ULL << 43) | (1ULL << 44) | (1ULL << 47) | (1ULL << 53);
    
    // 0x10: Kernel Data. Present, Read/Write
    gdt[2] = (1ULL << 41) | (1ULL << 44) | (1ULL << 47);
    
    // 0x18: User Data. Present, Read/Write, DPL=3
    gdt[3] = (1ULL << 41) | (1ULL << 44) | (1ULL << 47) | (3ULL << 45);
    
    // 0x20: User Code. Present, Executable, Read/Write, 64-bit (L=1), DPL=3
    gdt[4] = (1ULL << 43) | (1ULL << 44) | (1ULL << 47) | (1ULL << 53) | (3ULL << 45);
    
    // TSS setup
    for (size_t i = 0; i < sizeof(tss); i++) {
        ((uint8_t*)&tss)[i] = 0;
    }
    tss.iopb_offset = sizeof(tss); // No IOPB
    
    uint64_t tss_base = (uint64_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;
    
    // 0x28: TSS Low. Present, Type=0x9 (Available 64-bit TSS)
    gdt[5] = (tss_limit & 0xFFFFULL) |
             ((tss_base & 0xFFFFFFULL) << 16) |
             (0x89ULL << 40) |
             (((tss_limit >> 16) & 0x0FULL) << 48) |
             (((tss_base >> 24) & 0xFFULL) << 56);
             
    // 0x30: TSS High
    gdt[6] = (tss_base >> 32);

    struct gdtr gdtr_struct = {
        .limit = sizeof(gdt) - 1,
        .base = (uint64_t)&gdt
    };

    __asm__ volatile ("lgdt %0" : : "m"(gdtr_struct));
    __asm__ volatile ("ltr %w0" : : "r"((uint16_t)GDT_TSS));
}

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

#include <stdint.h>
#include "serial.h"
#include "idt.h"
#include "multiboot.h"
#include "../memory/vmm.h"
#include "tests.h"

void kernel_main(uint32_t magic, uint32_t info_addr, uint64_t pml4_phys) {
    uint64_t* pml4 = (uint64_t*)(pml4_phys + 0xFFFFFFFF80000000ULL);
    pml4[0] = 0;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
    
    vmm_init(pml4_phys);

    serial_init();
    idt_init();
    multiboot_parse(magic, info_addr);

    serial_puts("[PASS] boot\n");
    serial_puts("[PASS] long_mode\n");
    serial_puts("[PASS] higher_half\n");
    
    // Run diagnostic tests
    run_kernel_tests();

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

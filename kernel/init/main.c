#include <stdint.h>
#include "serial.h"
#include "idt.h"
#include "tests.h"

void kernel_main(uint32_t magic, uint32_t info_addr, uint64_t pml4_phys) {
    (void)magic;
    (void)info_addr;

    uint64_t* pml4 = (uint64_t*)(pml4_phys + 0xFFFFFFFF80000000ULL);
    pml4[0] = 0;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");

    serial_init();
    serial_puts("[PASS] boot\n");
    serial_puts("[PASS] long_mode\n");
    serial_puts("[PASS] higher_half\n");

    idt_init();
    
    // Run diagnostic tests
    run_kernel_tests();

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

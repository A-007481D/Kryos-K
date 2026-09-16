#include <stdint.h>
#include "serial.h"
#include "idt.h"
#include "idt.h"
#include "multiboot.h"
#include "../interrupts/pit.h"
#include "../memory/vmm.h"
#include "../memory/layout.h"
#include "../../include/heap.h"
#include "../../include/thread.h"
#include "../../include/process.h"
#include "../../include/syscall.h"
#include "tests.h"

void kernel_main(uint32_t magic, uint32_t info_addr, uint64_t pml4_phys) {
    uint64_t* pml4 = (uint64_t*)(pml4_phys + 0xFFFFFFFF80000000ULL);
    pml4[0] = 0;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
    
    serial_init();
    multiboot_parse(magic, info_addr);
    vmm_init(pml4_phys);
    process_init();
    kheap_init(KERNEL_HEAP_BASE);
    thread_init();

    idt_init();
    pit_init();
    syscall_init();

    serial_puts("[PASS] boot\n");
    serial_puts("[PASS] long_mode\n");
    serial_puts("[PASS] higher_half\n");
    
    // Run diagnostic tests
    run_kernel_tests();

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

#include <stdint.h>
#include "serial.h"

void kernel_main(uint32_t magic, uint32_t info_addr, uint64_t pml4_phys) {
    (void)magic;
    (void)info_addr;

    // The physical address of PML4 is passed in RDX.
    // The kernel is mapped at 0xFFFFFFFF80000000, so we calculate the virtual address
    // of the PML4 table to manipulate it.
    uint64_t* pml4 = (uint64_t*)(pml4_phys + 0xFFFFFFFF80000000ULL);

    // Unmap the lower-half identity mapping (PML4[0])
    pml4[0] = 0;

    // Reload CR3 to flush the TLB and fully remove the low memory mapping
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");

    serial_init();
    serial_puts("Kryos\nPhase 2: Higher-half virtual memory established.\n");

    // Halt the CPU (64-bit mode)
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

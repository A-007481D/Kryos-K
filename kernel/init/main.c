#include <stdint.h>
#include "serial.h"

void kernel_main(uint32_t magic, uint32_t info_addr) {
    // We intentionally ignore magic and info_addr for Phase 0,
    // but preserving them is part of the Multiboot2 contract.
    (void)magic;
    (void)info_addr;

    serial_init();
    serial_puts("Kryos\nBooting kernel...\n");

    // Halt the CPU (32-bit bootstrap)
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

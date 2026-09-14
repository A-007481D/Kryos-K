#include <stdint.h>
#include "serial.h"

void kernel_main(uint32_t magic, uint32_t info_addr) {
    // We intentionally ignore magic and info_addr for Phase 1,
    // but preserving them is part of the Multiboot2 contract.
    (void)magic;
    (void)info_addr;

    serial_init();
    serial_puts("Kryos\nPhase 1: x86_64 long mode active.\n");

    // Halt the CPU (64-bit mode)
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

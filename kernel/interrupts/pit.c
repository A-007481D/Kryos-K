#include "pit.h"
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void pit_init(void) {
    // Base frequency is 1193182 Hz
    // To get ~100 Hz, we divide by 11931
    uint32_t divisor = 11931;
    
    // Command byte: Channel 0, Access mode lo/hi, Mode 2, Binary (0x36)
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

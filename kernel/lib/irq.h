#pragma once

#include <stdint.h>

typedef uint64_t irq_state_t;

static inline irq_state_t irq_save(void) {
    irq_state_t flags;
    __asm__ volatile(
        "pushfq\n"
        "popq %0\n"
        "cli\n"
        : "=r"(flags)
        : : "memory"
    );
    return flags;
}

static inline void irq_restore(irq_state_t flags) {
    __asm__ volatile(
        "pushq %0\n"
        "popfq\n"
        : : "r"(flags)
        : "memory", "cc"
    );
}

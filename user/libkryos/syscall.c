#include "include/kryos.h"

uint64_t __syscall(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a3;
    (void)a4;
    (void)a5;
    
    uint64_t ret;
    /* 
     * The Kryos ABI currently expects up to 3 arguments in:
     * RDI, RSI, RDX
     * 
     * We accept 6 arguments in C to be extensible, but we only pass 3 to the syscall instruction 
     * for now, per the Phase 15 contract.
     */
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a0), "S"(a1), "d"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

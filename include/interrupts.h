#ifndef KRYOS_INTERRUPTS_H
#define KRYOS_INTERRUPTS_H

#include <stdint.h>

// Normalized exception frame.
// The ISR assembly stubs push the error code (or a dummy 0) and the interrupt vector,
// followed by the general-purpose registers. The CPU pushes RIP, CS, RFLAGS, RSP, and SS.
typedef struct {
    // Pushed by ISR stub (common handler)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    
    // Pushed by specific ISR stub
    uint64_t vector;
    uint64_t error_code;
    
    // Pushed by the CPU on exception
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) exception_frame_t;

typedef struct {
    uint64_t recovery_rip;
    uint64_t recovery_rsp;
    uint64_t recovery_rbp;
    uint64_t recovery_rbx;
    uint64_t recovery_r12;
    uint64_t recovery_r13;
    uint64_t recovery_r14;
    uint64_t recovery_r15;
    uint8_t  expected_vector;
    uint64_t expected_cr2; // Only used for Page Faults (#PF)
    bool     active;
} exception_test_context_t;

extern volatile exception_test_context_t current_test_context;

void fault_handler(exception_frame_t *frame);

#endif

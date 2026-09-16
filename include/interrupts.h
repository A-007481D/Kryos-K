#ifndef KRYOS_INTERRUPTS_H
#define KRYOS_INTERRUPTS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Normalized exception frame.
// The ISR assembly stubs push the error code (or a dummy 0) and the interrupt vector,
// followed by the general-purpose registers. The CPU pushes RIP, CS, RFLAGS, RSP, and SS.
typedef struct {
    // Pushed by ISR stub (common handler)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    
    // Pushed by specific ISR stub
    uint64_t int_no;
    uint64_t err_code;
    
    // Pushed by the CPU
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
} __attribute__((packed)) kernel_interrupt_frame;

_Static_assert(offsetof(kernel_interrupt_frame, int_no) == 15 * 8, "int_no offset is wrong");
_Static_assert(offsetof(kernel_interrupt_frame, err_code) == 16 * 8, "err_code offset is wrong");
_Static_assert(offsetof(kernel_interrupt_frame, rip) == 17 * 8, "rip offset is wrong");
_Static_assert(sizeof(kernel_interrupt_frame) == 20 * 8, "kernel_interrupt_frame size is wrong");

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

void fault_handler(kernel_interrupt_frame *frame);

#endif

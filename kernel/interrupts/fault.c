#include "interrupts.h"
#include "assert.h"
#include "stdio.h"
#include <stddef.h>

exception_test_context_t *current_test_context = NULL;

static const char *exception_names[32] = {
    "Divide Error", "Debug", "NMI", "Breakpoint", "Overflow", "BOUND Range Exceeded",
    "Invalid Opcode", "Device Not Available", "Double Fault", "Coprocessor Segment Overrun",
    "Invalid TSS", "Segment Not Present", "Stack-Segment Fault", "General Protection Fault",
    "Page Fault", "Reserved", "x87 Floating-Point Exception", "Alignment Check",
    "Machine Check", "SIMD Floating-Point Exception", "Virtualization Exception",
    "Control Protection Exception", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Hypervisor Injection Exception", "VMM Communication Exception",
    "Security Exception", "Reserved"
};

void fault_handler(exception_frame_t *frame) {
    uint64_t cr2 = 0;
    if (frame->vector == 14) {
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    }

    if (current_test_context && current_test_context->active) {
        if (frame->vector != current_test_context->expected_vector) {
            panic(__FILE__, __LINE__, 
                "Test failed: Expected exception %d, but got %d (%s)", 
                current_test_context->expected_vector, 
                frame->vector, 
                frame->vector < 32 ? exception_names[frame->vector] : "Unknown");
        }
        
        if (frame->vector == 14) {
            if (cr2 != current_test_context->expected_cr2) {
                panic(__FILE__, __LINE__, 
                    "Test failed: Expected CR2 0x%p, but got 0x%p", 
                    current_test_context->expected_cr2, cr2);
            }
        }
        
        // Recover execution
        frame->rip = current_test_context->recovery_rip;
        current_test_context->active = false;
        return;
    }
    
    // Unhandled exception
    kprintf("\n--- UNHANDLED CPU EXCEPTION ---\n");
    kprintf("Vector: %d (%s)\n", frame->vector, frame->vector < 32 ? exception_names[frame->vector] : "Unknown");
    kprintf("Error Code: 0x%x\n", frame->error_code);
    kprintf("RIP: 0x%p\n", frame->rip);
    kprintf("RSP: 0x%p\n", frame->rsp);
    kprintf("RFLAGS: 0x%x\n", frame->rflags);
    if (frame->vector == 14) {
        kprintf("CR2 (Faulting Address): 0x%p\n", cr2);
    }
    
    panic(__FILE__, __LINE__, "Unhandled exception %d", frame->vector);
}

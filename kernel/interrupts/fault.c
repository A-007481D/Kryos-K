#include "interrupts.h"
#include "assert.h"
#include "stdio.h"
#include "pic.h"
#include "../../include/thread.h"
#include <stddef.h>

volatile exception_test_context_t current_test_context = {0};

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

void fault_handler(kernel_interrupt_frame *frame) {
    if (frame->int_no == 32) {
        timer_handler();
        return;
    }

    uint64_t cr2 = 0;
    if (frame->int_no == 14) {
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    }

    if (current_test_context.active) {
        if (frame->int_no != current_test_context.expected_vector) {
            panic(__FILE__, __LINE__, 
                "Test failed: Expected exception %d, but got %d (%s)", 
                current_test_context.expected_vector, 
                frame->int_no, 
                frame->int_no < 32 ? exception_names[frame->int_no] : "Unknown");
        }
        
        if (frame->int_no == 14) {
            if (cr2 != current_test_context.expected_cr2) {
                panic(__FILE__, __LINE__, 
                    "Test failed: Expected CR2 0x%p, but got 0x%p", 
                    current_test_context.expected_cr2, cr2);
            }
        }
        
        // Recover execution
        frame->rip = current_test_context.recovery_rip;
        
        // Although the user's conceptual kernel_interrupt_frame ends at RFLAGS,
        // x86-64 long mode unconditionally pushes RSP and SS for all interrupts.
        // We must modify the pushed RSP so iretq restores the stack to the recovery point,
        // preventing a stack leak from the nested panic() call.
        uint64_t* hardware_rsp_slot = (uint64_t*)((uint8_t*)frame + sizeof(kernel_interrupt_frame));
        *hardware_rsp_slot = current_test_context.recovery_rsp;
        
        frame->rbp = current_test_context.recovery_rbp;
        frame->rbx = current_test_context.recovery_rbx;
        frame->r12 = current_test_context.recovery_r12;
        frame->r13 = current_test_context.recovery_r13;
        frame->r14 = current_test_context.recovery_r14;
        frame->r15 = current_test_context.recovery_r15;
        current_test_context.active = false;
        return;
    }
    
    // Unhandled exception
    kprintf("\n--- UNHANDLED CPU EXCEPTION ---\n");
    kprintf("Vector: %d (%s)\n", frame->int_no, frame->int_no < 32 ? exception_names[frame->int_no] : "Unknown");
    kprintf("Error Code: 0x%x\n", frame->err_code);
    kprintf("RIP: 0x%p\n", frame->rip);
    kprintf("RFLAGS: 0x%x\n", frame->rflags);
    if (frame->int_no == 14) {
        kprintf("CR2 (Faulting Address): 0x%p\n", cr2);
    }
    
    panic(__FILE__, __LINE__, "Unhandled exception %d", frame->int_no);
}

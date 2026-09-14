#include "tests.h"
#include "interrupts.h"
#include "stdio.h"
#include "assert.h"
#include "serial.h"
#include <stdint.h>
#include <stdbool.h>

static void test_kassert(void) {
    KASSERT(1 == 1);
    serial_puts("[PASS] kassert\n");
}

static void test_divide_by_zero(void) {
    exception_test_context_t ctx = {0};
    ctx.expected_vector = 0; // #DE
    ctx.active = true;
    ctx.recovery_rip = (uint64_t)&&recovery;
    
    current_test_context = &ctx;
    
    __asm__ volatile("div %0" : : "r"(0));
    
    panic(__FILE__, __LINE__, "Divide by zero did not fault");
    
recovery:
    current_test_context = NULL;
    serial_puts("[PASS] divide_by_zero\n");
}

static void test_invalid_opcode(void) {
    exception_test_context_t ctx = {0};
    ctx.expected_vector = 6; // #UD
    ctx.active = true;
    ctx.recovery_rip = (uint64_t)&&recovery;
    
    current_test_context = &ctx;
    
    __asm__ volatile("ud2");
    
    panic(__FILE__, __LINE__, "Invalid opcode did not fault");
    
recovery:
    current_test_context = NULL;
    serial_puts("[PASS] invalid_opcode\n");
}

static void test_page_fault(void) {
    exception_test_context_t ctx = {0};
    ctx.expected_vector = 14; // #PF
    ctx.expected_cr2 = 0xDEADBEEF;
    ctx.active = true;
    ctx.recovery_rip = (uint64_t)&&recovery;
    
    current_test_context = &ctx;
    
    volatile uint64_t *ptr = (volatile uint64_t *)0xDEADBEEF;
    *ptr = 0xCAFEBABE;
    
    panic(__FILE__, __LINE__, "Page fault did not trigger");
    
recovery:
    current_test_context = NULL;
    serial_puts("[PASS] page_fault\n");
}

static void test_panic(void) {
    serial_puts("[PASS] all tests completed\n");
    // Trigger expected panic. The Python runner will detect this specific panic
    // after the completion message as proof that panics work.
    KASSERT(false);
}

void run_kernel_tests(void) {
    test_kassert();
    test_divide_by_zero();
    test_invalid_opcode();
    test_page_fault();
    
    test_panic();
}

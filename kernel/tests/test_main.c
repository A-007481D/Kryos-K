#include "tests.h"
#include "interrupts.h"
#include "stdio.h"
#include "assert.h"
#include "serial.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdbool.h>

static void test_kassert(void) {
    KASSERT(1 == 1);
    serial_puts("[PASS] kassert\n");
}

static void test_divide_by_zero(void) {
    exception_test_context_t ctx = {0};
    ctx.expected_vector = 0; // #DE
    ctx.active = true;
    current_test_context = &ctx;
    
    __asm__ volatile(
        "lea 1f(%%rip), %%rcx\n"
        "mov %%rcx, %0\n"
        "div %1\n"
        "1:\n"
        : "=m"(ctx.recovery_rip)
        : "r"(0)
        : "rcx", "rax", "rdx", "memory"
    );
    
    current_test_context = NULL;
    serial_puts("[PASS] divide_by_zero\n");
}

static void test_invalid_opcode(void) {
    exception_test_context_t ctx = {0};
    ctx.expected_vector = 6; // #UD
    ctx.active = true;
    current_test_context = &ctx;
    
    __asm__ volatile(
        "lea 1f(%%rip), %%rcx\n"
        "mov %%rcx, %0\n"
        "ud2\n"
        "1:\n"
        : "=m"(ctx.recovery_rip)
        :
        : "rcx", "memory"
    );
    
    current_test_context = NULL;
    serial_puts("[PASS] invalid_opcode\n");
}

static void test_page_fault(void) {
    exception_test_context_t ctx = {0};
    ctx.expected_vector = 14; // #PF
    ctx.expected_cr2 = 0xDEADBEEF;
    ctx.active = true;
    current_test_context = &ctx;
    
    __asm__ volatile(
        "lea 1f(%%rip), %%rcx\n"
        "mov %%rcx, %0\n"
        "mov %%rdx, (%%rax)\n"
        "1:\n"
        : "=m"(ctx.recovery_rip)
        : "a"(0xDEADBEEF), "d"(0xCAFEBABEULL)
        : "rcx", "memory"
    );
    
    current_test_context = NULL;
    serial_puts("[PASS] page_fault\n");
}

static void test_panic(void) {
    serial_puts("@@KRYOS:SUITE:PASS\n");
    serial_puts("@@KRYOS:TEST:panic:EXPECTED\n");
    // Trigger expected panic.
    KASSERT(false);
}

void run_kernel_tests(void) {
    test_kassert();
    test_divide_by_zero();
    test_invalid_opcode();
    test_page_fault();
    
    test_panic();
}

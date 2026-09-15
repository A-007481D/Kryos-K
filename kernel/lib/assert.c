#include "assert.h"
#include <stdio.h>
#include <interrupts.h>
#include "serial.h"
#include <stdarg.h>

void panic(const char *file, int line, const char *fmt, ...) {
    serial_puts("\n===================================================\n");
    serial_puts("KERNEL PANIC\n");
    kprintf("Location: %s:%d\n", file, line);
    serial_puts("Reason: ");
    
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
    
    if (current_test_context.active) {
        // If we are in a test context that expects a panic, we will trigger a #UD
        // to gracefully fall into the exception handler and recover.
        serial_puts(" (Panic intercepted by test context)\n");
        __asm__ volatile("ud2");
    }
    
    serial_puts("===================================================\n");

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

#include "assert.h"
#include "stdio.h"
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
    
    serial_puts("\n===================================================\n");
    
    // Halt the CPU
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

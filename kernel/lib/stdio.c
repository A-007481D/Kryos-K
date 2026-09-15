#include "stdio.h"
#include "serial.h"
#include <stdint.h>

static void print_uint(uint64_t value, int base, int width, char pad) {
    char buf[64];
    int i = 0;
    
    if (value == 0) {
        buf[i++] = '0';
    } else {
        while (value > 0) {
            int digit = value % base;
            buf[i++] = digit < 10 ? '0' + digit : 'a' + (digit - 10);
            value /= base;
        }
    }
    
    while (i < width) {
        buf[i++] = pad;
    }
    
    while (i > 0) {
        i--;
        serial_putc(buf[i]);
    }
}

static void print_int(int64_t value, int base) {
    if (value < 0) {
        serial_putc('-');
        value = -value;
    }
    print_uint((uint64_t)value, base, 0, ' ');
}

void kvprintf(const char *format, va_list args) {
    for (const char *p = format; *p; p++) {
        if (*p != '%') {
            serial_putc(*p);
            continue;
        }
        
        p++;
        int is_long = 0;
        if (*p == 'l') {
            is_long = 1;
            p++;
        }
        
        if (!*p) break;
        
        switch (*p) {
            case 'd':
                if (is_long) print_int(va_arg(args, int64_t), 10);
                else print_int(va_arg(args, int), 10);
                break;
            case 'u':
                if (is_long) print_uint(va_arg(args, uint64_t), 10, 0, ' ');
                else print_uint(va_arg(args, unsigned int), 10, 0, ' ');
                break;
            case 'x':
                if (is_long) print_uint(va_arg(args, uint64_t), 16, 0, ' ');
                else print_uint(va_arg(args, unsigned int), 16, 0, ' ');
                break;
            case 'p':
                serial_puts("0x");
                print_uint((uint64_t)va_arg(args, void*), 16, 16, '0');
                break;
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                serial_puts(s);
                break;
            }
            case 'c':
                serial_putc((char)va_arg(args, int));
                break;
            case '%':
                serial_putc('%');
                break;
            default:
                serial_putc('%');
                serial_putc(*p);
                break;
        }
    }
}

void kprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    kvprintf(format, args);
    va_end(args);
}

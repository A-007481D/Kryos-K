#ifndef KRYOS_STDIO_H
#define KRYOS_STDIO_H

#include <stdarg.h>

void kprintf(const char *format, ...);
void kvprintf(const char *format, va_list args);

#endif

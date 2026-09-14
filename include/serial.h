#ifndef KRYOS_SERIAL_H
#define KRYOS_SERIAL_H

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);

#endif // KRYOS_SERIAL_H

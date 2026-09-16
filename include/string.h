#ifndef KRYOS_STRING_H
#define KRYOS_STRING_H

#include <stddef.h>

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int memcmp(const void *s1, const void *s2, size_t n);
char *strncpy(char *dest, const char *src, size_t n);

#endif

#ifndef KRYOS_ASSERT_H
#define KRYOS_ASSERT_H

#include <stdbool.h>

void panic(const char *file, int line, const char *fmt, ...);

#define KASSERT(cond) \
    do { \
        if (!(cond)) { \
            panic(__FILE__, __LINE__, "Assertion failed: %s", #cond); \
        } \
    } while (0)

#endif

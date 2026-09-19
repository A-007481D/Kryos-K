#ifndef KRYOS_TEST_FRAMEWORK_H
#define KRYOS_TEST_FRAMEWORK_H

#include "stdio.h"
#include "assert.h"

// Define a global to track the currently running test
extern const char *current_test_name;

#define TEST_BEGIN(name) \
    do { \
        current_test_name = (name); \
        kprintf("\n@@KRYOS:TEST:%s:BEGIN\n", current_test_name); \
    } while (0)

#define TEST_END() \
    do { \
        if (current_test_name) { \
            kprintf("\n@@KRYOS:TEST:%s:PASS\n", current_test_name); \
            current_test_name = NULL; \
        } \
    } while (0)

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            if (current_test_name) { \
                kprintf("\n@@KRYOS:TEST:%s:FAIL\n", current_test_name); \
            } \
            panic(__FILE__, __LINE__, "TEST_ASSERT failed: %s", #cond); \
        } \
    } while (0)

#endif // KRYOS_TEST_FRAMEWORK_H

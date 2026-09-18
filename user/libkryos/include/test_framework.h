#ifndef USER_TEST_FRAMEWORK_H
#define USER_TEST_FRAMEWORK_H

#include "kryos.h"

static const char *current_user_test_name = NULL;

static inline void _test_print(const char *msg) {
    write(1, msg, strlen(msg));
}

#define TEST_BEGIN(name) \
    do { \
        current_user_test_name = (name); \
        _test_print("@@KRYOS:TEST:"); \
        _test_print(current_user_test_name); \
        _test_print(":BEGIN\n"); \
    } while (0)

#define TEST_END() \
    do { \
        if (current_user_test_name) { \
            _test_print("@@KRYOS:TEST:"); \
            _test_print(current_user_test_name); \
            _test_print(":PASS\n"); \
            current_user_test_name = NULL; \
        } \
    } while (0)

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            if (current_user_test_name) { \
                _test_print("@@KRYOS:TEST:"); \
                _test_print(current_user_test_name); \
                _test_print(":FAIL\n"); \
            } \
            _test_print("ASSERTION FAILED!\n"); \
            exit(1); \
        } \
    } while (0)

#endif // USER_TEST_FRAMEWORK_H

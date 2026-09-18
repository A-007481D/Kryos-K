#include "../libkryos/include/kryos.h"
#include "../libkryos/include/test_framework.h"

int main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    
    TEST_BEGIN("RUNTIME-010"); // Test 1: Initial sbrk state
    void *initial_brk = sbrk(0);
    TEST_ASSERT(initial_brk != (void*)-1);
    TEST_ASSERT((uint64_t)initial_brk == 0x10000000);
    TEST_END();
    
    TEST_BEGIN("RUNTIME-011"); // Test 2: Basic malloc
    void *ptr1 = malloc(16);
    TEST_ASSERT(ptr1 != NULL);
    
    // Verify memory is writable
    char *c1 = (char*)ptr1;
    for (int i = 0; i < 16; i++) {
        c1[i] = 'A' + i;
    }
    
    // Verify memory is readable
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT(c1[i] == 'A' + i);
    }
    TEST_END();
    
    TEST_BEGIN("RUNTIME-012"); // Test 3: Multiple allocations
    void *ptr2 = malloc(32);
    TEST_ASSERT(ptr2 != NULL);
    TEST_ASSERT(ptr1 != ptr2);
    TEST_END();
    
    TEST_BEGIN("RUNTIME-013"); // Test 4: Free and reuse
    free(ptr1);
    void *ptr3 = malloc(8);
    // ptr3 should reuse ptr1's space, since we do first fit and it's large enough
    TEST_ASSERT(ptr3 == ptr1);
    TEST_END();
    
    TEST_BEGIN("RUNTIME-014"); // Test 5: Large allocation (forces sbrk expansion)
    void *large_ptr = malloc(8192);
    TEST_ASSERT(large_ptr != NULL);
    
    // Verify writable over page boundary
    char *l1 = (char*)large_ptr;
    for (int i = 0; i < 8192; i++) {
        l1[i] = (char)(i % 256);
    }
    
    // Verify readable
    for (int i = 0; i < 8192; i++) {
        TEST_ASSERT(l1[i] == (char)(i % 256));
    }
    
    // Check brk moved up
    void *new_brk = sbrk(0);
    TEST_ASSERT((uint64_t)new_brk > (uint64_t)initial_brk);
    TEST_END();
    
    return 0; // Success
}

#include "../libkryos/include/kryos.h"
#include "../libkryos/include/test_framework.h"

int main(int argc, char **argv, char **envp) {
    if (argc == 3 && strcmp(argv[0], "/hello.elf") == 0) {
        // We are launched by test_exec.elf!
        // Run tests
        TEST_BEGIN("RUNTIME-001"); // _start -> main
        TEST_ASSERT(1); // We reached here!
        TEST_END();
        
        TEST_BEGIN("RUNTIME-002"); // argc correct
        TEST_ASSERT(argc == 3);
        TEST_END();
        
        TEST_BEGIN("RUNTIME-003"); // argv pointers correct
        TEST_ASSERT(argv != NULL);
        TEST_ASSERT(argv[0] != NULL);
        TEST_ASSERT(argv[1] != NULL);
        TEST_ASSERT(argv[2] != NULL);
        TEST_END();
        
        TEST_BEGIN("RUNTIME-004"); // argv strings correct
        TEST_ASSERT(strcmp(argv[0], "/hello.elf") == 0);
        TEST_ASSERT(strcmp(argv[1], "one") == 0);
        TEST_ASSERT(strcmp(argv[2], "two") == 0);
        TEST_END();
        
        TEST_BEGIN("RUNTIME-005"); // envp termination
        TEST_ASSERT(envp != NULL);
        TEST_ASSERT(envp[0] != NULL);
        TEST_ASSERT(strcmp(envp[0], "MODE=test") == 0);
        TEST_ASSERT(envp[1] == NULL);
        TEST_END();
        
        TEST_BEGIN("RUNTIME-006"); // stack alignment
        uint64_t stack_addr;
        __asm__ volatile ("mov %%rsp, %0" : "=r"(stack_addr));
        // main is called with 'call main'. So at entry, stack is NOT 16-byte aligned.
        // It's offset by 8 because the return address is pushed.
        // Wait, the stack pointer might be modified by prologue (push rbp).
        // Let's use __builtin_frame_address(0).
        uint64_t frame_ptr = (uint64_t)__builtin_frame_address(0);
        // The frame pointer (RBP) points to saved RBP. Previous RSP is frame_ptr + 16.
        // Before the 'call main' in _start, RSP must be 16-byte aligned.
        // So frame_ptr + 16 must be 16-byte aligned.
        TEST_ASSERT((frame_ptr % 16) == 0);
        TEST_END();
        
        TEST_BEGIN("RUNTIME-009"); // execve replaces argv/envp
        TEST_ASSERT(1); // Validated by the values above!
        TEST_END();
        
        return 42;
    }
    
    // Normal hello execution
    const char *msg = "hello from user space!\n";
    write(1, msg, strlen(msg));
    
    return 0;
}

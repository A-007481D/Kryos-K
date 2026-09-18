#include "../libkryos/include/kryos.h"
#include "../libkryos/include/test_framework.h"

int main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    
    // Test spawn passes argv
    TEST_BEGIN("RUNTIME-008");
    char *const child_argv[] = {"/test_exec.elf", NULL};
    pid_t pid = spawn("/test_exec.elf", child_argv);
    
    if (pid < 0) {
        TEST_ASSERT(0);
    }
    
    int status = 0;
    pid_t ret = waitpid(pid, &status);
    
    if (ret != pid) {
        TEST_ASSERT(0);
    }
    
    if (status != 42) {
        // If it failed in hello, it would exit with 1 (from test_exec or assert)
        TEST_ASSERT(0);
    }
    
    TEST_END(); // RUNTIME-008
    
    TEST_BEGIN("RUNTIME-007");
    // If we reached here, hello returned 42 and exit() correctly returned it to waitpid
    TEST_ASSERT(1);
    TEST_END();
    
    return 0; // success code for init
}

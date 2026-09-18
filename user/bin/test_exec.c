#include "../libkryos/include/kryos.h"
#include "../libkryos/include/test_framework.h"

int main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    
    // Launch hello with specific args
    char *const new_argv[] = {"/hello.elf", "one", "two", NULL};
    char *const new_envp[] = {"MODE=test", NULL};
    
    execve("/hello.elf", new_argv, new_envp);
    
    // execve only returns on failure
    TEST_ASSERT(0);
    return 1;
}

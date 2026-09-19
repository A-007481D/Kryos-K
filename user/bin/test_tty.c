#include "../libkryos/include/kryos.h"
#include <stddef.h>

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

void main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;

    const char *prompt = "Type something:\n";
    write(1, prompt, strlen(prompt));
    
    char buf[128];
    int n = read(0, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        const char *resp = "You typed: ";
        write(1, resp, strlen(resp));
        write(1, buf, n);
    }
    
    exit(0);
}

#include "../libkryos/include/kryos.h"

static void print(const char *str) {
    write(1, str, strlen(str));
}

#define MAX_ARGS 16

static void parse_line(char *line, char **argv) {
    int argc = 0;
    char *p = line;
    
    while (*p && argc < MAX_ARGS - 1) {
        // Skip whitespace
        while (*p == ' ' || *p == '\t' || *p == '\n') {
            *p = '\0';
            p++;
        }
        
        if (*p == '\0') break;
        
        argv[argc++] = p;
        
        // Skip until next whitespace
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') {
            p++;
        }
    }
    
    argv[argc] = NULL;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    char line[256];
    char *cmd_argv[MAX_ARGS];
    
    while (1) {
        print("Kryos $ ");
        
        ssize_t n = read(0, line, sizeof(line) - 1);
        if (n < 0) {
            print("read error\n");
            break;
        }
        
        line[n] = '\0';
        
        parse_line(line, cmd_argv);
        
        if (cmd_argv[0] == NULL) {
            continue;
        }
        
        if (strcmp(cmd_argv[0], "exit") == 0) {
            break;
        }
        
        if (strcmp(cmd_argv[0], "echo") == 0) {
            for (int i = 1; cmd_argv[i] != NULL; i++) {
                print(cmd_argv[i]);
                if (cmd_argv[i+1] != NULL) print(" ");
            }
            print("\n");
            continue;
        }
        
        // The VFS layer will handle any missing leading '/'
        pid_t pid = spawn(cmd_argv[0], cmd_argv);
        if (pid < 0) {
            print("spawn: command not found\n");
        } else {
            int status = 0;
            waitpid(pid, &status);
        }
    }
    
    return 0;
}

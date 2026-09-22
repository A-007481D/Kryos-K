#include "../libkryos/include/kryos.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        write(1, "Usage: mkdir <path>\n", 20);
        return 1;
    }
    
    int err = mkdir(argv[1], 0755);
    if (err < 0) {
        write(1, "mkdir: failed to create directory\n", 34);
        return 1;
    }
    
    return 0;
}

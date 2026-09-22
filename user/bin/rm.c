#include "../libkryos/include/kryos.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        write(1, "Usage: rm <path>\n", 17);
        return 1;
    }
    
    int err = unlink(argv[1]);
    if (err == -EISDIR) {
        err = rmdir(argv[1]);
    }
    
    if (err < 0) {
        write(1, "rm: failed to remove\n", 21);
        return 1;
    }
    
    return 0;
}

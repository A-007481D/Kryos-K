#include "../libkryos/include/kryos.h"

static void print(const char *str) {
    write(1, str, strlen(str));
}

int main(int argc, char *argv[]) {
    const char *target = (argc > 1) ? argv[1] : "/";
    
    int fd = open(target, 0);
    if (fd < 0) {
        print("ls: failed to open directory\n");
        return 1;
    }
    
    struct dirent dent;
    while (1) {
        int err = getdents(fd, &dent, sizeof(dent));
        if (err < 0) {
            print("ls: getdents error\n");
            close(fd);
            return 1;
        }
        if (err == 0) {
            break; // EOF
        }
        
        if (dent.type == DT_DIR) {
            print("[DIR]  ");
        } else {
            print("[FILE] ");
        }
        print(dent.name);
        print("\n");
    }
    
    close(fd);
    return 0;
}

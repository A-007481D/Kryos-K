#include "../libkryos/include/kryos.h"

static void print(const char *str) {
    write(1, str, strlen(str));
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print("Usage: cat <filename>\n");
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        print("cat: cannot open file: ");
        print(argv[1]);
        print("\n");
        return 1;
    }

    char buf[256];
    while (1) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            print("cat: read error\n");
            close(fd);
            return 1;
        }
        if (n == 0) {
            break; // EOF
        }
        write(1, buf, (size_t)n);
    }

    close(fd);
    return 0;
}

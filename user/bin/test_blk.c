#include "../libkryos/include/kryos.h"
#include "../libkryos/include/test_framework.h"

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;

    _test_print("Running Phase 20 block device tests...\n");
    TEST_BEGIN("BLK-001");

    int fd = open("/dev/hda", 0);
    if (fd < 0) {
        _test_print("Failed to open /dev/hda\n");
        return 1;
    }

    const char *sig = "KRYOS_PERSISTENT_DATA_PHASE_20";
    size_t len = strlen(sig);

    // Write signature
    ssize_t written = write(fd, sig, len);
    if (written != (ssize_t)len) {
        _test_print("Failed to write to /dev/hda\n");
        return 1;
    }

    close(fd);

    // Reopen and read
    fd = open("/dev/hda", 0);
    if (fd < 0) {
        _test_print("Failed to reopen /dev/hda\n");
        return 1;
    }

    char buf[64] = {0};
    ssize_t r = read(fd, buf, len);
    if (r != (ssize_t)len) {
        _test_print("Failed to read from /dev/hda\n");
        return 1;
    }

    close(fd);

    int mismatch = 0;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != sig[i]) {
            mismatch = 1;
            break;
        }
    }

    if (mismatch) {
        _test_print("Data mismatch!\n");
        return 1;
    }

    TEST_END();
    _test_print("test_blk.elf passed all tests.\n");
    return 0;
}

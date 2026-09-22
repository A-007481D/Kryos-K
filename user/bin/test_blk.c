#include "../libkryos/include/kryos.h"
#include "../libkryos/include/test_framework.h"

int _memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while(n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

void _memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    while(n--) *d++ = *s++;
}

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

    // 1. Single-sector write
    const char *sig = "KRYOS_PERSISTENT_DATA_PHASE_20";
    size_t len = strlen(sig);
    ssize_t written = write(fd, sig, len);
    if (written != (ssize_t)len) {
        _test_print("Failed to write to /dev/hda\n");
        return 1;
    }

    // 2. Boundary sector write (offset 510, 4 bytes, crosses sector 0 and 1)
    char padding[480] = {0};
    for(int i=0; i<480; i++) padding[i] = 'A';
    written = write(fd, padding, 480);
    if (written != 480) { _test_print("Padding write failed\n"); return 1; }
    
    // Now at offset 510. Write 4 bytes: "BNDY"
    written = write(fd, "BNDY", 4);
    if (written != 4) { _test_print("Boundary write failed\n"); return 1; }

    // 3. Multi-sector write (1500 bytes starting at offset 514)
    char multi[1500];
    for(int i=0; i<1500; i++) multi[i] = 'M';
    _memcpy(multi + 500, "MULTI_SECTOR_TEST", 17);
    written = write(fd, multi, 1500);
    if (written != 1500) { _test_print("Multi-sector write failed\n"); return 1; }

    close(fd);

    // 4. Write/read after reopen
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

    int mismatch = 0;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != sig[i]) mismatch = 1;
    }
    if (mismatch) {
        _test_print("Single-sector data mismatch!\n");
        return 1;
    }

    char dump[480];
    r = read(fd, dump, 480);
    if (r != 480) { _test_print("Failed to read padding\n"); return 1; }

    char bndy[5] = {0};
    r = read(fd, bndy, 4);
    if (r != 4 || _memcmp(bndy, "BNDY", 4) != 0) {
        _test_print("Boundary read mismatch!\n");
        return 1;
    }

    char multi_read[1500];
    r = read(fd, multi_read, 1500);
    if (r != 1500 || _memcmp(multi_read + 500, "MULTI_SECTOR_TEST", 17) != 0) {
        _test_print("Multi-sector read mismatch!\n");
        return 1;
    }

    close(fd);

    TEST_END();
    _test_print("test_blk.elf passed all tests.\n");
    return 0;
}

#include <stdint.h>
#include "serial.h"
#include "idt.h"
#include "idt.h"
#include "multiboot.h"
#include "../interrupts/pit.h"
#include "../memory/vmm.h"
#include "../memory/layout.h"
#include "../../include/heap.h"
#include "../../include/thread.h"
#include "../../include/process.h"
#include "../../include/syscall.h"
#include "../../include/vfs.h"
#include "../../include/tarfs.h"
#include "../../include/elf.h"
#include "../../include/assert.h"
#include "../../include/blk.h"
#include "../fs/tty.h"
#include "../drivers/ps2.h"
#include "tests.h"

void kernel_main(uint32_t magic, uint32_t info_addr, uint64_t pml4_phys) {
    uint64_t* pml4 = (uint64_t*)(pml4_phys + 0xFFFFFFFF80000000ULL);
    pml4[0] = 0;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
    
    serial_init();
    multiboot_parse(magic, info_addr);
    vmm_init(pml4_phys);
    kheap_init(KERNEL_HEAP_BASE);
    process_init();
    thread_init();

    idt_init();
    
    // PS/2 Keyboard Init
    ps2_init();
    
    // TTY Init
    tty_init();

    pit_init();
    syscall_init();

    serial_puts("[PASS] boot\n");
    serial_puts("[PASS] long_mode\n");
    serial_puts("[PASS] higher_half\n");
    
    vfs_init();
    
    // Block Device & ATA Init
    extern void blk_init(void);
    extern void ata_init(void);
    blk_init();
    ata_init();
    
    // Find ATA disk "hda" and mount KFS
    struct blk_dev *hda = blk_get_dev("hda");
    if (!hda) {
        panic(__FILE__, __LINE__, "No hda block device found!");
    }

    extern int kfs_mount(struct blk_dev *dev);
    int err = kfs_mount(hda);
    if (err < 0) {
        panic(__FILE__, __LINE__, "Failed to mount KFS: %d", err);
    }
    serial_puts("[PASS] kfs_mount\n");
    
    // Run diagnostic tests
    run_kernel_tests();
    
    // Load /init.elf
    struct file *f = NULL;
    err = vfs_open("/init.elf", 0, &f);
    if (err < 0) {
        panic(__FILE__, __LINE__, "Failed to open /init.elf: %d", err);
    }
    
    // Read the file into a temporary buffer
    uint64_t max_elf_size = 65536; // 64KB is plenty for init.elf
    void *elf_buf = kmalloc(max_elf_size);
    if (!elf_buf) {
        panic(__FILE__, __LINE__, "OOM allocating ELF buffer");
    }
    
    size_t bytes_read = 0;
    err = vfs_read(f, elf_buf, max_elf_size, &bytes_read);
    if (err < 0 || bytes_read == 0) {
        panic(__FILE__, __LINE__, "Failed to read /init.elf");
    }
    uint64_t elf_size = bytes_read;
    
    vfs_close(f);
    
    struct process *init_proc = process_create();
    if (!init_proc) {
        panic(__FILE__, __LINE__, "OOM allocating init proc");
    }
    
    uint64_t init_entry = 0;
    uint64_t init_rsp = 0;
    
    const char *argv[] = {"/init.elf"};
    
    err = elf_load_image(&init_proc->as, elf_buf, elf_size, &init_entry, &init_rsp, 1, argv, 0, NULL);
    if (err != 0) {
        panic(__FILE__, __LINE__, "Failed to parse ELF: %d", err);
    }
    kfree(elf_buf);
    
    struct thread *init_thread = thread_create_user(init_proc, init_entry, init_rsp);
    if (!init_thread) {
        panic(__FILE__, __LINE__, "Failed to create init thread");
    }
    
    serial_puts("[INFO] Handing off to user-space /init.elf...\n");
    
    for (;;) {
        schedule();
    }
}

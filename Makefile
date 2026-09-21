CC = gcc
LD = ld
ASM = nasm

CFLAGS = -m64 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -nostartfiles -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel -Wall -Wextra -Werror -O2 -g -Iinclude
LDFLAGS = -m elf_x86_64 -T linker.ld -nostdlib
ASMFLAGS = -f elf64

BUILD_DIR = build
ISO_DIR = $(BUILD_DIR)/isodir

KERNEL_ELF = $(BUILD_DIR)/kryos.elf
KERNEL_ISO = $(BUILD_DIR)/kryos.iso
DISK_IMG = $(BUILD_DIR)/disk.img

OBJS = \
    build/kernel/process/process.o \
    build/kernel/process/elf.o \
    $(BUILD_DIR)/arch/x86_64/boot/boot.o \
    $(BUILD_DIR)/arch/x86_64/boot/long_mode_start.o \
    $(BUILD_DIR)/arch/x86_64/interrupts/isr.o \
    $(BUILD_DIR)/kernel/init/main.o \
    $(BUILD_DIR)/kernel/init/multiboot.o \
    $(BUILD_DIR)/kernel/memory/pmm.o \
    $(BUILD_DIR)/kernel/memory/vmm.o \
    $(BUILD_DIR)/kernel/memory/heap.o \
    $(BUILD_DIR)/kernel/thread/thread.o \
    $(BUILD_DIR)/kernel/tests/test_user.o \
    build/kernel/tests/test_process.o \
    $(BUILD_DIR)/kernel/thread/usermode.o \
    $(BUILD_DIR)/arch/x86_64/thread/switch.o \
    $(BUILD_DIR)/drivers/serial/serial.o \
    $(BUILD_DIR)/kernel/lib/string.o \
    $(BUILD_DIR)/kernel/lib/stdio.o \
    $(BUILD_DIR)/kernel/lib/assert.o \
    $(BUILD_DIR)/kernel/interrupts/gdt.o \
    $(BUILD_DIR)/kernel/interrupts/idt.o \
    $(BUILD_DIR)/kernel/interrupts/fault.o \
    $(BUILD_DIR)/kernel/interrupts/pic.o \
    $(BUILD_DIR)/kernel/interrupts/pit.o \
    $(BUILD_DIR)/kernel/interrupts/irq.o \
    $(BUILD_DIR)/kernel/drivers/ps2.o \
    $(BUILD_DIR)/kernel/drivers/vga.o \
    $(BUILD_DIR)/kernel/drivers/ata.o \
    $(BUILD_DIR)/kernel/fs/vfs.o \
    $(BUILD_DIR)/kernel/fs/tarfs.o \
    $(BUILD_DIR)/kernel/fs/vt.o \
    $(BUILD_DIR)/kernel/fs/tty.o \
    $(BUILD_DIR)/kernel/fs/blk.o \
    $(BUILD_DIR)/kernel/syscall/syscall.o \
    $(BUILD_DIR)/arch/x86_64/syscall/syscall_entry.o \
    $(BUILD_DIR)/kernel/tests/test_elf.o \
    $(BUILD_DIR)/kernel/tests/test_syscall.o \
    $(BUILD_DIR)/kernel/tests/test_process_hierarchy.o \
    $(BUILD_DIR)/kernel/tests/test_tarfs.o \
    $(BUILD_DIR)/kernel/tests/test_vfs.o \
    $(BUILD_DIR)/kernel/tests/test_syscall_fs.o \
    $(BUILD_DIR)/kernel/tests/test_framework.o \
    $(BUILD_DIR)/kernel/tests/test_main.o

.PHONY: all clean iso run debug test

USER_INIT = $(BUILD_DIR)/user/bin/init.elf
USER_HELLO = $(BUILD_DIR)/user/bin/hello.elf
USER_TEST_EXEC = $(BUILD_DIR)/user/bin/test_exec.elf
USER_TEST_ALLOC = $(BUILD_DIR)/user/bin/test_alloc.elf
USER_TEST_TTY = $(BUILD_DIR)/user/bin/test_tty.elf
USER_LS = $(BUILD_DIR)/user/bin/ls.elf
USER_SHELL = $(BUILD_DIR)/user/bin/shell.elf
USER_TEST_BLK = $(BUILD_DIR)/user/bin/test_blk.elf

INITRD = $(BUILD_DIR)/initrd.tar

USER_BINARIES = $(USER_INIT) $(USER_HELLO) $(USER_TEST_EXEC) $(USER_TEST_ALLOC) $(USER_TEST_TTY) $(USER_LS) $(USER_SHELL) $(USER_TEST_BLK)

all: $(KERNEL_ELF) $(USER_BINARIES) $(INITRD) $(DISK_IMG)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

USER_LIBKRYOS_OBJS = \
    $(BUILD_DIR)/user/libkryos/startup.o \
    $(BUILD_DIR)/user/libkryos/syscall.o \
    $(BUILD_DIR)/user/libkryos/process.o \
    $(BUILD_DIR)/user/libkryos/io.o \
    $(BUILD_DIR)/user/libkryos/string.o \
    $(BUILD_DIR)/user/libkryos/alloc.o

$(BUILD_DIR)/user/libkryos.a: $(USER_LIBKRYOS_OBJS)
	@mkdir -p $(dir $@)
	ar rcs $@ $(USER_LIBKRYOS_OBJS)

$(BUILD_DIR)/user/bin/%.elf: user/bin/%.c $(BUILD_DIR)/user/libkryos.a
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -ffreestanding -nostdlib -fno-pic -fno-pie -no-pie -Wl,--build-id=none -Wl,-Ttext=0x400000 $(BUILD_DIR)/user/libkryos/startup.o $< -L$(BUILD_DIR)/user -lkryos -o $@

$(INITRD): $(USER_BINARIES)
	tar -cf $@ -C $(BUILD_DIR)/user/bin init.elf hello.elf test_exec.elf test_alloc.elf test_tty.elf ls.elf shell.elf test_blk.elf

$(DISK_IMG):
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$@ bs=1M count=16

iso: $(KERNEL_ISO)

$(KERNEL_ISO): $(KERNEL_ELF) $(INITRD) boot/grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	@mkdir -p $(ISO_DIR)/boot/modules
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/kryos.elf
	cp $(INITRD) $(ISO_DIR)/boot/modules/initrd.tar
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(KERNEL_ISO) $(ISO_DIR)

run: iso $(DISK_IMG)
	qemu-system-x86_64 -cdrom $(KERNEL_ISO) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -display none

debug: iso $(DISK_IMG)
	qemu-system-x86_64 -cdrom $(KERNEL_ISO) -drive file=$(DISK_IMG),format=raw,index=0,media=disk -serial stdio -display none -s -S

clean:
	rm -rf $(BUILD_DIR)

test: $(DISK_IMG)
	python3 scripts/test_runner.py

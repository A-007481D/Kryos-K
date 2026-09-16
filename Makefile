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
    $(BUILD_DIR)/kernel/fs/vfs.o \
    $(BUILD_DIR)/kernel/fs/tarfs.o \
    $(BUILD_DIR)/kernel/fs/console.o \
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

USER_INIT = $(BUILD_DIR)/user/init.elf
USER_TEST_TARGET = $(BUILD_DIR)/user/test_target.elf
USER_TEST_SPAWN = $(BUILD_DIR)/user/test_spawn.elf
USER_TEST_EXEC = $(BUILD_DIR)/user/test_exec.elf
USER_TEST_ROLLBACK = $(BUILD_DIR)/user/test_rollback.elf

INITRD = $(BUILD_DIR)/initrd.tar

USER_BINARIES = $(USER_INIT) $(USER_TEST_TARGET) $(USER_TEST_SPAWN) $(USER_TEST_EXEC) $(USER_TEST_ROLLBACK)

all: $(KERNEL_ELF) $(USER_BINARIES) $(INITRD)

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

$(BUILD_DIR)/user/%.elf: user/%.c user/syscall.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -ffreestanding -nostdlib -fno-pic -fno-pie -no-pie -Wl,-Ttext=0x400000 $< -o $@

$(INITRD): $(USER_BINARIES)
	tar -cf $@ -C $(BUILD_DIR)/user init.elf test_target.elf test_spawn.elf test_exec.elf test_rollback.elf

iso: $(KERNEL_ISO)

$(KERNEL_ISO): $(KERNEL_ELF) $(INITRD) boot/grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	@mkdir -p $(ISO_DIR)/boot/modules
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/kryos.elf
	cp $(INITRD) $(ISO_DIR)/boot/modules/initrd.tar
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(KERNEL_ISO) $(ISO_DIR)

run: iso
	qemu-system-x86_64 -cdrom $(KERNEL_ISO) -serial stdio -display none

debug: iso
	qemu-system-x86_64 -cdrom $(KERNEL_ISO) -serial stdio -display none -s -S

clean:
	rm -rf $(BUILD_DIR)

test:
	python3 scripts/test_runner.py

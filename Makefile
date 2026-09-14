CC = gcc
LD = ld
ASM = nasm

CFLAGS = -m64 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -nostartfiles -mno-red-zone -mcmodel=kernel -Wall -Wextra -Werror -O2 -g -Iinclude
LDFLAGS = -m elf_x86_64 -T linker.ld -nostdlib
ASMFLAGS = -f elf64

BUILD_DIR = build
ISO_DIR = $(BUILD_DIR)/isodir

KERNEL_ELF = $(BUILD_DIR)/kryos.elf
KERNEL_ISO = $(BUILD_DIR)/kryos.iso

OBJS = \
    $(BUILD_DIR)/arch/x86_64/boot/boot.o \
    $(BUILD_DIR)/arch/x86_64/boot/long_mode_start.o \
    $(BUILD_DIR)/arch/x86_64/interrupts/isr.o \
    $(BUILD_DIR)/kernel/init/main.o \
    $(BUILD_DIR)/drivers/serial/serial.o \
    $(BUILD_DIR)/kernel/lib/stdio.o \
    $(BUILD_DIR)/kernel/lib/assert.o \
    $(BUILD_DIR)/kernel/interrupts/idt.o \
    $(BUILD_DIR)/kernel/interrupts/fault.o \
    $(BUILD_DIR)/kernel/tests/test_main.o

.PHONY: all clean iso run debug

all: $(KERNEL_ELF)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS) $< -o $@

$(KERNEL_ELF): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

iso: $(KERNEL_ISO)

$(KERNEL_ISO): $(KERNEL_ELF) boot/grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/kryos.elf
	cp boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(KERNEL_ISO) $(ISO_DIR)

run: iso
	qemu-system-x86_64 -cdrom $(KERNEL_ISO) -serial stdio -display none

debug: iso
	qemu-system-x86_64 -cdrom $(KERNEL_ISO) -serial stdio -display none -s -S

clean:
	rm -rf $(BUILD_DIR)

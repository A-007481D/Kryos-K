# Kryos

Kryos is a small x86_64 operating system built from scratch.

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![x86_64 Assembly](https://img.shields.io/badge/x86_64_Assembly-000000?style=for-the-badge)
![QEMU](https://img.shields.io/badge/QEMU-FF6600?style=for-the-badge)
![GRUB](https://img.shields.io/badge/GRUB-333333?style=for-the-badge&logo=gnu&logoColor=white)

Learning operating systems by implementing the machine-level abstractions normally hidden by modern operating systems.

## Building and Testing

Dependencies: `nasm`, `qemu-system-x86`, `grub`, `libisoburn`, `mtools`.

To build the ISO:
```sh
make iso
```

To run the kernel in QEMU:
```sh
make run
```

To execute the automated test suite:
```sh
make test
```

## Current Status (Phase 22 — Dynamic File Creation, Writes & Utilities)

Kryos has implemented the foundational layers of a modern Unix-like operating system:

- **Bootstrap & Core**: Multiboot2 boot transition, 64-bit Long Mode with 4-level paging, kernel executing at higher-half (`0xFFFFFFFF80000000`), identity mapping unmapped.
- **Memory Management**: Multiboot2-aware Physical Memory Manager (PMM) with exact frame accounting, Virtual Memory Manager (VMM) with per-process address space isolation, and kernel heap allocator (`kmalloc`/`kfree`).
- **Scheduling & Concurrency**: Cooperative and preemptive kernel threads, PIT IRQ0 timer-driven round-robin scheduler, callee/caller register preservation.
- **Privilege Separation & Syscalls**: Ring 3 user mode transition, hardware-enforced isolation, fast `syscall`/`sysret` dispatch (LSTAR) preserving user contexts.
- **Process Subsystem**: Hierarchical process management (`fork`/`spawn`, transactional `execve`, `waitpid`, `exit`), ELF loading and validation, userspace memory management (`sys_brk`).
- **Userspace Runtime**: Minimal standard library (`libkryos`), SysV ABI string arrays, dynamic memory allocation (`malloc`/`free`).
- **TTY & Virtual Terminals**: VT switching, PS/2 keyboard driver with interrupt scancode decoding and modifier tracking, non-blocking/blocking TTY line discipline.
- **Userspace Shell & Commands**: Interactive shell (`shell.elf`) and core commands (`ls`, `cat`, `echo`).
- **Block Device Subsystem**: IDE/ATA PIO controller driver supporting single- and multi-sector reads and writes with hardware persistence across reboot cycles.
- **Kryos File System (KFS)**: Full on-disk read/write Unix-style filesystem:
  - Superblock, inode bitmap, block allocation bitmap, and inode table.
  - Runtime dynamic inode and data block allocation/freeing with immediate disk persistence.
  - File creation (`O_CREAT`), truncation (`O_TRUNC`), append mode (`O_APPEND`), and multi-sector writes spanning direct (0–9) and indirect block tables (up to 70.6 KiB).
  - Directory entry creation and directory traversal (`getdents`).
  - Partition layout reserving boot sectors 0–63 for bootloaders and raw persistence tests.
  - Offline image builder (`mkfs_kfs.py`) and static filesystem integrity checker (`kfsck.py`).
- **Verification & Testing**: Rigorous, staff-level automated test suite executing 120+ hardware, kernel, and userspace invariant assertions under QEMU.


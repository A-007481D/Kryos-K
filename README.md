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

## Current Status (Phase 4)
- 32-bit to 64-bit long mode transition
- Initial 4-level page tables with 1 GiB mapping using 2 MiB huge pages
- Kernel executing at higher-half virtual address (`0xFFFFFFFF80000000`)
- Low memory identity mapping strictly unmapped
- Exception handling and IDT
- Deterministic python test runner with exception recovery and failure classification
- Multiboot2-aware Physical Memory Manager (PMM) with exact frame ownership and accounting

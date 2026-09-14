# Kryos

Kryos is a small x86_64 operating system built from scratch.

C · x86_64 Assembly · QEMU · GRUB

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

## Current Status (Phase 3)
- 32-bit to 64-bit long mode transition
- Initial 4-level page tables with 1 GiB mapping using 2 MiB huge pages
- Kernel executing at higher-half virtual address (`0xFFFFFFFF80000000`)
- Low memory identity mapping strictly unmapped
- Exception handling and IDT
- Deterministic python test runner with exception recovery and failure classification

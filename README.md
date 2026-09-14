# Kryos

Kryos is a small x86_64 operating system built from scratch.

C · x86_64 Assembly · QEMU · GRUB

Learning operating systems by implementing the machine-level abstractions normally hidden by modern operating systems.

## Building and Running

Dependencies: `nasm`, `qemu-system-x86`, `grub`, `libisoburn`, `mtools`.

```bash
make iso
make run
```

## Current Status (Phase 1)
- 32-bit to 64-bit long mode transition
- CPUID long-mode detection
- Initial 4-level page tables (identity mapped)
- True `x86_64` C execution environment

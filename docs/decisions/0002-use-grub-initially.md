# ADR 0002: Use GRUB initially

## Context
We need a mechanism to boot the kernel in our test environment (QEMU). Writing a custom UEFI bootloader is a major project in itself.

## Decision
Use **GRUB** as the initial bootloader and adhere to the **Multiboot2** specification for kernel handoff.

## Alternatives
- Write a custom UEFI bootloader immediately.
- Use Limine or BOOTBOOT.

## Tradeoffs
- GRUB hides the extreme low-level details of UEFI and disk reading.
- We must conform to the Multiboot2 header specification and entry state.

## Consequences
- Phase 0 entry code must be a 32-bit Multiboot2 entry path.
- A custom Kryos bootloader is deferred to Phase 20, keeping early momentum focused on kernel fundamentals.

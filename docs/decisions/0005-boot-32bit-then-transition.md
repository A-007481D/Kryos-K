# ADR 0005: Boot 32-bit, Then Transition

## Context
Multiboot2 (and GRUB) hands control to the kernel in 32-bit protected mode on x86, not 64-bit long mode.

## Decision
Phase 0 will execute in **32-bit protected mode** as a bootstrap phase. We will transition to **x86_64 long mode** in Phase 1.

## Alternatives
- Transition to long mode immediately in the boot assembly before calling C code.

## Tradeoffs
- Setting up long mode requires configuring page tables, a 64-bit GDT, and enabling specific CPU flags (PAE, EFER.LME, CR0.PG).
- Doing this immediately in Phase 0 bloats the initial verification step and makes debugging early toolchain/boot issues much harder.

## Consequences
- Phase 0 C code (`kernel_main`) is compiled as 32-bit (`-m32`).
- We can prove the Multiboot2 handoff, ELF layout, and serial I/O work perfectly before tackling the complex state transitions required for 64-bit mode.
- Phase 1 will replace or extend the bootstrap to perform the transition to an identity-mapped 64-bit environment.

# ADR 0004: Use System GCC Temporarily

## Context
We need a compiler toolchain to build the freestanding kernel. Setting up a dedicated cross-compiler toolchain (e.g., `x86_64-elf-gcc`) is standard practice to avoid host OS dependencies.

## Decision
Use the **system GCC** temporarily for the initial freestanding bootstrap because the development host and target ISA are both x86_64.

## Alternatives
- Build binutils and GCC for `x86_64-elf` immediately in Phase 0.

## Tradeoffs
- Using the system compiler risks accidental inclusion of host OS assumptions or libraries if flags aren't meticulously managed.
- It simplifies Phase 0 setup dramatically.

## Consequences
- We must strictly use `-ffreestanding`, `-nostdlib`, `-nostartfiles`, `-fno-pie`, and `-fno-stack-protector` to strip away Linux host dependencies.
- A dedicated `x86_64-elf` cross-toolchain will be introduced when the kernel ABI/toolchain requirements become more demanding.

# ADR 0003: Kernel in C

## Context
We must select a programming language for the kernel proper.

## Decision
Write the kernel primarily in **freestanding C**, with low-level architecture-specific code in **x86_64 Assembly** (NASM syntax).

## Alternatives
- Rust: Excellent safety guarantees, but hides some pointer/memory mechanics that are educational to implement manually first. (Rust is planned for the hypervisor later).
- C++: Introduces complex runtime requirements (RTTI, exceptions) that are difficult to support in a bare-metal kernel.

## Tradeoffs
- C requires manual memory management and offers no safety nets against buffer overflows or use-after-free errors.
- We must be highly disciplined with pointers and invariants.

## Consequences
- We will use `-ffreestanding` to detach from libc.
- We must write our own memory manipulation functions (`memcpy`, `memset`, etc.) eventually, or rely on compiler builtins if absolutely necessary.

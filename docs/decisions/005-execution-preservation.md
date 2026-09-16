# ADR 005: Execution Preservation Mechanisms

## Status
Accepted

## Context
In Phase 8, Kryos introduced asynchronous preemption (timer interrupts) alongside the existing cooperative threading model (`thread_yield()`). This creates two distinct scenarios where execution context must be preserved and restored.

## Decision
We explicitly define two different preservation mechanisms, which cleanly interoperate under the "nested context switch" model:

1. **Cooperative Switch**
   - **Mechanism:** `context_switch(old, new)`
   - **State Preserved:** Only the SysV AMD64 callee-saved GPRs (`RBP`, `RBX`, `R12-R15`) and the stack pointer (`RSP`).
   - **Reasoning:** In a cooperative yield (an explicit C function call), the caller automatically preserves caller-saved registers and expects callee-saved registers to be preserved. `RIP` is preserved implicitly via the return address on the stack. No hardware context (like `RFLAGS` or `CR3`) needs to be manually preserved yet.

2. **Asynchronous Interrupt (Preemption)**
   - **Mechanism:** Hardware interrupt push + ISR wrapper (`isr_common`) + `context_switch`
   - **State Preserved:** The full interrupt-visible CPU state.
     - The CPU automatically pushes `RIP`, `CS`, and `RFLAGS` (and eventually `RSP`/`SS` in user-space).
     - The ISR stub pushes the remaining 15 general-purpose registers and an error code.
     - The scheduler then calls `context_switch()`, which effectively treats the interrupted state as just another stack frame, preserving the callee-saved registers on top of the interrupt frame.
   - **Reasoning:** An interrupt can occur at any arbitrary instruction, meaning the interrupted code cannot make any ABI assumptions. The full state must be saved. By layering the cooperative `context_switch` on top of the interrupt frame, we avoid needing a separate set of context-switching primitives.

## Consequences
- **Robustness:** We have demonstrated that a thread can be interrupted at any point, control handed to the kernel scheduler, switched to another thread, and later successfully resumed.
- **Simplicity:** The scheduler only ever deals with one `context_switch` function, whether the switch was voluntary or asynchronous.
- **Preparation for Phase 9:** This dual model sets the foundation for Phase 9 (User/Kernel Privilege Boundary). When transitioning to user-mode (Ring 3), the asynchronous mechanism will naturally scale to handle privilege level switches by capturing the full Ring 3 state pushed by the CPU (`RSP`, `SS`, `RIP`, `CS`, `RFLAGS`).

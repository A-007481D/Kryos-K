# Phase 4 PMM: Bug Report & Post-Mortem

> Documented: 2026-09-15  
> Phase: 4 — Physical Memory Manager  
> Status: **All bugs resolved. Tests passing.**

---

## Summary

During Phase 4 (PMM implementation), three critical bugs were discovered that
interacted in a cascading failure pattern. Each bug masked the others, making
diagnosis extremely difficult. The bugs spanned three different subsystems:
boot memory layout, Multiboot2 tag parsing, and the test recovery mechanism.

All three bugs were latent in the codebase — they only manifested when the
PMM was introduced because it was the first subsystem to write large amounts
of data to physical memory during boot.

## The Bug Cascade: A Success for Systems Methodology

The cascade of bugs exposed during this phase is a strong validation of the rigorous, invariant-based testing methodology. 

We found:
1. **Memory Layout Bug**: BIOS/MB2 layout assumption → memory corruption → parser corruption → infinite loop.
2. **Protocol Parsing Bug**: Physical-address-space assumption → 32 MiB bitmap → QEMU timeout.
3. **CPU/Context Semantics Bug**: Exception recovery assumption → RSP/register corruption → test harness corruption → false-looking exception.
4. **ABI Correctness Bug**: ABI violation → latent stack-alignment bug.

These are four different classes of systems bugs. Exposing them now, through deliberate testing and failure-mode analysis, prevents catastrophic, undebuggable failures later.

### Distinguishing Root Causes

It is important to distinguish between a **"Bug introduced by PMM"** and a **"Bug discovered because of PMM"**. 

For example, BUG-003 and BUG-004 were pre-existing flaws in the test recovery machinery and ISR stubs. The new PMM failure tests simply exercised the infrastructure deeply enough to expose them.

> **A new subsystem can reveal dormant bugs in supposedly unrelated infrastructure.**

---

## BUG-001: PMM Bitmap Overwrites Multiboot2 Information Structure

### Classification
**Severity:** Critical (data corruption, infinite loop)  
**Root Cause:** Incorrect bitmap placement assumption  
**Subsystem:** `kernel/memory/pmm.c`, `kernel/init/multiboot.c`

### Description

`pmm_init()` was originally called with a single argument (`max_phys_addr`)
and placed the bitmap immediately after `_kernel_end`:

```c
// BEFORE (broken)
void pmm_init(uint64_t max_phys_addr) {
    uint64_t kernel_end_phys = virt_to_phys(_kernel_end);
    bitmap_phys_addr = align_up(kernel_end_phys, PMM_PAGE_SIZE);
    ...
}
```

The problem: GRUB places the Multiboot2 information structure in physical
memory **after** the kernel image. In our case:

```
_kernel_end   → phys ~0x511000
MB2 info      → phys  0x517bf0  (size ~0x1800 bytes)
bitmap placed → phys  0x511000  ← OVERLAPS MB2!
```

When `pmm_init()` wrote `0xFF` to every byte of the bitmap (to mark all
frames as reserved), it destroyed the Multiboot2 tag structure in physical
memory. When `multiboot_parse()` then entered Pass 2 to iterate over the
tags, it read `0xFFFFFFFF` for both `tag->type` and `tag->size`, causing:

- `tag->type` never equaled `MULTIBOOT_TAG_TYPE_END` (value 0)
- `align_up_8(0xFFFFFFFF)` wrapped to 0, so `iter` never advanced
- Result: **infinite loop**

### Fix

`multiboot_parse()` now computes `highest_used`, which is the maximum of
`_kernel_end` and `info_addr_phys + total_size`, and passes it to
`pmm_init()` so the bitmap is placed safely above all boot structures:

```c
// AFTER (fixed)
uint64_t highest_used = kernel_end_phys;
if (boot_structures_end > highest_used) {
    highest_used = boot_structures_end;
}
pmm_init(highest_used, max_phys_addr);
```

### Lesson

Never assume `_kernel_end` is the last thing in physical memory. The
bootloader can place structures anywhere. The PMM must query the actual
layout before deciding where to put its own data.

---

## BUG-002: Unbounded Physical Address Space Causes QEMU Timeout

### Classification
**Severity:** High (test infrastructure failure, masked by BUG-001)  
**Root Cause:** Trusting raw BIOS memory map without capping to mapped range  
**Subsystem:** `kernel/init/multiboot.c`

### Description

QEMU (with the default `-m` setting of 128 MiB) reports memory map entries
via Multiboot2 that include regions far beyond physical RAM:

```
Entry: addr=0x0,        len=0x9FC00,      type=AVAILABLE
Entry: addr=0x100000,   len=0x7EE0000,    type=AVAILABLE
Entry: addr=0xFD000000, len=0x300000,     type=RESERVED   (MMIO)
Entry: addr=0x10000000000, len=...         type=RESERVED   (ACPI hotplug)
```

The code was computing `max_phys_addr` as the highest `addr + len` across
**all** entries, yielding `0x10000000000` (1 TiB). This meant:

- `total_frames = 1 TiB / 4 KiB = 268,435,456 frames`
- `bitmap_size = 33,554,432 bytes` (32 MiB)
- The `memset` loop (`bitmap[i] = 0xFF`) iterated 33 million times

Under QEMU's TCG (software CPU emulation without KVM), this took far longer
than the 10-second test timeout, causing every test run to fail with
`[FAIL] TIMEOUT`.

### Fix

Cap `max_phys_addr` to `0x40000000` (1 GiB), which matches the bootstrap
page table mapping range:

```c
if (end_addr > 0x40000000) {
    end_addr = 0x40000000;
}
```

This also includes a secondary optimization: `pmm_mark_region()` was changed
to use bitwise shifts instead of division/modulo:

```c
// BEFORE: uint64_t frame = addr / PMM_PAGE_SIZE;
// AFTER:
uint64_t frame = addr >> 12;
uint64_t byte  = frame >> 3;
uint8_t  bit   = frame & 7;
```

### Lesson

The BIOS memory map reports the **hardware address space**, not the
**usable mapped range**. The PMM must respect the current page table
coverage. When a proper VMM exists, this cap can be lifted.

---

## BUG-003: Exception Recovery Corrupts Stack and Callee-Saved Registers

### Classification
**Severity:** Critical (silent test infrastructure corruption)  
**Root Cause:** Incomplete CPU state restoration after intercepted panic  
**Subsystem:** `kernel/interrupts/fault.c`, `kernel/tests/test_main.c`

### Description

The `TEST_PANIC` macro intercepts `panic()` calls to verify that the PMM
correctly panics on invalid operations (double-free, unaligned free, etc.).
The mechanism works by:

1. Setting `current_test_context.active = true`
2. Calling the expression that should panic
3. `panic()` executes `ud2`, which triggers `#UD` (vector 6)
4. `fault_handler()` intercepts it and rewrites `frame->rip` to a recovery label
5. `iretq` returns to the recovery label

**The original code only restored `rip`:**

```c
// BEFORE (broken)
frame->rip = current_test_context->recovery_rip;
current_test_context->active = false;
```

This was catastrophically wrong. When `ud2` fires inside `panic()`, the CPU
has already pushed a new interrupt frame. The call chain is:

```
test_pmm_failures()
  → pmm_free_page()     [pushes stack frame]
    → panic()           [pushes stack frame]
      → kvprintf()      [pushes stack frame]
        → ud2           [CPU pushes interrupt frame]
          → fault_handler() rewrites rip only
        → iretq pops to recovery label...
           ...but RSP still points deep inside kvprintf's frame!
```

The result: execution continues at the recovery label, but RSP, RBP, RBX,
and R12-R15 hold values from inside `kvprintf()`. The next `TEST_PANIC`
invocation then operates on a corrupted stack, and the cascade produces
garbage like `"Expected exception 101, but got 6"` — because `101` was
whatever happened to be in the memory location the compiler thought held
`expected_vector`.

Additionally, the original design used a **pointer** to a stack-local
`exception_test_context_t`:

```c
// BEFORE (broken)
exception_test_context_t ctx = {0};
current_test_context = &ctx;
```

After `iretq` returned to the recovery label with a corrupted RSP, `&ctx`
pointed into the clobbered stack region, creating use-after-free behavior.

### Fix

Three-part fix implementing a kernel-space equivalent of `setjmp`/`longjmp`:

**1. Global struct instead of stack-local pointer:**

```c
// AFTER
volatile exception_test_context_t current_test_context = {0};
```

**2. Save ALL callee-saved registers at the recovery point:**

```c
#define SAVE_RECOVERY_STATE() __asm__ volatile( \
    "lea 1f(%%rip), %%rcx\n"  \
    "mov %%rcx, %0\n"         \
    "mov %%rsp, %1\n"         \
    "mov %%rbp, %2\n"         \
    "mov %%rbx, %3\n"         \
    "mov %%r12, %4\n"         \
    "mov %%r13, %5\n"         \
    "mov %%r14, %6\n"         \
    "mov %%r15, %7\n"         \
    : "=m"(current_test_context.recovery_rip),  \
      "=m"(current_test_context.recovery_rsp),  \
      "=m"(current_test_context.recovery_rbp),  \
      "=m"(current_test_context.recovery_rbx),  \
      "=m"(current_test_context.recovery_r12),  \
      "=m"(current_test_context.recovery_r13),  \
      "=m"(current_test_context.recovery_r14),  \
      "=m"(current_test_context.recovery_r15)   \
    : : "rcx", "memory" \
)
```

**3. Restore ALL registers in the fault handler:**

```c
frame->rip = current_test_context.recovery_rip;
frame->rsp = current_test_context.recovery_rsp;
frame->rbp = current_test_context.recovery_rbp;
frame->rbx = current_test_context.recovery_rbx;
frame->r12 = current_test_context.recovery_r12;
frame->r13 = current_test_context.recovery_r13;
frame->r14 = current_test_context.recovery_r14;
frame->r15 = current_test_context.recovery_r15;
```

When `iretq` executes, it restores `rip`, `rsp`, `rflags`, `cs`, and `ss`
from the interrupt frame. The ISR preamble pops the general-purpose registers
from the stack (which now contains our saved values). The CPU returns to the
exact machine state that existed before the faulting expression was called.

### Lesson

Exception recovery in a kernel is not "just rewrite RIP". It is a full
context switch. Any mechanism that intercepts deep call chains must save
and restore the complete set of callee-saved registers, or the caller's
invariants will be silently violated.

---

## BUG-004: ISR Stack Alignment Violation (System V ABI)

### Classification
**Severity:** Medium (latent UB, could cause SSE faults in future)  
**Root Cause:** Missing 16-byte stack alignment before `call`  
**Subsystem:** `arch/x86_64/interrupts/isr.asm`

### Description

The System V AMD64 ABI requires the stack to be 16-byte aligned at the point
of a `call` instruction. After pushing 15 general-purpose registers (120
bytes) plus the vector and error code (16 bytes), the stack alignment depends
on the initial CPU-pushed frame. The ISR was calling `fault_handler` without
ensuring alignment.

### Fix

Save `rsp` in `rbp`, align with `and rsp, -16`, call the handler, then
restore `rsp` from `rbp`:

```nasm
mov rbp, rsp
and rsp, -16
cld
call fault_handler
mov rsp, rbp
```

### Lesson

Even in freestanding kernel code, violating the ABI is undefined behavior.
Future code that uses SSE or calls compiler-generated functions will fault
on unaligned `movaps` instructions if this is not enforced.

---

## Cascade Interaction Diagram

```
BUG-002 (1 TiB address space)
    │
    ▼
  bitmap_size = 32 MiB → memset timeout → [FAIL] TIMEOUT
    │
    │ (after capping to 1 GiB)
    ▼
BUG-001 (bitmap overlaps MB2 info)
    │
    ▼
  memset(bitmap, 0xFF) destroys MB2 tags → infinite loop in Pass 2
    │
    │ (after fixing bitmap placement)
    ▼
BUG-003 (incomplete context restore)
    │
    ▼
  TEST_PANIC macro returns to recovery label with corrupted RSP
    │
    ▼
  Next TEST_PANIC reads garbage from corrupted stack
    │
    ▼
  "Expected exception 101, but got 6" → infinite panic/recovery loop
```

Each bug was only visible after fixing the previous one, creating a
three-layer debugging puzzle.

---

## Verification

All 11 tests pass deterministically:

```
[PASS] boot
[PASS] long_mode
[PASS] higher_half
[PASS] kassert
[PASS] divide_by_zero
[PASS] invalid_opcode
[PASS] page_fault
[PASS] pmm_basic
[PASS] pmm_randomization
[PASS] pmm_failures         (double-free, reserved-free, unaligned, OOR)
[PASS] pmm_exhaustion_oom   (full exhaust + reverse-free + accounting)
@@KRYOS:SUITE:PASS
@@KRYOS:TEST:panic:EXPECTED
KERNEL PANIC                (expected terminal event)
[SUCCESS] All tests passed!
```

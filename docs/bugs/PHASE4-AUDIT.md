# Phase 4 Audit Report: Remaining Issues & Observations

> Audited: 2026-09-15  
> Auditor: Post-fix independent verification  
> Scope: All uncommitted changes from Phase 4 PMM work

---

## Verdict

The previous agent's three claimed bug fixes are **verified correct**:

| Claim | Verified | Evidence |
|-------|----------|----------|
| BUG-001: Bitmap overlapped MB2 info | ✅ | `highest_used` computation in multiboot.c L63-72 |
| BUG-002: Unbounded max_phys_addr caused timeout | ✅ | Cap at 0x40000000 in multiboot.c L44-46 |
| BUG-003: Stack corruption in TEST_PANIC recovery | ✅ | SAVE_RECOVERY_STATE macro + full register restore in fault.c L43-50 |
| BUG-004: ISR stack misalignment | ✅ | `and rsp, -16` in isr.asm with rbp save/restore |

Tests pass deterministically on two consecutive runs (verified).

---

## Remaining Issues Found

### ISSUE-001: Debug Trace Statements Left in Production Code

**Severity:** Low (noise, not a bug)  
**Files:** `kernel/init/multiboot.c`

Line 85 still contains a per-tag debug print inside the hot path:

```c
kprintf("Pass 2: tag type=%d, size=%d\n", tag->type, tag->size);
```

Lines 73, 94 in `pmm.c` also have verbose per-call tracing:

```c
kprintf("pmm_mark_region: start=0x%lx, end=0x%lx, free=%d\n", ...);
kprintf("pmm_mark_region: done.\n");
```

**Recommendation:** These were debugging aids. Strip them before commit, or
gate behind a `#ifdef KRYOS_DEBUG` flag.

---

### ISSUE-002: Missing Newline After SAVE_RECOVERY_STATE Macro

**Severity:** Cosmetic (compiles fine, but misleading)  
**File:** `kernel/tests/test_main.c`, line 30

```c
    : : "rcx", "memory" \
)
static void test_kassert(void) {  // ← no blank line separator
```

The macro definition ends and the next function begins on the immediately
next line. This compiles correctly (the `)` terminates the macro, and
`static void...` starts a new declaration), but it looks like the function
might be part of the macro to a human reader.

**Recommendation:** Add a blank line after the closing `)`.

---

### ISSUE-003: `pmm.h` Comment is Stale

**Severity:** Cosmetic  
**File:** `include/pmm.h`, line 8-9

```c
// Initializes the PMM, placing the bitmap after the kernel and 
// marking all memory up to max_phys_addr as RESERVED.
void pmm_init(uint64_t bitmap_start_phys, uint64_t max_phys_addr);
```

The comment says "after the kernel" but the function now takes
`bitmap_start_phys` explicitly. The comment should say "after the specified
physical address".

---

### ISSUE-004: Test Runner Doesn't Check New PMM Tests

**Severity:** Medium (test regression gap)  
**File:** `scripts/test_runner.py`, lines 7-14

```python
EXPECTED_PASSES = [
    "[PASS] boot",
    "[PASS] long_mode",
    "[PASS] higher_half",
    "[PASS] kassert",
    "[PASS] divide_by_zero",
    "[PASS] invalid_opcode",
    "[PASS] page_fault"
]
```

The new PMM tests (`pmm_basic`, `pmm_randomization`, `pmm_failures`,
`pmm_exhaustion_oom`) are not in `EXPECTED_PASSES`. If they silently
disappeared from the output, the runner would still report `[SUCCESS]`.

**Recommendation:** Add all four PMM test markers to `EXPECTED_PASSES`.

---

### ISSUE-005: `_kernel_end` is a Virtual Address Declared as `extern uint64_t[]`

**Severity:** Low (works correctly, but confusing)  
**File:** `kernel/init/multiboot.c`, line 66

```c
extern uint64_t _kernel_end[];
uint64_t kernel_end_phys = virt_to_phys(_kernel_end);
```

`_kernel_end` is a linker symbol at a higher-half virtual address.
Declaring it as `uint64_t[]` and then passing the array name (which decays
to a pointer) to `virt_to_phys()` works, but the type is misleading — it
is not actually an array of uint64_t. A cleaner pattern:

```c
extern char _kernel_end[];
```

This is the conventional idiom for linker symbols.

---

### ISSUE-006: `pmm_mark_region` in Pass 2 Doesn't Cap Region Lengths

**Severity:** Medium (correctness concern)  
**File:** `kernel/init/multiboot.c`, lines 95-99

```c
if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
    pmm_mark_region(entry->addr, entry->len, true);
}
```

Pass 1 caps `max_phys_addr` to 1 GiB, but Pass 2 passes the raw
`entry->len` to `pmm_mark_region()`. If a BIOS reports an available
region like `[0x100000, 0x7FE0000)` (which it does), that's fine because
it's within 1 GiB. But if a region reported `[0x30000000, 0x50000000)`
(crossing the 1 GiB boundary), `pmm_mark_region` would be called with a
range extending to 1.25 GiB, and only the internal `end_addr` cap in
`pmm_mark_region()` prevents an out-of-bounds write.

The cap inside `pmm_mark_region()` does catch this (`if (end_addr >
max_physical_address) end_addr = max_physical_address`), so this is
**not a bug**, but it relies on a defensive check two call layers deep
rather than sanitizing input at the source.

---

### ISSUE-007: All Phase 4 Files Are Uncommitted

**Severity:** High (process/hygiene)

```
?? include/pmm.h          (untracked)
?? kernel/init/multiboot.c  (untracked)
?? kernel/init/multiboot.h  (untracked)
?? kernel/init/multiboot2.h (untracked)
?? kernel/memory/           (untracked)
```

Plus 11 modified tracked files. The entire Phase 4 implementation exists
only in the working tree. A single `git clean -fd` would destroy the PMM.

**Recommendation:** Commit this work immediately, in coherent pieces.

---

### ISSUE-008: `oom_pages` Static Array is 4 MiB

**Severity:** Low (acceptable for tests, but worth noting)  
**File:** `kernel/tests/test_main.c`, line 140

```c
static uint64_t oom_pages[524288];  // 4 MiB
```

This 4 MiB static array is linked into the kernel `.bss` section
permanently, even in non-test builds. If Kryos ever separates test code
from the kernel binary, this should move to a test-only section.

---

## Memory-Safety / Correctness Review

For a bare-metal kernel at this stage, we review for memory-management correctness rather than "security" in the usual OS sense (as there is no userspace or privilege separation yet). The relevant checks:

| Property | Status | Notes |
|----------|--------|-------|
| Bitmap can't overlap boot structures | ✅ Fixed | BUG-001 |
| Reserved regions can't be freed | ✅ | `pmm_free_page` checks `phys_addr < first_usable_phys` |
| Double-free detected | ✅ | Bitmap bit check before clear |
| Unaligned free detected | ✅ | Modulo check |
| Out-of-range free detected | ✅ | Bounds check |
| Frame 0 can't be allocated | ✅ | Reserved by `pmm_reserve_boot_regions` |
| Bitmap bounds respected | ✅ | `end_addr` capped to `max_physical_address` |
| No integer overflow in bitmap addressing | ✅ | 1 GiB cap limits frame count to 262144 |

**No critical memory-management correctness gaps found** for the current phase scope.

---

## Phase 4 Completion Rule

Before moving to Phase 5, the following invariant is established:

> **The PMM must be considered authoritative for physical-frame ownership.**
> No future subsystem may allocate, reserve, or free physical memory without going through the PMM, except for explicitly documented bootstrap reservations.

---

## Recommended Next Steps

1. Strip debug `kprintf` statements (ISSUE-001)
2. Add PMM tests to `EXPECTED_PASSES` in test runner (ISSUE-004)
3. Fix stale comment in `pmm.h` (ISSUE-003)
4. Commit all Phase 4 work (ISSUE-007)

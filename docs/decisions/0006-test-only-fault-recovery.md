# 6. Test-Only Fault Recovery

Date: 2026-09-16

## Status

Accepted

## Context

During Phase 9 (Privilege Boundary and Ring 3 Execution), we implemented a synthetic testing framework to verify that Ring 3 user mode correctly faults when attempting to access privileged resources. To keep the testing suite self-contained without crashing the kernel or requiring complex process management, we intercept exceptions (like `#PF` and `#GP`) in `fault_handler`. 

When an expected fault occurs during a test, `fault_handler` rewrites the saved `CS`, `SS`, and `RIP` in the interrupt frame (`iretq` frame) to point back to the kernel-mode test wrapper. This effectively transitions execution from the faulting Ring 3 context directly back into the Ring 0 test suite.

## Decision

This exception recovery mechanism is **TEST-ONLY**. 

Exception recovery that rewrites `CS`/`SS`/`RIP` to resume the kernel test harness is permitted **only** inside the Phase 3–9 synthetic test framework.

Production user-mode fault handling (which will be developed alongside real process management) **must** terminate or otherwise manage the offending process. It must **never** return to kernel code through a modified user-originated `iret` frame.

## Consequences

- Our test framework can seamlessly validate hardware isolation boundaries without manual QEMU resets.
- We must build standard signal delivery and process termination routines when we introduce real `struct process` abstractions in Phase 10 and beyond.
- This ADR serves as a strict boundary marker to ensure this hack is never leaked into production execution paths.

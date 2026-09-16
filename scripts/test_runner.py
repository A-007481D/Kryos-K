#!/usr/bin/env python3
import subprocess
import sys
import time
import os

EXPECTED_PASSES = [
    "[PASS] boot",
    "[PASS] long_mode",
    "[PASS] higher_half",
    "[PASS] kassert",
    "[PASS] divide_by_zero",
    "[PASS] invalid_opcode",
    "[PASS] page_fault",
    "[PASS] pmm_basic",
    "[PASS] pmm_randomization",
    "[PASS] pmm_failures",
    "[PASS] pmm_exhaustion_oom",
    "[PASS] vmm_001_basic",
    "[PASS] vmm_002_fault",
    "[PASS] vmm_003_huge_page",
    "[PASS] vmm_004_alignment",
    "[PASS] vmm_005_canonical",
    "[PASS] vmm_006_double_map",
    "[PASS] vmm_007_pt_alloc",
    "[PASS] vmm_008_accounting",
    "[PASS] heap_001_minimal",
    "[PASS] heap_002_alignment",
    "[PASS] heap_003_boundaries",
    "[PASS] heap_004_split",
    "[PASS] heap_005_coalesce",
    "[PASS] heap_006_double_free",
    "[PASS] heap_007_invalid_ptr",
    "[PASS] heap_008_unaligned_ptr",
    "[PASS] heap_009_exhaustion",
    "[PASS] heap_010_verify",
    "[PASS] heap_011_corruption",
    "[PASS] heap_012_randomized",
    "[PASS] heap_013_accounting",
    "[PASS] thread_001_creation",
    "[PASS] thread_002_stack_construction",
    "[PASS] thread_002a_first_activation",
    "[PASS] thread_003_cooperative_yield",
    "[PASS] thread_006_thread_exit",
    "[PASS] thread_007_dead_skipped",
    "[PASS] thread_008_round_robin",
    "[PASS] thread_004_callee_saved",
    "[PASS] thread_005_stack_alignment",
    "[PASS] thread_010_randomized",
    "[PASS] thread_011_accounting",
    "[PASS] thread_009_idle_behavior",
    "[PASS] preempt_001_pit_init",
    "[PASS] preempt_002_irq0_delivery",
    "[PASS] preempt_003_pic_ack",
    "[PASS] preempt_007_critical_section",
    "[PASS] preempt_004_switching",
    "[PASS] preempt_005_register_preservation",
    "[PASS] preempt_006_rip_rflags_preservation",
    "[PASS] preempt_008_resumption",
    "[PASS] preempt_009_voluntary_preemptive_mix",
    "[PASS] preempt_010_dead_thread_reap",
    "[PASS] preempt_011_scheduler_accounting",
    "[PASS] preempt_012_cross_subsystem_stress",
    "TAR-001 Passed.",
    "TAR-002 Passed.",
    "TAR-003 Passed.",
    "TAR-004 Passed.",
    "TAR-005 Passed.",
    "TAR-006 Passed.",
    "TAR-007 Passed.",
    "TAR-008 Passed.",
    "TAR-009 Passed.",
    "TAR-010 Passed.",
    "VFS-001 Passed.",
    "VFS-002 Passed.",
    "VFS-003 Passed.",
    "VFS-004 Passed.",
    "VFS-010 Passed.",
    "VFS-006 Passed.",
    "VFS-005 Passed.",
    "VFS-007 Passed.",
    "VFS-008 Passed.",
    "VFS-009 Passed.",
    "FS-001 Passed.",
    "FS-002 Passed.",
    "FS-010 Passed.",
    "FS-004 Passed.",
    "FS-007 Passed.",
    "FS-003 Passed.",
    "FS-005 Passed.",
    "FS-006 Passed.",
    "FS-008 Passed.",
    "FS-009 Passed.",
    "[PASS] PROC-001",
    "[PASS] PROC-002",
    "[PASS] PROC-003",
    "[PASS] PROC-004",
    "[PASS] PROC-005",
    "[PASS] PROC-006",
    "[PASS] PROC-007",
    "[PASS] PROC-008",
    "[PASS] PROC-009",
    "[PASS] PROC-010",
    "[PASS] PROC-011",
    "[PASS] PROC-012",
    "[PASS] PROC-013",
    "[PASS] PROC-014",
    "[PASS] PROC-015",
    "[PASS] PROC-016",
    "[PASS] PROC-017",
    "[PASS] PROC-018",
    "[PASS] PROC-019",
    "[PASS] PROC-020",
    "@@KRYOS:SUITE:PASS"
]

def main():
    print("=== Kryos Test Runner ===")
    
    # 1. Build ISO
    print("Building Kryos...")
    res = subprocess.run(["make", "clean"], capture_output=True)
    res = subprocess.run(["make", "iso"], capture_output=True)
    if res.returncode != 0:
        print("Build failed!")
        print(res.stderr.decode())
        sys.exit(1)
        
    iso_path = "build/kryos.iso"
    if not os.path.exists(iso_path):
        print("ISO not found at", iso_path)
        sys.exit(1)
        
    # 2. Run QEMU
    print("Booting QEMU...")
    cmd = [
        "qemu-system-x86_64",
        "-cdrom", iso_path,
        "-serial", "stdio",
        "-display", "none",
        "-no-reboot",
        "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04"
    ]
    
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    
    output = []
    start_time = time.time()
    timeout = 10.0
    
    panic_detected = False
    unhandled_exception_detected = False
    tests_completed = False
    expecting_panic = False
    
    while True:
        if time.time() - start_time > timeout:
            process.kill()
            print("\n[FAIL] TIMEOUT")
            sys.exit(1)
            
        line = process.stdout.readline()
        if not line and process.poll() is not None:
            break
            
        if line:
            line = line.strip()
            print(f"| {line}")
            output.append(line)
            
            if "@@KRYOS:SUITE:PASS" in line:
                tests_completed = True
                
            if "@@KRYOS:TEST:panic:EXPECTED" in line:
                expecting_panic = True
            
            if "KERNEL PANIC" in line:
                panic_detected = True
                if tests_completed and expecting_panic:
                    process.kill() # Terminate VM early to speed up test execution
                    break
            
            if "UNHANDLED CPU EXCEPTION" in line:
                unhandled_exception_detected = True
                
    print("\n=== Test Results ===")
    
    if unhandled_exception_detected:
        print("[FAIL] UNEXPECTED EXCEPTION")
        sys.exit(1)
        
    if panic_detected and not (tests_completed and expecting_panic):
        print("[FAIL] KERNEL PANIC (Unexpected)")
        sys.exit(1)
        
    missing = [e for e in EXPECTED_PASSES if e not in output]
    if missing:
        print("[FAIL] NO TEST COMPLETION: Missing expected output:")
        for m in missing:
            print(f"  - {m}")
        print("[FAIL] QEMU RESET / TRIPLE FAULT or early exit")
        sys.exit(1)
        
    if not (panic_detected and expecting_panic):
        print("[FAIL] TEST FAILURE: Expected terminal panic did not occur.")
        sys.exit(1)
        
    print("[SUCCESS] All tests passed!")
    sys.exit(0)

if __name__ == "__main__":
    main()

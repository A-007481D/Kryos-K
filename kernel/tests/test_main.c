#include "tests.h"
#include "interrupts.h"
#include "stdio.h"
#include "assert.h"
#include "serial.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdbool.h>

#define SAVE_RECOVERY_STATE() __asm__ volatile( \
    "lea 1f(%%rip), %%rcx\n" \
    "mov %%rcx, %0\n" \
    "mov %%rsp, %1\n" \
    "mov %%rbp, %2\n" \
    "mov %%rbx, %3\n" \
    "mov %%r12, %4\n" \
    "mov %%r13, %5\n" \
    "mov %%r14, %6\n" \
    "mov %%r15, %7\n" \
    : "=m"(current_test_context.recovery_rip), \
      "=m"(current_test_context.recovery_rsp), \
      "=m"(current_test_context.recovery_rbp), \
      "=m"(current_test_context.recovery_rbx), \
      "=m"(current_test_context.recovery_r12), \
      "=m"(current_test_context.recovery_r13), \
      "=m"(current_test_context.recovery_r14), \
      "=m"(current_test_context.recovery_r15) \
    : : "rcx", "memory" \
)

static void test_kassert(void) {
    KASSERT(1 == 1);
    serial_puts("[PASS] kassert\n");
}

static void test_divide_by_zero(void) {
    current_test_context.expected_vector = 0; // #DE
    current_test_context.active = true;
    current_test_context.expected_cr2 = 0;
    
    SAVE_RECOVERY_STATE();
    __asm__ volatile(
        "div %0\n"
        "1:\n"
        : 
        : "r"(0)
        : "rax", "rdx", "memory"
    );
    
    current_test_context.active = false;
    serial_puts("[PASS] divide_by_zero\n");
}

static void test_invalid_opcode(void) {
    current_test_context.expected_vector = 6; // #UD
    current_test_context.active = true;
    current_test_context.expected_cr2 = 0;
    
    SAVE_RECOVERY_STATE();
    __asm__ volatile(
        "ud2\n"
        "1:\n"
        : : : "memory"
    );
    
    current_test_context.active = false;
    serial_puts("[PASS] invalid_opcode\n");
}

static void test_page_fault(void) {
    current_test_context.expected_vector = 14; // #PF
    current_test_context.expected_cr2 = 0xDEADBEEF;
    current_test_context.active = true;
    
    SAVE_RECOVERY_STATE();
    __asm__ volatile(
        "mov %%rdx, (%%rax)\n"
        "1:\n"
        : 
        : "a"(0xDEADBEEF), "d"(0xCAFEBABEULL)
        : "memory"
    );
    
    current_test_context.active = false;
    serial_puts("[PASS] page_fault\n");
}

static void test_panic(void) {
    serial_puts("@@KRYOS:SUITE:PASS\n");
    serial_puts("@@KRYOS:TEST:panic:EXPECTED\n");
    // Trigger expected panic.
    KASSERT(false);
}

#include "../../include/pmm.h"
#include "../memory/vmm.h"
#include "../memory/virt.h"

// Simple pseudo-random number generator for shuffling
static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void test_pmm_basic(void) {
    uint64_t initial_free = pmm_free_frames();
    uint64_t initial_used = pmm_used_frames();
    
    // PMM-001: Unique frames
    uint64_t p1 = pmm_alloc_page();
    uint64_t p2 = pmm_alloc_page();
    KASSERT(p1 != p2);
    KASSERT(p1 != 0);
    KASSERT(p2 != 0);
    
    // PMM-007: Accounting invariants
    KASSERT(pmm_free_frames() == initial_free - 2);
    KASSERT(pmm_used_frames() == initial_used + 2);
    KASSERT(pmm_total_frames() == pmm_free_frames() + pmm_used_frames());
    
    // PMM-002: Reallocation after free
    pmm_free_page(p1);
    uint64_t p3 = pmm_alloc_page();
    KASSERT(p1 == p3);
    
    pmm_free_page(p2);
    pmm_free_page(p3);
    
    KASSERT(pmm_free_frames() == initial_free);
    serial_puts("[PASS] pmm_basic\n");
}

static void test_pmm_exhaustion(void) {
    uint64_t initial_free = pmm_free_frames();
    
    // We will track the allocated pages to free them later.
    // 524288 frames * 8 bytes = 4 MiB (Supports up to 2 GiB RAM)
    static uint64_t oom_pages[524288]; 
    uint64_t count = 0;
    
    // PMM-008: OOM Exhaustion
    while (true) {
        uint64_t p = pmm_alloc_page();
        if (p == 0) break;
        if (count < 524288) {
            oom_pages[count] = p;
        } else {
            panic(__FILE__, __LINE__, "OOM Test exceeded array capacity");
        }
        count++;
    }
    
    KASSERT(pmm_free_frames() == 0);
    KASSERT(count == initial_free);
    
    // Free them in reverse order
    for (uint64_t i = count; i > 0; i--) {
        pmm_free_page(oom_pages[i - 1]);
    }
    
    KASSERT(pmm_free_frames() == initial_free);
    serial_puts("[PASS] pmm_exhaustion_oom\n");
}

static void test_pmm_randomization(void) {
    // We will allocate N pages, store them, shuffle them, and free them.
    #define NUM_TEST_PAGES 1024
    static uint64_t pages[NUM_TEST_PAGES]; // static to avoid stack overflow
    
    uint64_t initial_free = pmm_free_frames();
    
    for (int i = 0; i < NUM_TEST_PAGES; i++) {
        pages[i] = pmm_alloc_page();
        KASSERT(pages[i] != 0);
    }
    
    KASSERT(pmm_free_frames() == initial_free - NUM_TEST_PAGES);
    
    // Shuffle
    uint32_t state = 0xBADF00D;
    for (int i = NUM_TEST_PAGES - 1; i > 0; i--) {
        int j = xorshift32(&state) % (i + 1);
        uint64_t tmp = pages[i];
        pages[i] = pages[j];
        pages[j] = tmp;
    }
    
    // Free in random order
    for (int i = 0; i < NUM_TEST_PAGES; i++) {
        pmm_free_page(pages[i]);
    }
    
    KASSERT(pmm_free_frames() == initial_free);
    serial_puts("[PASS] pmm_randomization\n");
}

#define TEST_PANIC(expr, msg) do { \
    current_test_context.expected_vector = 6; /* #UD triggered by panic() */ \
    current_test_context.active = true; \
    current_test_context.expected_cr2 = 0; \
    SAVE_RECOVERY_STATE(); \
    expr; \
    current_test_context.active = false; \
    panic(__FILE__, __LINE__, "Expected panic did not occur: " msg); \
    __asm__ volatile("1:\n" : : : "memory"); \
    current_test_context.active = false; \
} while(0)

static void test_pmm_failures(void) {
    volatile uint64_t initial_free = pmm_free_frames();
    
    // PMM-003: Double-Free Detection
    volatile uint64_t p1 = pmm_alloc_page();
    pmm_free_page(p1);
    TEST_PANIC(pmm_free_page(p1), "Double free");
    
    // PMM-004: Reserved Frame Free Detection
    TEST_PANIC(pmm_free_page(0x0), "Reserved free");
    
    // PMM-005: Unaligned Free Detection
    TEST_PANIC(pmm_free_page(p1 + 1), "Unaligned free");
    
    // PMM-006: Out-of-Range Free Detection
    TEST_PANIC(pmm_free_page(0xFFFFFFFFFFFFF000ULL), "Out of range free");
    
    KASSERT(pmm_free_frames() == initial_free);
    serial_puts("[PASS] pmm_failures\n");
}

static void test_vmm_001_basic(void) {
    uint64_t phys_page = pmm_alloc_page();
    KASSERT(phys_page != 0);
    uint64_t test_virt = 0x0000700000000000ULL;
    
    bool mapped = vmm_map_page(test_virt, phys_page, VMM_FLAG_WRITABLE);
    KASSERT(mapped == true);
    
    uint64_t out_phys;
    KASSERT(vmm_get_phys(test_virt, &out_phys) == true);
    KASSERT(out_phys == phys_page);
    
    volatile uint64_t* ptr = (volatile uint64_t*)test_virt;
    *ptr = 0xCAFEBABE12345678ULL;
    KASSERT(*ptr == 0xCAFEBABE12345678ULL);
    
    // We intentionally don't unmap here so VMM-002 can test unmap.
    serial_puts("[PASS] vmm_001_basic\n");
}

static void test_vmm_002_fault(void) {
    uint64_t test_virt = 0x0000700000000000ULL;
    
    bool unmapped = vmm_unmap_page(test_virt);
    KASSERT(unmapped == true);
    
    current_test_context.expected_vector = 14; // #PF
    current_test_context.expected_cr2 = test_virt;
    current_test_context.active = true;
    
    SAVE_RECOVERY_STATE();
    __asm__ volatile(
        "mov %%rdx, (%%rax)\n"
        "1:\n"
        : 
        : "a"(test_virt), "d"(0xDEADBEEFULL)
        : "memory"
    );
    
    current_test_context.active = false;
    
    // The physical frame from VMM-001 is now "leaked" to simulate caller ownership 
    // without tracking it, which is fine for this test suite.
    serial_puts("[PASS] vmm_002_fault\n");
}

static void test_vmm_003_huge_page(void) {
    extern char _kernel_start[];
    uint64_t kernel_virt = (uint64_t)_kernel_start;
    
    uint64_t out_phys;
    KASSERT(vmm_get_phys(kernel_virt, &out_phys) == true);
    KASSERT(out_phys == (kernel_virt - 0xFFFFFFFF80000000ULL));
    
    // Attempt to map inside the huge page, should be rejected
    uint64_t new_phys = pmm_alloc_page();
    KASSERT(vmm_map_page(kernel_virt, new_phys, VMM_FLAG_WRITABLE) == false);
    
    // Attempt to unmap inside the huge page, should be rejected
    KASSERT(vmm_unmap_page(kernel_virt) == false);
    
    pmm_free_page(new_phys);
    serial_puts("[PASS] vmm_003_huge_page\n");
}

static void test_vmm_004_alignment(void) {
    uint64_t test_virt = 0x0000700000001000ULL;
    uint64_t phys_page = pmm_alloc_page();
    
    KASSERT(vmm_map_page(test_virt + 1, phys_page, VMM_FLAG_WRITABLE) == false);
    KASSERT(vmm_map_page(test_virt, phys_page + 1, VMM_FLAG_WRITABLE) == false);
    
    pmm_free_page(phys_page);
    serial_puts("[PASS] vmm_004_alignment\n");
}

static void test_vmm_005_canonical(void) {
    uint64_t non_canonical = 0x0000800000000000ULL; // Bit 47 is 1, bits 48-63 are 0
    uint64_t phys_page = pmm_alloc_page();
    
    KASSERT(vmm_map_page(non_canonical, phys_page, VMM_FLAG_WRITABLE) == false);
    KASSERT(vmm_unmap_page(non_canonical) == false);
    KASSERT(vmm_get_phys(non_canonical, NULL) == false);
    
    pmm_free_page(phys_page);
    serial_puts("[PASS] vmm_005_canonical\n");
}

static void test_vmm_006_double_map(void) {
    uint64_t test_virt = 0x0000700000002000ULL;
    uint64_t phys_page1 = pmm_alloc_page();
    uint64_t phys_page2 = pmm_alloc_page();
    
    KASSERT(vmm_map_page(test_virt, phys_page1, VMM_FLAG_WRITABLE) == true);
    
    // Second map should fail
    KASSERT(vmm_map_page(test_virt, phys_page2, VMM_FLAG_WRITABLE) == false);
    
    KASSERT(vmm_unmap_page(test_virt) == true);
    
    pmm_free_page(phys_page1);
    pmm_free_page(phys_page2);
    
    serial_puts("[PASS] vmm_006_double_map\n");
}

static void test_vmm_007_008_accounting(void) {
    uint64_t free_before = pmm_free_frames();
    
    uint64_t test_virt = 0x0000600000000000ULL;
    uint64_t phys_page = pmm_alloc_page(); // Caller allocates the data frame
    
    KASSERT(pmm_free_frames() == free_before - 1);
    
    KASSERT(vmm_map_page(test_virt, phys_page, VMM_FLAG_WRITABLE) == true);
    
    uint64_t free_after_map = pmm_free_frames();
    KASSERT(free_after_map == free_before - 1 - 3);
    serial_puts("[PASS] vmm_007_pt_alloc\n");
    
    KASSERT(vmm_unmap_page(test_virt) == true);
    
    KASSERT(pmm_free_frames() == free_after_map);
    
    pmm_free_page(phys_page);
    
    KASSERT(pmm_free_frames() == free_after_map + 1);
    serial_puts("[PASS] vmm_008_accounting\n");
}

void run_kernel_tests(void) {
    test_kassert();
    test_divide_by_zero();
    test_invalid_opcode();
    test_page_fault();
    
    test_pmm_basic();
    test_pmm_randomization();
    test_pmm_failures();
    test_pmm_exhaustion(); // Now tracked and freed safely
    
    test_vmm_001_basic();
    test_vmm_002_fault();
    test_vmm_003_huge_page();
    test_vmm_004_alignment();
    test_vmm_005_canonical();
    test_vmm_006_double_map();
    test_vmm_007_008_accounting();
    
    test_panic();
}

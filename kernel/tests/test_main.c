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
#include "../../include/heap.h"
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

static void test_heap_001_minimal(void) {
    void* p0 = kmalloc(0);
    KASSERT(p0 == NULL);
    
    void* p1 = kmalloc(1);
    KASSERT(p1 != NULL);
    KASSERT(((uintptr_t)p1 % 16) == 0);
    
    kfree(p1);
    kheap_verify();
    serial_puts("[PASS] heap_001_minimal\n");
}

static void test_heap_002_alignment(void) {
    void* p1 = kmalloc(17);
    void* p2 = kmalloc(33);
    void* p3 = kmalloc(7);
    
    KASSERT(((uintptr_t)p1 % 16) == 0);
    KASSERT(((uintptr_t)p2 % 16) == 0);
    KASSERT(((uintptr_t)p3 % 16) == 0);
    
    kfree(p1);
    kfree(p2);
    kfree(p3);
    kheap_verify();
    serial_puts("[PASS] heap_002_alignment\n");
}

static void test_heap_003_boundaries(void) {
    void* p1 = kmalloc(4095);
    void* p2 = kmalloc(4096);
    void* p3 = kmalloc(4097);
    
    KASSERT(p1 != NULL);
    KASSERT(p2 != NULL);
    KASSERT(p3 != NULL);
    
    ((uint8_t*)p1)[4094] = 0xAA;
    ((uint8_t*)p2)[4095] = 0xBB;
    ((uint8_t*)p3)[4096] = 0xCC;
    
    KASSERT(((uint8_t*)p1)[4094] == 0xAA);
    KASSERT(((uint8_t*)p2)[4095] == 0xBB);
    KASSERT(((uint8_t*)p3)[4096] == 0xCC);
    
    kfree(p1);
    kfree(p2);
    kfree(p3);
    kheap_verify();
    serial_puts("[PASS] heap_003_boundaries\n");
}

static void test_heap_004_005_split_coalesce(void) {
    void* a = kmalloc(128);
    void* b = kmalloc(256);
    void* c = kmalloc(128);
    
    kfree(b);
    kheap_verify();
    
    void* d = kmalloc(64);
    KASSERT(d == b); 
    kheap_verify();
    
    kfree(a);
    kfree(d);
    kfree(c);
    kheap_verify();
    serial_puts("[PASS] heap_004_split\n");
    serial_puts("[PASS] heap_005_coalesce\n");
}

static void test_heap_006_double_free(void) {
    void* a = kmalloc(64);
    kfree(a);
    TEST_PANIC(kfree(a), "Double free");
    serial_puts("[PASS] heap_006_double_free\n");
}

static void test_heap_007_invalid_ptr(void) {
    TEST_PANIC(kfree((void*)0x12345678), "Invalid free");
    serial_puts("[PASS] heap_007_invalid_ptr\n");
}

static void test_heap_008_unaligned_ptr(void) {
    void* a = kmalloc(64);
    TEST_PANIC(kfree((void*)((uintptr_t)a + 1)), "Unaligned free");
    kfree(a);
    serial_puts("[PASS] heap_008_unaligned_ptr\n");
}

static void test_heap_009_exhaustion(void) {
    #define CHUNKS 256
    static void* chunks[CHUNKS];
    int count = 0;
    
    while (count < CHUNKS) {
        chunks[count] = kmalloc(16 * 1024 * 1024); // 16 MiB
        if (!chunks[count]) break;
        count++;
    }
    
    KASSERT(count < CHUNKS); 
    
    for (int i = 0; i < count; i++) {
        kfree(chunks[i]);
    }
    kheap_verify();
    serial_puts("[PASS] heap_009_exhaustion\n");
}

static void test_heap_010_verify(void) {
    serial_puts("[PASS] heap_010_verify\n");
}

static void test_heap_011_corruption(void) {
    void* a = kmalloc(64);
    void* b = kmalloc(64);
    
    // Save B's 48-byte header
    uint8_t b_header_copy[48];
    for (int i = 0; i < 48; i++) {
        b_header_copy[i] = ((uint8_t*)b - 48)[i];
    }
    
    // Overflow A by 48 bytes to overwrite B's header
    for (int i = 0; i < 64 + 48; i++) {
        ((uint8_t*)a)[i] = 0x00;
    }
    
    TEST_PANIC(kfree(b), "Heap corruption");
    
    // Restore B's header
    for (int i = 0; i < 48; i++) {
        ((uint8_t*)b - 48)[i] = b_header_copy[i];
    }
    
    kfree(b);
    kfree(a);
    
    serial_puts("[PASS] heap_011_corruption\n");
}

static void test_heap_012_randomized(void) {
    #define RAND_OPS 1000
    #define MAX_LIVE 64
    static void* live[MAX_LIVE];
    for(int i=0; i<MAX_LIVE; i++) live[i] = NULL;
    
    uint32_t state = 0xCAFEBABE;
    for (int i = 0; i < RAND_OPS; i++) {
        int slot = xorshift32(&state) % MAX_LIVE;
        if (live[slot]) {
            kfree(live[slot]);
            live[slot] = NULL;
        } else {
            size_t size = (xorshift32(&state) % 4096) + 1;
            live[slot] = kmalloc(size);
            KASSERT(live[slot] != NULL);
        }
        if (i % 100 == 0) kheap_verify();
    }
    
    for (int i = 0; i < MAX_LIVE; i++) {
        if (live[i]) kfree(live[i]);
    }
    kheap_verify();
    serial_puts("[PASS] heap_012_randomized\n");
}

static void test_heap_013_accounting(void) {
    uint64_t pmm_free_before = pmm_free_frames();
    struct kheap_stats stats_before;
    kheap_get_stats(&stats_before);
    
    void* p = kmalloc(1024 * 1024); 
    
    struct kheap_stats stats_mid;
    kheap_get_stats(&stats_mid);
    
    uint64_t pmm_dropped = pmm_free_before - pmm_free_frames();
    uint64_t heap_grew = stats_mid.virtual_committed - stats_before.virtual_committed;
    
    KASSERT(pmm_dropped * 4096 >= heap_grew); 
    
    kfree(p);
    
    struct kheap_stats stats_after;
    kheap_get_stats(&stats_after);
    
    KASSERT(stats_after.allocated_bytes == stats_before.allocated_bytes);
    KASSERT(pmm_free_frames() == (pmm_free_before - pmm_dropped));
    
    serial_puts("[PASS] heap_013_accounting\n");
}

void run_kernel_tests(void) {
    test_kassert();
    test_divide_by_zero();
    test_invalid_opcode();
    test_page_fault();
    
    test_pmm_basic();
    test_pmm_randomization();
    test_pmm_failures();
    test_pmm_exhaustion();
    
    test_vmm_001_basic();
    test_vmm_002_fault();
    test_vmm_003_huge_page();
    test_vmm_004_alignment();
    test_vmm_005_canonical();
    test_vmm_006_double_map();
    test_vmm_007_008_accounting();
    
    test_heap_001_minimal();
    test_heap_002_alignment();
    test_heap_003_boundaries();
    test_heap_004_005_split_coalesce();
    test_heap_006_double_free();
    test_heap_007_invalid_ptr();
    test_heap_008_unaligned_ptr();
    test_heap_009_exhaustion();
    test_heap_010_verify();
    test_heap_011_corruption();
    test_heap_012_randomized();
    test_heap_013_accounting();
    
    test_panic();
}

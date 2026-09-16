#include "../../include/heap.h"
#include "../../include/pmm.h"
#include "../lib/irq.h"
#include "vmm.h"
#include "../../include/process.h"
#include "layout.h"
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#define HEAP_MAGIC 0xC001BEEFDEADBEEFULL
#define HEAP_FREE  0x0000000000000000ULL
#define HEAP_USED  0xFFFFFFFFFFFFFFFFULL

struct heap_block {
    uint64_t magic;
    uint64_t size;    // 16-byte aligned payload capacity
    uint64_t state;   // HEAP_FREE or HEAP_USED
    struct heap_block *next;
    struct heap_block *prev;
    uint64_t _padding; // Forces sizeof(struct heap_block) == 48
};

_Static_assert(sizeof(struct heap_block) % 16 == 0, "heap block header must be 16-byte aligned");

static uint64_t heap_start = 0;
static uint64_t heap_committed_end = 0;
static uint64_t heap_reserved_end = 0;
static struct heap_block* head = NULL;

static inline size_t align_up(size_t val, size_t alignment) {
    return (val + alignment - 1) & ~(alignment - 1);
}

void kheap_init(uint64_t virtual_base) {
    heap_start = virtual_base;
    heap_committed_end = heap_start;
    heap_reserved_end = KERNEL_HEAP_MAX;
    head = NULL;
}

// Expands the heap by at least `bytes` bytes. Returns false on OOM.
static bool expand_heap(size_t bytes) {
    size_t pages_needed = align_up(bytes, PMM_PAGE_SIZE) / PMM_PAGE_SIZE;
    if (heap_committed_end + pages_needed * PMM_PAGE_SIZE > heap_reserved_end) {
        return false; // Virtual OOM
    }
    
    // Allocate and map
    for (size_t i = 0; i < pages_needed; i++) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) {
            // Note: Returning false here leaks physical pages allocated in this loop prior to failure.
            // A more complex allocator would unwind/rollback. This is acceptable for Phase 6.
            return false; // Physical OOM
        }
        
        bool mapped = vmm_map_page(&kernel_process->as, heap_committed_end, phys, VMM_FLAG_WRITABLE);
        if (!mapped) {
            pmm_free_page(phys);
            return false;
        }
        heap_committed_end += PMM_PAGE_SIZE;
    }
    
    return true;
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    irq_state_t flags = irq_save();

    size_t aligned_size = align_up(size, 16);
    size_t block_extent = sizeof(struct heap_block) + aligned_size;
    
    while (true) {
        struct heap_block* curr = head;
        struct heap_block* tail = NULL;
        
        while (curr) {
            if (curr->state == HEAP_FREE && curr->size >= aligned_size) {
                size_t remaining = curr->size - aligned_size;
                // Minimum free block must hold header + 16 bytes payload
                if (remaining >= sizeof(struct heap_block) + 16) {
                    struct heap_block* new_block = (struct heap_block*)((uint8_t*)curr + block_extent);
                    new_block->magic = HEAP_MAGIC;
                    new_block->size = remaining - sizeof(struct heap_block);
                    new_block->state = HEAP_FREE;
                    
                    new_block->prev = curr;
                    new_block->next = curr->next;
                    if (new_block->next) new_block->next->prev = new_block;
                    
                    curr->next = new_block;
                    curr->size = aligned_size;
                }
                curr->state = HEAP_USED;
                irq_restore(flags);
                return (void*)((uint8_t*)curr + sizeof(struct heap_block));
            }
            tail = curr;
            curr = curr->next;
        }
        
        // Expand
        uint64_t prev_end = heap_committed_end;
        bool expanded = expand_heap(block_extent);
        
        if (heap_committed_end > prev_end) {
            if (tail && tail->state == HEAP_FREE) {
                tail->size += (heap_committed_end - prev_end);
            } else {
                struct heap_block* new_block = (struct heap_block*)prev_end;
                new_block->magic = HEAP_MAGIC;
                new_block->size = (heap_committed_end - prev_end) - sizeof(struct heap_block);
                new_block->state = HEAP_FREE;
                new_block->next = NULL;
                new_block->prev = tail;
                if (tail) tail->next = new_block;
                else head = new_block;
            }
        }
        
        if (!expanded) {
            irq_restore(flags);
            return NULL;
        }
    }
}

void kfree(void* ptr) {
    if (!ptr) return;
    
    irq_state_t flags = irq_save();
    
    uintptr_t addr = (uintptr_t)ptr;
    KASSERT(addr >= heap_start + sizeof(struct heap_block) && addr < heap_committed_end);
    KASSERT(addr % 16 == 0);
    
    struct heap_block* block = (struct heap_block*)(addr - sizeof(struct heap_block));
    KASSERT(block->magic == HEAP_MAGIC);
    KASSERT(block->state == HEAP_USED);
    
    block->state = HEAP_FREE;
    
    // Coalesce with next
    if (block->next && block->next->state == HEAP_FREE) {
        struct heap_block* next = block->next;
        block->size += sizeof(struct heap_block) + next->size;
        block->next = next->next;
        if (block->next) block->next->prev = block;
    }
    
    // Coalesce with prev
    if (block->prev && block->prev->state == HEAP_FREE) {
        struct heap_block* prev = block->prev;
        prev->size += sizeof(struct heap_block) + block->size;
        prev->next = block->next;
        if (prev->next) prev->next->prev = prev;
    }
    
    irq_restore(flags);
}

void kheap_verify(void) {
    if (!head) return;
    
    struct heap_block* curr = head;
    uint64_t total_extent = 0;
    
    while (curr) {
        uintptr_t addr = (uintptr_t)curr;
        KASSERT(addr >= heap_start && addr < heap_committed_end);
        
        uint64_t extent = sizeof(struct heap_block) + curr->size;
        KASSERT(addr + extent <= heap_committed_end);
        
        uintptr_t payload_addr = addr + sizeof(struct heap_block);
        KASSERT(payload_addr % 16 == 0);
        KASSERT(curr->size % 16 == 0);
        KASSERT(extent % 16 == 0);
        
        if (curr->next) {
            KASSERT(curr->next->prev == curr);
            KASSERT(addr + extent == (uintptr_t)curr->next);
        } else {
            KASSERT(addr + extent == heap_committed_end);
        }
        
        if (curr->prev) {
            KASSERT(curr->prev->next == curr);
        } else {
            KASSERT(curr == head);
        }
        
        if (curr->state == HEAP_FREE && curr->next) {
            KASSERT(curr->next->state == HEAP_USED);
        }
        
        total_extent += extent;
        curr = curr->next;
    }
    
    KASSERT(total_extent == heap_committed_end - heap_start);
}

void kheap_get_stats(struct kheap_stats* out_stats) {
    if (!out_stats) return;
    
    out_stats->virtual_reserved = heap_reserved_end - heap_start;
    out_stats->virtual_committed = heap_committed_end - heap_start;
    out_stats->allocated_bytes = 0;
    out_stats->free_bytes = 0;
    out_stats->block_count = 0;
    out_stats->free_block_count = 0;
    
    struct heap_block* curr = head;
    while (curr) {
        out_stats->block_count++;
        if (curr->state == HEAP_FREE) {
            out_stats->free_block_count++;
            out_stats->free_bytes += curr->size;
        } else {
            out_stats->allocated_bytes += curr->size;
        }
        curr = curr->next;
    }
}

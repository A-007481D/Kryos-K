#pragma once

#include <stddef.h>
#include <stdint.h>

struct kheap_stats {
    uint64_t virtual_reserved;
    uint64_t virtual_committed;
    uint64_t allocated_bytes;
    uint64_t free_bytes;
    uint64_t block_count;
    uint64_t free_block_count;
};

// Initialize the kernel heap starting at a designated virtual base
void kheap_init(uint64_t virtual_base);

// Allocate an arbitrary sized block. Returns NULL on OOM.
// The payload is guaranteed to be 16-byte aligned.
void* kmalloc(size_t size);

// Free an allocated block. Panics on invalid pointers.
void kfree(void* ptr);

// Verify heap invariants (panics on failure)
void kheap_verify(void);

// Retrieve heap statistics
void kheap_get_stats(struct kheap_stats* out_stats);

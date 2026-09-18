#include "include/kryos.h"

typedef struct block_header {
    size_t size;         // Size of block (including header)
    int is_free;
    struct block_header *next;
} block_header_t;

static block_header_t *free_list = NULL;

void *sbrk(intptr_t increment) {
    uint64_t current = __syscall(SYS_BRK, 0, 0, 0, 0, 0, 0);
    if (increment == 0) {
        return (void*)current;
    }
    
    uint64_t new_brk = current + increment;
    uint64_t result = __syscall(SYS_BRK, new_brk, 0, 0, 0, 0, 0);
    if (result != new_brk) {
        return (void*)-1;
    }
    return (void*)current;
}

void *malloc(size_t size) {
    if (size == 0) return NULL;
    
    // Align size to 16 bytes, include header
    size_t total_size = size + sizeof(block_header_t);
    total_size = (total_size + 15) & ~15ULL;
    
    // First fit search
    block_header_t *curr = free_list;
    block_header_t *prev = NULL;
    
    while (curr) {
        if (curr->is_free && curr->size >= total_size) {
            // Split block if it's large enough (at least enough for another minimal block)
            if (curr->size >= total_size + sizeof(block_header_t) + 16) {
                block_header_t *new_block = (block_header_t*)((uint8_t*)curr + total_size);
                new_block->size = curr->size - total_size;
                new_block->is_free = 1;
                new_block->next = curr->next;
                
                curr->size = total_size;
                curr->next = new_block;
            }
            curr->is_free = 0;
            return (void*)((uint8_t*)curr + sizeof(block_header_t));
        }
        prev = curr;
        curr = curr->next;
    }
    
    // Expand heap
    block_header_t *new_block = (block_header_t*)sbrk(total_size);
    if (new_block == (void*)-1) {
        return NULL;
    }
    
    new_block->size = total_size;
    new_block->is_free = 0;
    new_block->next = NULL;
    
    if (prev) {
        prev->next = new_block;
    } else {
        free_list = new_block;
    }
    
    return (void*)((uint8_t*)new_block + sizeof(block_header_t));
}

void free(void *ptr) {
    if (!ptr) return;
    
    block_header_t *block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    block->is_free = 1;
    
    // Simple coalesce sweep
    block_header_t *curr = free_list;
    while (curr && curr->next) {
        // If they are strictly adjacent in memory
        if (curr->is_free && curr->next->is_free && ((uint8_t*)curr + curr->size == (uint8_t*)curr->next)) {
            curr->size += curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

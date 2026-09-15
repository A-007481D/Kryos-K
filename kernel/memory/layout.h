#pragma once

// Provisional Kernel Virtual Address Layout
// This layout serves as a placeholder until we formalize per-CPU data, MMIO, and vmalloc regions.
#define KERNEL_VIRT_BASE   0xFFFFFFFF80000000ULL // Boot mappings (text/data)
#define KERNEL_HEAP_BASE   0xFFFFC00000000000ULL // Temporary heap base
#define KERNEL_HEAP_MAX    0xFFFFC00040000000ULL // 1 GiB max reserved space

#ifndef KRYOS_PROCESS_H
#define KRYOS_PROCESS_H

#include <stdint.h>

typedef int pid_t;

typedef struct address_space {
    uint64_t pml4_phys;
} address_space_t;

struct process {
    pid_t pid;
    address_space_t as;
};

// Global reference to the kernel process (PID 0)
extern struct process* kernel_process;

// Process management APIs
void process_init(void);
struct process* process_create(void);
void process_destroy(struct process* proc);

#endif // KRYOS_PROCESS_H

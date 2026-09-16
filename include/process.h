#ifndef KRYOS_PROCESS_H
#define KRYOS_PROCESS_H

#include <stdint.h>

typedef int pid_t;

typedef struct address_space {
    uint64_t pml4_phys;
} address_space_t;

typedef enum {
    PROCESS_RUNNING,
    PROCESS_TERMINATED,
    PROCESS_DEAD,
    PROCESS_ZOMBIE
} process_state_t;

#define MAX_FDS 32

struct file; // forward declaration
struct thread;

struct process {
    pid_t pid;
    process_state_t state;
    address_space_t as;
    struct file *fd_table[MAX_FDS];
    
    struct process *parent;
    struct process *children_head;
    struct process *next_sibling;
    
    int exit_status;
    struct thread *waiter;
};

// Global reference to the kernel process (PID 0)
extern struct process* kernel_process;

// Process management APIs
void process_init(void);
struct process* process_create(void);
void process_destroy(struct process* proc);
void process_terminate(struct process* proc);
void process_reparent_children(struct process *proc, struct process *new_parent);
void process_exit(struct process *proc, int status);
#endif // KRYOS_PROCESS_H

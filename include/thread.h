#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum {
    THREAD_NEW,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DEAD
} thread_state_t;

struct process; // Forward declaration

struct thread {
    uint64_t id;
    uint64_t rsp;
    struct process *process;

    void *kernel_stack_base;
    size_t kernel_stack_size;

    void *user_stack_base;
    size_t user_stack_size;

    thread_state_t state;
    struct thread *next;
};

// Initializes the threading subsystem (creates a 'main' thread to represent the boot context).
void thread_init(void);

// Creates a new kernel thread
struct thread* thread_create(void (*entry_point)(void));

// Creates a new thread attached to a specific process
struct thread* thread_create_process(void (*entry_point)(void), struct process* process);
struct thread* thread_create_user(struct process* process, uint64_t rip, uint64_t rsp);

// Yields the CPU to the next READY thread in the round-robin list
void thread_yield(void);
void schedule(void);
_Noreturn void schedule_after_exit(void);

// Exits the current thread and yields to another. Never returns.
void thread_exit(void);
void thread_terminate_process(struct process *proc);

// Gets the currently running thread
struct thread* thread_current(void);

// Timer IRQ handler
void timer_handler(void);

extern volatile uint64_t scheduler_ticks;

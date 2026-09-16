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

struct thread {
    uint64_t id;
    uint64_t rsp;

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

// Yields the CPU to the next READY thread in the round-robin list
void thread_yield(void);

// Exits the current thread and yields to another. Never returns.
void thread_exit(void);

// Gets the currently running thread
struct thread* thread_current(void);

// Timer IRQ handler
void timer_handler(void);

extern volatile uint64_t scheduler_ticks;

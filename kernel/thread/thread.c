#include "../../include/thread.h"
#include "../../include/heap.h"
#include "../lib/irq.h"
#include "../interrupts/pic.h"
#include "../interrupts/gdt.h"
#include <assert.h>
#include <serial.h>
#include <stddef.h>
#include <stdbool.h>

extern void context_switch(struct thread* old_thread, struct thread* new_thread);

static struct thread* current = NULL;
static struct thread* head = NULL;
static uint64_t next_tid = 1;
volatile uint64_t scheduler_ticks = 0;
static bool in_scheduler = false;

#define STACK_SIZE 16384 // 16 KiB

void thread_init(void) {
    struct thread* main_thread = kmalloc(sizeof(struct thread));
    KASSERT(main_thread != NULL);
    
    main_thread->id = 0;
    main_thread->rsp = 0; 
    main_thread->kernel_stack_base = NULL;
    main_thread->kernel_stack_size = 0;
    main_thread->user_stack_base = NULL;
    main_thread->user_stack_size = 0;
    main_thread->state = THREAD_RUNNING;
    main_thread->next = main_thread;
    
    current = main_thread;
    head = main_thread;
}

struct thread* thread_current(void) {
    return current;
}

static void __attribute__((naked)) thread_start_wrapper(void) {
    __asm__ volatile(
        "sti\n"
        "call *%%r12\n"
        "call thread_exit\n"
        : : : "memory"
    );
}

struct thread* thread_create(void (*entry_point)(void)) {
    struct thread* t = kmalloc(sizeof(struct thread));
    KASSERT(t != NULL);
    
    t->id = next_tid++;
    t->kernel_stack_size = STACK_SIZE;
    t->kernel_stack_base = kmalloc(t->kernel_stack_size);
    KASSERT(t->kernel_stack_base != NULL);
    
    t->user_stack_size = 0;
    t->user_stack_base = NULL;
    
    uint64_t* stack_top = (uint64_t*)((uint8_t*)t->kernel_stack_base + t->kernel_stack_size);
    
    uintptr_t top = (uintptr_t)stack_top;
    top &= ~0xFULL;
    stack_top = (uint64_t*)top;
    
    *(--stack_top) = 0; // Alignment pad (maintains 16-byte alignment for SysV ABI)
    *(--stack_top) = (uint64_t)thread_exit; // Fake return address
    *(--stack_top) = (uint64_t)thread_start_wrapper; // Entry point (popped by ret)
    
    // Pushed in same order as context_switch (rbp first, r15 last)
    *(--stack_top) = 0; // rbp
    *(--stack_top) = 0; // rbx
    *(--stack_top) = (uint64_t)entry_point; // r12
    *(--stack_top) = 0; // r13
    *(--stack_top) = 0; // r14
    *(--stack_top) = 0; // r15
    
    t->rsp = (uint64_t)stack_top;
    t->state = THREAD_READY;
    
    irq_state_t flags = irq_save();
    struct thread* tail = head;
    while (tail->next != head) {
        tail = tail->next;
    }
    tail->next = t;
    t->next = head;
    irq_restore(flags);
    
    return t;
}

static void reap_dead_threads(void) {
    if (!head) return;
    
    struct thread* curr = head;
    struct thread* prev = NULL;
    
    struct thread* temp = head;
    while (temp->next != head) temp = temp->next;
    prev = temp;
    
    struct thread* start = head;
    bool done = false;
    
    while (!done) {
        struct thread* next_node = curr->next;
        
        if (curr->state == THREAD_DEAD && curr != current) {
            prev->next = next_node;
            if (curr == head) {
                head = next_node;
                start = next_node; 
            }
            if (curr->kernel_stack_base) {
                kfree(curr->kernel_stack_base);
            }
            if (curr->user_stack_base) {
                kfree(curr->user_stack_base);
            }
            kfree(curr);
            
            if (curr == next_node) {
                head = NULL;
                break;
            }
        } else {
            prev = curr;
        }
        
        curr = next_node;
        if (curr == start) done = true;
    }
}

static void schedule(void) {
    if (in_scheduler) return;
    in_scheduler = true;

    reap_dead_threads();
    
    if (current->state == THREAD_RUNNING) {
        current->state = THREAD_READY;
    }
    
    struct thread* next = current->next;
    while (next->state != THREAD_READY) {
        if (next == current) {
            if (current->state == THREAD_READY) {
                current->state = THREAD_RUNNING;
                in_scheduler = false;
                return;
            } else {
                serial_puts("No READY threads left. Halting.\n");
                while(1) {
                    __asm__ volatile("cli; hlt");
                }
            }
        }
        next = next->next;
    }
    
    struct thread* old = current;
    current = next;
    current->state = THREAD_RUNNING;
    
    in_scheduler = false;
    
    if (next->kernel_stack_base) {
        tss_set_rsp0((uint64_t)next->kernel_stack_base + next->kernel_stack_size);
    }
    
    if (old != current) {
        context_switch(old, current);
    }
}

void timer_handler(void) {
    scheduler_ticks++;
    pic_eoi(0);
    
    schedule();
}

void thread_yield(void) {
    KASSERT(current != NULL);
    irq_state_t flags = irq_save();
    schedule();
    irq_restore(flags);
}

void thread_exit(void) {
    KASSERT(current != NULL);
    
    __asm__ volatile("cli");
    
    current->state = THREAD_DEAD;
    schedule();
    
    KASSERT(false && "thread_exit returned!");
    while(1);
}

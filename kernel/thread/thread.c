#include "../../include/thread.h"
#include "../../include/heap.h"
#include <assert.h>
#include <serial.h>
#include <stddef.h>
#include <stdbool.h>

extern void context_switch(struct thread* old_thread, struct thread* new_thread);

static struct thread* current = NULL;
static struct thread* head = NULL;
static uint64_t next_tid = 1;

#define STACK_SIZE 16384 // 16 KiB

void thread_init(void) {
    struct thread* main_thread = kmalloc(sizeof(struct thread));
    KASSERT(main_thread != NULL);
    
    main_thread->id = 0;
    main_thread->rsp = 0; 
    main_thread->stack_base = NULL;
    main_thread->stack_size = 0;
    main_thread->state = THREAD_RUNNING;
    main_thread->next = main_thread;
    
    current = main_thread;
    head = main_thread;
}

struct thread* thread_current(void) {
    return current;
}

struct thread* thread_create(void (*entry_point)(void)) {
    struct thread* t = kmalloc(sizeof(struct thread));
    KASSERT(t != NULL);
    
    t->id = next_tid++;
    t->stack_size = STACK_SIZE;
    t->stack_base = kmalloc(t->stack_size);
    KASSERT(t->stack_base != NULL);
    
    uint64_t* stack_top = (uint64_t*)((uint8_t*)t->stack_base + t->stack_size);
    
    uintptr_t top = (uintptr_t)stack_top;
    top &= ~0xFULL;
    stack_top = (uint64_t*)top;
    
    *(--stack_top) = (uint64_t)thread_exit; // Fake return address
    *(--stack_top) = (uint64_t)entry_point; // Entry point (popped by ret)
    
    *(--stack_top) = 0; // r15
    *(--stack_top) = 0; // r14
    *(--stack_top) = 0; // r13
    *(--stack_top) = 0; // r12
    *(--stack_top) = 0; // rbx
    *(--stack_top) = 0; // rbp
    
    t->rsp = (uint64_t)stack_top;
    t->state = THREAD_READY;
    
    struct thread* tail = head;
    while (tail->next != head) {
        tail = tail->next;
    }
    tail->next = t;
    t->next = head;
    
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
            if (curr->stack_base) {
                kfree(curr->stack_base);
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

void thread_yield(void) {
    KASSERT(current != NULL);
    
    // Cooperative scheduling is performed without asynchronous entry
    __asm__ volatile("cli");
    
    reap_dead_threads();
    
    if (current->state == THREAD_RUNNING) {
        current->state = THREAD_READY;
    }
    
    struct thread* next = current->next;
    while (next->state != THREAD_READY) {
        if (next == current) {
            if (current->state == THREAD_READY) {
                current->state = THREAD_RUNNING;
                return;
            }
        }
        next = next->next;
    }
    
    struct thread* old = current;
    current = next;
    current->state = THREAD_RUNNING;
    
    if (old != current) {
        context_switch(old, current);
    }
}

void thread_exit(void) {
    KASSERT(current != NULL);
    
    __asm__ volatile("cli");
    
    current->state = THREAD_DEAD;
    
    struct thread* next = current->next;
    while (next->state != THREAD_READY) {
        if (next == current) {
            serial_puts("No READY threads left. Halting.\n");
            while(1) {
                __asm__ volatile("hlt");
            }
        }
        next = next->next;
    }
    
    struct thread* old = current;
    current = next;
    current->state = THREAD_RUNNING;
    
    context_switch(old, current);
    
    KASSERT(false && "thread_exit returned!");
    while(1);
}

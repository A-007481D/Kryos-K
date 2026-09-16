#include "../../include/process.h"
#include "../memory/vmm.h"
#include "../../include/heap.h"
#include "../memory/virt.h"
#include "../../include/pmm.h"
#include "../../include/thread.h"
#include "../../include/vfs.h"
#include "../../include/stdio.h"
#include <stddef.h>

static struct process _kernel_process = {0};
struct process* kernel_process = &_kernel_process;
static pid_t next_pid = 1;

void process_init(void) {
    kernel_process->pid = 0;
    // The kernel process uses the currently active PML4 (which is initialized in boot/VMM)
    kernel_process->as.pml4_phys = vmm_get_current_pml4();
    
    for (int i = 0; i < MAX_FDS; i++) {
        kernel_process->fd_table[i] = NULL;
    }
    
    struct file *con1 = kmalloc(sizeof(struct file));
    if (con1) {
        con1->vnode = console_get_vnode();
        con1->offset = 0;
        con1->flags = 0;
        con1->private_data = NULL;
        kernel_process->fd_table[1] = con1;
    }
    
    struct file *con2 = kmalloc(sizeof(struct file));
    if (con2) {
        con2->vnode = console_get_vnode();
        con2->offset = 0;
        con2->flags = 0;
        con2->private_data = NULL;
        kernel_process->fd_table[2] = con2;
    }
}

struct process* process_create(void) {
    struct process* proc = (struct process*)kmalloc(sizeof(struct process));
    if (!proc) return NULL;
    
    proc->pid = next_pid++;
    proc->state = PROCESS_RUNNING;
    
    for (int i = 0; i < MAX_FDS; i++) {
        proc->fd_table[i] = NULL;
    }
    
    struct file *con1 = kmalloc(sizeof(struct file));
    if (con1) {
        con1->vnode = console_get_vnode();
        con1->offset = 0;
        con1->flags = 0;
        con1->private_data = NULL;
        proc->fd_table[1] = con1;
    }
    
    struct file *con2 = kmalloc(sizeof(struct file));
    if (con2) {
        con2->vnode = console_get_vnode();
        con2->offset = 0;
        con2->flags = 0;
        con2->private_data = NULL;
        proc->fd_table[2] = con2;
    }
    proc->parent = thread_current() ? thread_current()->process : kernel_process;
    proc->children_head = NULL;
    proc->next_sibling = proc->parent->children_head;
    proc->parent->children_head = proc;
    
    proc->exit_status = 0;
    proc->waiter = NULL;
    
    if (!vmm_create_address_space(&proc->as)) {
        kfree(proc);
        return NULL;
    }
    
    return proc;
}

void process_destroy(struct process* proc) {
    if (!proc || proc == kernel_process) return;
    
    // Remove from parent's children list
    if (proc->parent) {
        struct process **curr = &proc->parent->children_head;
        while (*curr && *curr != proc) {
            curr = &(*curr)->next_sibling;
        }
        if (*curr == proc) {
            *curr = proc->next_sibling;
        }
    }
    
    vmm_destroy_address_space(&proc->as);
    kfree(proc);
}

void process_reparent_children(struct process *proc, struct process *new_parent) {
    if (!proc || !new_parent) return;
    
    struct process *child = proc->children_head;
    while (child) {
        struct process *next = child->next_sibling;
        child->parent = new_parent;
        child->next_sibling = new_parent->children_head;
        new_parent->children_head = child;
        child = next;
    }
    proc->children_head = NULL;
}

void process_exit(struct process *proc, int status) {
    if (!proc || proc == kernel_process) return;
    
    proc->exit_status = status;
    proc->state = PROCESS_ZOMBIE;
    
    // Reparent children to kernel_process (PID 0)
    process_reparent_children(proc, kernel_process);
    
    // Wake up parent if waiting
    if (proc->parent && proc->parent->waiter) {
        thread_wake_waiter(proc->parent->waiter);
        proc->parent->waiter = NULL;
    }
    
    // Kill threads, scheduler reaper will clean them up
    thread_terminate_process(proc);
}

void process_terminate(struct process* proc) {
    process_exit(proc, -1);
}


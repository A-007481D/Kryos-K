#include "../../include/process.h"
#include "../memory/vmm.h"
#include "../../include/heap.h"
#include "../memory/virt.h"
#include "../../include/pmm.h"
#include "../../include/thread.h"
#include "../../include/vfs.h"
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
    
    if (!vmm_create_address_space(&proc->as)) {
        kfree(proc);
        return NULL;
    }
    
    return proc;
}

void process_destroy(struct process* proc) {
    if (!proc || proc == kernel_process) return;
    
    vmm_destroy_address_space(&proc->as);
    kfree(proc);
}

void process_terminate(struct process* proc) {
    if (!proc || proc == kernel_process) return;
    
    // Transition the process to terminated state.
    proc->state = PROCESS_TERMINATED;
    
    // Mark all threads belonging to this process as dead.
    // The thread reaper will free the thread structs on the next schedule(),
    // and when all threads are dead, it will destroy the address space.
    thread_terminate_process(proc);
}

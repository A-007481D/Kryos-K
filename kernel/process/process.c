#include "../../include/process.h"
#include "../memory/vmm.h"
#include "../../include/heap.h"
#include "../memory/virt.h"
#include "../../include/pmm.h"
#include "../../include/thread.h"
#include <stddef.h>

static struct process _kernel_process = {0};
struct process* kernel_process = &_kernel_process;
static pid_t next_pid = 1;

void process_init(void) {
    kernel_process->pid = 0;
    // The kernel process uses the currently active PML4 (which is initialized in boot/VMM)
    kernel_process->as.pml4_phys = vmm_get_current_pml4();
}

struct process* process_create(void) {
    struct process* proc = (struct process*)kmalloc(sizeof(struct process));
    if (!proc) return NULL;
    
    proc->pid = next_pid++;
    proc->state = PROCESS_RUNNING;
    
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

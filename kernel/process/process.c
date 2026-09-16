#include "../../include/process.h"
#include "../memory/vmm.h"
#include "../../include/heap.h"
#include "../memory/virt.h"
#include "../../include/pmm.h"
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
    
    if (!vmm_create_address_space(&proc->as)) {
        kfree(proc);
        return NULL;
    }
    
    return proc;
}

void process_destroy(struct process* proc) {
    if (!proc) return;
    if (proc == kernel_process) return; // Cannot destroy the kernel process
    
    // Destroy the address space (user mappings and page tables)
    vmm_destroy_address_space(&proc->as);
    
    kfree(proc);
}

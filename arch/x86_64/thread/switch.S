// void context_switch(struct thread* old_thread, struct thread* new_thread);
// RDI = old_thread, RSI = new_thread
.global context_switch
context_switch:
    // 1. Save old thread's callee-saved registers
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    
    // 2. Save current stack pointer to old_thread->rsp
    // 'rsp' is at offset 8 in struct thread (id is 8 bytes)
    mov %rsp, 8(%rdi)
    
    // 3. Load new thread's stack pointer
    mov 8(%rsi), %rsp
    
    // 4. Restore new thread's callee-saved registers
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    
    // 5. Resume execution of the new thread
    ret

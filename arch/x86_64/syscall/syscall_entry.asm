; arch/x86_64/syscall/syscall_entry.asm
;
; System Call Entry Point
;
; ABI:
;   RAX = syscall number
;   RDI = arg0
;   RSI = arg1
;   RDX = arg2
;   RAX = return value
;
;   RCX = user RIP (destroyed by hardware)
;   R11 = user RFLAGS (destroyed by hardware)
;   R10 = user RSP (destroyed by entry stub)

global syscall_entry
extern tss
extern syscall_dispatch

section .text
syscall_entry:
    ; 1. Capture user RSP into R10.
    ; We use R10 because it's a caller-saved syscall argument register
    ; (currently unused in our 3-arg ABI) and SYSCALL allows clobbering it.
    mov r10, rsp

    ; 2. Switch to the current thread's kernel stack.
    ; The top of the kernel stack is maintained in TSS.RSP0.
    mov rsp, [rel tss + 4]

    ; 3. Build the syscall frame.
    ; The layout on the kernel stack will be:
    ;   [ RSP + 16 ]  saved user RSP (from R10)
    ;   [ RSP + 8  ]  saved user RIP (from RCX)
    ;   [ RSP + 0  ]  saved user RFLAGS (from R11)
    push r10   ; saved user RSP
    push rcx   ; saved user RIP
    push r11   ; saved user RFLAGS

    ; Save callee-saved registers required by the kernel ABI.
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; Save caller-saved registers so user mode doesn't lose them!
    ; The kernel C function might clobber them.
    push rdi
    push rsi
    push rdx
    push r8
    push r9
    push r10

    ; 4. Setup C arguments and call dispatcher.
    ; C signature: syscall_dispatch(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2, void *frame)
    ; C expects args in RDI, RSI, RDX, RCX, R8.
    ; Incoming ABI has them in RAX, RDI, RSI, RDX.
    ; BUT wait, we just pushed RDI, RSI, RDX, R8, R9, R10!
    ; We can read them from the stack, or just not overwrite them before we need them.
    ; Actually, they are still in their registers because PUSH doesn't change them!
    
    ; frame is at RSP + (6 * 8) = RSP + 48 (since we pushed 6 caller-saved regs)
    ; Wait, no. frame is a pointer to syscall_frame. The frame struct has
    ; r15, r14, r13, r12, rbp, rbx, r11, rcx, r10.
    ; Wait, we just added 6 more registers to the stack. This changes the offset of `frame`!
    ; In Kryos, `struct syscall_frame` is defined in include/syscall.h.
    ; We should pop them before passing frame, or we just pass the correct frame pointer.
    ; Let's just pop them AFTER the call!
    
    ; C expects frame in R8.
    mov r8, rsp
    add r8, 48        ; frame -> C arg 5 (points to r15)
    
    mov rcx, rdx      ; arg2 -> C arg 4
    mov rdx, rsi      ; arg1 -> C arg 3
    mov rsi, rdi      ; arg0 -> C arg 2
    mov rdi, rax      ; nr   -> C arg 1

    call syscall_dispatch

    ; Restore caller-saved registers
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rsi
    pop rdi

    ; 5. Restore registers and return.
    ; The return value from syscall_dispatch is in RAX, exactly where
    ; the user expects it.
    
    ; Restore callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; Restore architectural state for SYSRETQ
    pop r11    ; original user RFLAGS
    pop rcx    ; original user RIP
    pop rsp    ; original user RSP

    ; Return to Ring 3 
    ; SYSRETQ loads CS from STAR[63:48]+16 and SS from STAR[63:48]+8
    ; R11 is restored to RFLAGS
    ; RCX is restored to RIP
    o64 sysret

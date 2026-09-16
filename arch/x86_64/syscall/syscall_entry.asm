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

    ; 4. Setup C arguments and call dispatcher.
    ; C signature: syscall_dispatch(uint64_t nr, uint64_t a0, uint64_t a1, uint64_t a2, void *frame)
    ; C expects args in RDI, RSI, RDX, RCX, R8.
    ; Incoming ABI has them in RAX, RDI, RSI, RDX.
    mov r8, rsp       ; frame -> C arg 5
    mov rcx, rdx      ; arg2 -> C arg 4
    mov rdx, rsi      ; arg1 -> C arg 3
    mov rsi, rdi      ; arg0 -> C arg 2
    mov rdi, rax      ; nr   -> C arg 1

    call syscall_dispatch

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

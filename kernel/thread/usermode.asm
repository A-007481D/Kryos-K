global jump_to_usermode

; void jump_to_usermode(uint64_t rip, uint64_t rsp)
; rdi = user entry point (rip)
; rsi = user stack pointer (rsp)
jump_to_usermode:
    ; Construct the iretq frame
    ; Stack must look like:
    ; SS
    ; RSP
    ; RFLAGS
    ; CS
    ; RIP

    ; SS = 0x1B (User Data)
    push 0x1B
    
    ; RSP
    push rsi
    
    ; RFLAGS (IF=1 -> 0x202)
    push 0x202
    
    ; CS = 0x23 (User Code)
    push 0x23
    
    ; RIP
    push rdi
    
    ; Clear segments to User Data
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Drop to Ring 3!
    iretq

global isr_stub_table
extern fault_handler

%macro ISR_NOERR 1
global isr_stub_%1
isr_stub_%1:
    push 0  ; dummy error code
    push %1 ; vector number
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr_stub_%1
isr_stub_%1:
    ; error code is already pushed by CPU
    push %1 ; vector number
    jmp isr_common
%endmacro

; Define the 32 exception stubs
ISR_NOERR 0   ; Divide Error
ISR_NOERR 1   ; Debug
ISR_NOERR 2   ; NMI
ISR_NOERR 3   ; Breakpoint
ISR_NOERR 4   ; Overflow
ISR_NOERR 5   ; BOUND Range Exceeded
ISR_NOERR 6   ; Invalid Opcode
ISR_NOERR 7   ; Device Not Available
ISR_ERR   8   ; Double Fault
ISR_NOERR 9   ; Coprocessor Segment Overrun
ISR_ERR   10  ; Invalid TSS
ISR_ERR   11  ; Segment Not Present
ISR_ERR   12  ; Stack-Segment Fault
ISR_ERR   13  ; General Protection Fault
ISR_ERR   14  ; Page Fault
ISR_NOERR 15  ; Reserved
ISR_NOERR 16  ; x87 Floating-Point Exception
ISR_ERR   17  ; Alignment Check
ISR_NOERR 18  ; Machine Check
ISR_NOERR 19  ; SIMD Floating-Point Exception
ISR_NOERR 20  ; Virtualization Exception
ISR_ERR   21  ; Control Protection Exception
ISR_NOERR 22  ; Reserved
ISR_NOERR 23  ; Reserved
ISR_NOERR 24  ; Reserved
ISR_NOERR 25  ; Reserved
ISR_NOERR 26  ; Reserved
ISR_NOERR 27  ; Reserved
ISR_NOERR 28  ; Hypervisor Injection Exception
ISR_ERR   29  ; VMM Communication Exception
ISR_ERR   30  ; Security Exception
ISR_NOERR 31  ; Reserved
ISR_NOERR 32  ; IRQ0 (Timer)
ISR_NOERR 33  ; IRQ1 (Keyboard)
ISR_NOERR 34  ; IRQ2
ISR_NOERR 35  ; IRQ3
ISR_NOERR 36  ; IRQ4
ISR_NOERR 37  ; IRQ5
ISR_NOERR 38  ; IRQ6
ISR_NOERR 39  ; IRQ7
ISR_NOERR 40  ; IRQ8
ISR_NOERR 41  ; IRQ9
ISR_NOERR 42  ; IRQ10
ISR_NOERR 43  ; IRQ11
ISR_NOERR 44  ; IRQ12
ISR_NOERR 45  ; IRQ13
ISR_NOERR 46  ; IRQ14
ISR_NOERR 47  ; IRQ15

isr_common:
    ; Push general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to exception_frame_t as first argument (RDI)
    mov rdi, rsp
    
    ; Align stack to 16 bytes as required by System V ABI
    mov rbp, rsp
    and rsp, -16
    
    ; Clear direction flag for C ABI
    cld
    call fault_handler
    
    ; Restore original stack pointer
    mov rsp, rbp

    ; Pop general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Drop vector and error code
    add rsp, 16

    iretq

section .rodata
align 8
isr_stub_table:
    dq isr_stub_0
    dq isr_stub_1
    dq isr_stub_2
    dq isr_stub_3
    dq isr_stub_4
    dq isr_stub_5
    dq isr_stub_6
    dq isr_stub_7
    dq isr_stub_8
    dq isr_stub_9
    dq isr_stub_10
    dq isr_stub_11
    dq isr_stub_12
    dq isr_stub_13
    dq isr_stub_14
    dq isr_stub_15
    dq isr_stub_16
    dq isr_stub_17
    dq isr_stub_18
    dq isr_stub_19
    dq isr_stub_20
    dq isr_stub_21
    dq isr_stub_22
    dq isr_stub_23
    dq isr_stub_24
    dq isr_stub_25
    dq isr_stub_26
    dq isr_stub_27
    dq isr_stub_28
    dq isr_stub_29
    dq isr_stub_30
    dq isr_stub_31
    dq isr_stub_32
    dq isr_stub_33
    dq isr_stub_34
    dq isr_stub_35
    dq isr_stub_36
    dq isr_stub_37
    dq isr_stub_38
    dq isr_stub_39
    dq isr_stub_40
    dq isr_stub_41
    dq isr_stub_42
    dq isr_stub_43
    dq isr_stub_44
    dq isr_stub_45
    dq isr_stub_46
    dq isr_stub_47

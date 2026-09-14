global long_mode_start
extern kernel_main

section .text
bits 64
long_mode_start:
    ; We are now officially in 64-bit long mode!
    
    ; Load data segment registers with the 64-bit data segment selector.
    ; (0x10 is the offset of the data segment in our temporary GDT).
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; EAX and EBX still contain the 32-bit Multiboot2 magic and info struct pointer.
    ; According to the System V AMD64 ABI, the first two arguments are passed in RDI and RSI.
    ; We zero-extend the 32-bit values into the 64-bit registers using mov.
    mov edi, eax
    mov esi, ebx

    ; Call the 64-bit C kernel entry point
    call kernel_main

    ; If kernel_main returns, halt
.halt:
    cli
    hlt
    jmp .halt

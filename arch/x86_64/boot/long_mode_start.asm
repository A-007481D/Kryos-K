global long_mode_start
extern kernel_main
extern pml4_table

section .boot_text
bits 64
long_mode_start:
    ; We are now officially in 64-bit long mode!
    ; However, we are still executing from the low memory identity map.
    ; We must use a 64-bit absolute jump to reach the higher-half mapping.
    
    mov rax, .higher_half
    jmp rax

section .text
bits 64
.higher_half:
    ; We are now executing in the higher half!
    
    ; Load data segment registers with the 64-bit data segment selector.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Zero-extend the 32-bit Multiboot2 values into RDI and RSI.
    mov edi, eax
    mov esi, ebx

    ; Pass the physical address of pml4_table as the 3rd argument (RDX)
    ; so that C code can unmap the lower half.
    ; Since pml4_table is in .boot_bss, its symbol value is the physical address.
    mov edx, pml4_table

    ; Call the 64-bit C kernel entry point
    call kernel_main

    ; If kernel_main returns, halt
.halt:
    cli
    hlt
    jmp .halt

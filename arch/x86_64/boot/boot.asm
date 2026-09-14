global _start
extern kernel_main

section .multiboot2
align 8
header_start:
    dd 0xe85250d6                ; Multiboot2 magic number
    dd 0                         ; Architecture 0 (i386 protected mode)
    dd header_end - header_start ; Header length
    ; Checksum
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))

    ; End tag
    align 8
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
header_end:

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB
stack_top:

section .text
bits 32
_start:
    ; The bootloader has loaded us into 32-bit protected mode.
    ; Set up the stack.
    mov esp, stack_top
    
    ; Multiboot2 bootloader passes:
    ; EAX = magic value (0x36d76289)
    ; EBX = address of the Multiboot2 information structure
    
    ; We push these onto the stack as arguments to kernel_main(magic, info_addr).
    ; In standard 32-bit C calling convention (cdecl), arguments are pushed in reverse order.
    push ebx
    push eax
    
    ; Call the C kernel entry point
    call kernel_main
    
    ; If kernel_main returns, halt the CPU
.halt:
    cli
    hlt
    jmp .halt

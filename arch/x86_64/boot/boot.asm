global _start
extern long_mode_start

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
align 4096
pml4_table:
    resb 4096
pdpt_table:
    resb 4096
pd_table:
    resb 4096

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
    
    ; Multiboot2 bootloader passes EAX (magic) and EBX (info structure addr).
    ; We push them onto the stack to preserve them across CPUID and paging functions,
    ; since those functions will clobber registers.
    push ebx
    push eax
    
    call check_cpuid
    call check_long_mode

    call set_up_page_tables
    call enable_paging

    ; Load the 64-bit GDT
    lgdt [gdt64.pointer]

    ; The transition: Paging is on and we have a 64-bit CS descriptor. 
    ; Execute a far jump to officially enter long mode.
    jmp gdt64.code_segment:long_mode_start

    ; If something goes wrong
.halt:
    cli
    hlt
    jmp .halt

; --- CPUID Checks ---
check_cpuid:
    ; Check if CPUID is supported by attempting to flip the ID bit (bit 21) in EFLAGS
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    hlt
    jmp .no_cpuid

check_long_mode:
    ; Test if extended processor info is available
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    ; Use extended info to test if long mode is available
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    ret
.no_long_mode:
    hlt
    jmp .no_long_mode

; --- Page Tables ---
set_up_page_tables:
    ; 1. Explicitly zero out the page table memory (since it is in .bss)
    mov edi, pml4_table
    xor eax, eax
    mov ecx, 4096 * 3 / 4 ; 3 tables * 4096 bytes / 4 bytes per stosd
    rep stosd

    ; 2. Build PML4 -> PDPT -> PD
    ; The page tables must be 4K aligned (which they are, via align 4096).
    ; We set the Present (bit 0) and Read/Write (bit 1) flags.
    mov eax, pdpt_table
    or eax, 0b11
    mov [pml4_table], eax

    mov eax, pd_table
    or eax, 0b11
    mov [pdpt_table], eax

    ; 3. Build PD[0..511] to identity-map the first 1 GiB using 2 MiB huge pages.
    mov ecx, 0         ; Counter

.map_pd_table:
    ; The physical address is ecx * 2 MiB
    mov eax, 0x200000  ; 2 MiB
    mul ecx
    ; Set Present (bit 0), Read/Write (bit 1), and Page Size (bit 7)
    or eax, 0b10000011
    mov [pd_table + ecx * 8], eax

    inc ecx
    cmp ecx, 512       ; 512 entries in the table
    jne .map_pd_table

    ret

enable_paging:
    ; 1. Load CR3 with the physical address of the PML4
    mov eax, pml4_table
    mov cr3, eax

    ; 2. Enable PAE (Physical Address Extension) in CR4
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; 3. Enable LME (Long Mode Enable) in EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; 4. Enable Paging in CR0
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret

; --- Temporary Bootstrap GDT ---
section .rodata
align 8
gdt64:
    dq 0 ; null descriptor
.code_segment: equ $ - gdt64
    ; 64-bit code segment: Executable (43), Descriptor type (44), Present (47), 64-bit flag (53)
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
.data_segment: equ $ - gdt64
    ; 64-bit data segment: Writable (41), Descriptor type (44), Present (47)
    dq (1 << 41) | (1 << 44) | (1 << 47)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

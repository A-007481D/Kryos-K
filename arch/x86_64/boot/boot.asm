global _start
global pml4_table
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

section .boot_bss nobits
align 4096
pml4_table:
    resb 4096
pdpt_table:
    resb 4096
kernel_pdpt_table:
    resb 4096
pd_table:
    resb 4096

align 16
stack_bottom:
    resb 16384 ; 16 KiB
stack_top:

section .boot_text
bits 32
_start:
    ; Set up the stack.
    mov esp, stack_top
    
    ; Save Multiboot2 EAX and EBX
    push ebx
    push eax
    
    call check_cpuid
    call check_long_mode

    call set_up_page_tables
    call enable_paging

    ; Load the 64-bit GDT
    lgdt [gdt64.pointer]

    ; Far jump to 64-bit code segment
    jmp gdt64.code_segment:long_mode_start

.halt:
    cli
    hlt
    jmp .halt

; --- CPUID Checks ---
check_cpuid:
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
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

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
    ; 1. Zero out the page table memory (4 tables)
    mov edi, pml4_table
    xor eax, eax
    mov ecx, 4096 * 4 / 4 
    rep stosd

    ; 2. Build PML4
    ; PML4[0] -> pdpt_table (Identity Map for lower half)
    mov eax, pdpt_table
    or eax, 0b11
    mov [pml4_table], eax

    ; PML4[511] -> kernel_pdpt_table (Higher Half)
    mov eax, kernel_pdpt_table
    or eax, 0b11
    mov [pml4_table + 511 * 8], eax

    ; 3. Build PDPTs
    ; pdpt_table[0] -> pd_table (Identity Map)
    mov eax, pd_table
    or eax, 0b11
    mov [pdpt_table], eax

    ; kernel_pdpt_table[510] -> pd_table (Higher Half: 0xFFFFFFFF80000000)
    mov eax, pd_table
    or eax, 0b11
    mov [kernel_pdpt_table + 510 * 8], eax

    ; 4. Build PD[0..511] to map the first 1 GiB of physical memory using 2 MiB huge pages.
    mov ecx, 0

.map_pd_table:
    mov eax, 0x200000  ; 2 MiB
    mul ecx
    ; Set Present (bit 0), Read/Write (bit 1), and Page Size (bit 7)
    or eax, 0b10000011
    mov [pd_table + ecx * 8], eax

    inc ecx
    cmp ecx, 512
    jne .map_pd_table

    ret

enable_paging:
    mov eax, pml4_table
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret

; --- Temporary Bootstrap GDT ---
section .boot_rodata
align 8
gdt64:
    dq 0 ; null descriptor
.code_segment: equ $ - gdt64
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
.data_segment: equ $ - gdt64
    dq (1 << 41) | (1 << 44) | (1 << 47)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

ENTRY(_start)

SECTIONS
{
    /* The kernel will live at 1MB in physical memory, per Multiboot2 conventions */
    . = 1M;

    _kernel_start = .;

    /* Multiboot2 header MUST come very early (in first 32 KiB) and be 64-bit aligned */
    .multiboot2 ALIGN(8) :
    {
        KEEP(*(.multiboot2))
    }

    .text ALIGN(4K) :
    {
        *(.text)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata)
    }

    .data ALIGN(4K) :
    {
        *(.data)
    }

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
    }

    _kernel_end = .;
}

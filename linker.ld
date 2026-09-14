ENTRY(_start)

SECTIONS
{
    /* Physical load address (LMA) */
    . = 1M;
    _kernel_phys_start = .;

    /* Boot sections execute in physical memory before paging is fully active */
    .boot ALIGN(4K) :
    {
        KEEP(*(.multiboot2))
        *(.boot_text)
        *(.boot_rodata)
        *(.boot_data)
        *(.boot_bss)
    }

    /* Move the virtual address (VMA) to the higher half (-2 GiB) */
    . += 0xFFFFFFFF80000000;

    _kernel_start = .;

    /* Kernel sections execute in higher-half virtual memory */
    /* AT() maps them consecutively in the physical ELF load image */
    .text ALIGN(4K) : AT(ADDR(.text) - 0xFFFFFFFF80000000)
    {
        *(.text)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xFFFFFFFF80000000)
    {
        *(.rodata)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - 0xFFFFFFFF80000000)
    {
        *(.data)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - 0xFFFFFFFF80000000)
    {
        *(COMMON)
        *(.bss)
    }

    _kernel_end = .;
}

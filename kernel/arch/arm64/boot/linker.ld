OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

KERNEL_RAM_BASE = 0x40200000;
KERNEL_RAM_SIZE = 0x0FE00000;
KERNEL_STACK_SIZE = 0x4000;

MEMORY
{
    RAM (rwx) : ORIGIN = KERNEL_RAM_BASE,
               LENGTH = KERNEL_RAM_SIZE
}

SECTIONS
{
    . = ORIGIN(RAM);

    __kernel_start = .;

    .text :
    {
        . = ALIGN(16);

        KEEP(*(.text.boot))

        *(.text)
        *(.text.*)
    } > RAM

    .rodata :
    {
        . = ALIGN(16);

        *(.rodata)
        *(.rodata.*)
    } > RAM

    .data :
    {
        . = ALIGN(16);

        *(.data)
        *(.data.*)
    } > RAM

    .bss (NOLOAD) :
    {
        . = ALIGN(8);

        __bss_start = .;

        *(.bss)
        *(.bss.*)
        *(COMMON)

        . = ALIGN(8);

        __bss_end = .;
    } > RAM

    .stack (NOLOAD) :
    {
        . = ALIGN(16);

        __stack_bottom = .;

        . += KERNEL_STACK_SIZE;

        . = ALIGN(16);

        __stack_top = .;
    } > RAM

    __kernel_end = .;

    ASSERT(
        __kernel_end <= ORIGIN(RAM) + LENGTH(RAM),
        "SyntraOS kernel exceeds available RAM"
    )
}

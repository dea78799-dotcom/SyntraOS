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

/*
 * Keep executable code, read-only data and writable data in
 * separate ELF LOAD segments. This avoids a single RWX segment.
 */
PHDRS
{
    text   PT_LOAD FLAGS(5); /* Read + Execute */
    rodata PT_LOAD FLAGS(4); /* Read only */
    data   PT_LOAD FLAGS(6); /* Read + Write */
}

SECTIONS
{
    . = ORIGIN(RAM);

    __kernel_start = .;

    .text ALIGN(4096) :
    {
        __text_start = .;

        KEEP(*(.text.boot))

        *(.text)
        *(.text.*)

        . = ALIGN(4096);
    } > RAM :text

    /* Dedicated page containing position-independent EL0 test instructions. */
    .user_text ALIGN(4096) :
    {
        __user_text_start = .;
        KEEP(*(.user.text))
        . = ALIGN(4096);
        __user_text_end = .;
    } > RAM :text
    __text_end = .;

    .rodata ALIGN(4096) :
    {
        __rodata_start = .;

        *(.rodata)
        *(.rodata.*)

        . = ALIGN(4096);
        __rodata_end = .;
    } > RAM :rodata

    .data ALIGN(4096) :
    {
        __data_start = .;

        *(.data)
        *(.data.*)

        . = ALIGN(4096);
        __data_end = .;
    } > RAM :data

    .bss ALIGN(4096) (NOLOAD) :
    {
        __bss_start = .;

        *(.bss)
        *(.bss.*)
        *(COMMON)

        . = ALIGN(4096);
        __user_stack_bottom = .;
        . += 4096;
        __user_stack_top = .;

        . = ALIGN(4096);
        __bss_end = .;
    } > RAM :data

    .stack ALIGN(4096) (NOLOAD) :
    {
        . = ALIGN(16);

        __stack_bottom = .;

        . += KERNEL_STACK_SIZE;

        . = ALIGN(16);

        __stack_top = .;
    } > RAM :data

    __kernel_end = .;

    ASSERT(__kernel_start < __kernel_end,
           "SyntraOS kernel range is empty")
    ASSERT((__stack_bottom & 0xF) == 0,
           "SyntraOS stack bottom must be 16-byte aligned")
    ASSERT((__stack_top & 0xF) == 0,
           "SyntraOS stack top must be 16-byte aligned")
    ASSERT(__stack_bottom < __stack_top,
           "SyntraOS stack range is invalid")
    ASSERT(__kernel_end <= ORIGIN(RAM) + LENGTH(RAM),
           "SyntraOS kernel exceeds available RAM")
}

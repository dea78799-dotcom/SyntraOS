.section .text.boot, "ax"
.global _start
.type _start, %function

/*
 * SyntraOS ARM64 Kernel Entry
 * Version: 1.0.0
 *
 * Initial bare-metal entry for QEMU virt.
 */

_start:
    /* Mask IRQ, FIQ, SError and Debug exceptions. */
    msr daifset, #0xf

    /* Read the initial exception level. */
    mrs x19, CurrentEL

    /* EL0 is not supported by this entry point. */
    lsr x20, x19, #2
    cbz x20, .Lhalt

    /* Set the initial stack pointer. */
    adrp x2, __stack_top
    add x2, x2, :lo12:__stack_top
    mov sp, x2

    /* Clear the BSS section. */
    adrp x2, __bss_start
    add x2, x2, :lo12:__bss_start

    adrp x3, __bss_end
    add x3, x3, :lo12:__bss_end

.Lclear_bss:
    cmp x2, x3
    b.hs .Lbss_done

    str xzr, [x2], #8
    b .Lclear_bss

.Lbss_done:
    /* QEMU virt bare-metal DTB address. */
    movz x0, #0x4000, lsl #16

    /* Pass the raw CurrentEL value to C. */
    mov x1, x19

    /* Enter the ARM64 C boot stage. */
    bl syntra_arm64_boot_main

.Lhalt:
    wfe
    b .Lhalt

.size _start, . - _start

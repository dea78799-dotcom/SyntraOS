.section .text.boot, "ax"
.global _start
.type _start, %function

/*
 * SyntraOS ARM64 Kernel Entry
 * Version: 1.1.0
 *
 * Initial bare-metal entry for QEMU virt.
 */

_start:
    /* Mask IRQ, FIQ, SError and Debug exceptions. */
    msr daifset, #0xf

    /* This entry currently supports EL1 only. */
    mrs x19, CurrentEL
    cmp x19, #0x4
    b.ne .Lhalt

    /* Disable FP/SIMD access until per-thread context save/restore exists. */
    mrs x2, cpacr_el1
    mov x3, #3
    lsl x3, x3, #20
    bic x2, x2, x3
    msr cpacr_el1, x2
    isb

    /* Set the initial stack pointer. */
    adrp x2, __stack_top
    add x2, x2, :lo12:__stack_top
    mov sp, x2

    /*
     * Install the EL1 exception vector table before entering C.
     * The table is 2 KiB aligned in vectors.S.
     */
    adrp x2, syntra_arm64_vectors
    add x2, x2, :lo12:syntra_arm64_vectors
    msr vbar_el1, x2
    isb

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

    /* GDB-visible return point for the QEMU boot smoke test. */
    .global syntra_arm64_boot_returned
syntra_arm64_boot_returned:

.Lhalt:
    msr daifset, #0xf
    wfe
    b .Lhalt

.size _start, . - _start

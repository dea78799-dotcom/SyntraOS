/*
 * SyntraOS ARM64 Exception Vector Table
 * Version: 1.1.0
 *
 * EL1 vector table with a common register-context capture entry.
 *
 * IRQ dispatch and exception return are intentionally deferred until
 * the GIC and a complete interrupt/exception policy are implemented.
 */

.section .text.vectors, "ax"
.balign 2048

.global syntra_arm64_vectors
.type syntra_arm64_vectors, %function

syntra_arm64_vectors:
    /* Current EL with SP0: synchronous, IRQ, FIQ, SError. */
    b syntra_arm64_exception_entry
    .space 124
    b syntra_arm64_exception_entry
    .space 124
    b syntra_arm64_exception_entry
    .space 124
    b syntra_arm64_exception_entry
    .space 124

    /* Current EL with SPx: synchronous, IRQ, FIQ, SError. */
    b syntra_arm64_exception_entry
    .space 124
    b syntra_arm64_exception_entry
    .space 124
    b syntra_arm64_exception_entry
    .space 124
    b syntra_arm64_exception_entry
    .space 124

    /* Lower EL using AArch64: synchronous, IRQ, FIQ, SError. */
    b syntra_arm64_exception_entry
    .space 124
    b syntra_arm64_exception_entry
    .space 124
    b syntra_arm64_exception_entry
    .space 124
    b syntra_arm64_exception_entry
    .space 124

    /* Lower EL using AArch32: synchronous, IRQ, FIQ, SError. */
    b syntra_arm64_exception_entry
    .space 124
    b syntra_arm64_exception_entry
    .space 124
    b syntra_arm64_exception_entry
    .space 124
    b syntra_arm64_exception_entry
    .space 124

.size syntra_arm64_vectors, . - syntra_arm64_vectors

/*
 * Exception context layout must match syntra_arm64_exception_context_t:
 *
 *   x0-x30   0..240
 *   SP_EL0      248
 *   ELR_EL1     256
 *   SPSR_EL1    264
 *   ESR_EL1     272
 *   FAR_EL1     280
 *
 * Total frame size: 288 bytes (16-byte aligned).
 */
.global syntra_arm64_exception_entry
.global syntra_arm64_exception_unhandled
.type syntra_arm64_exception_entry, %function
.type syntra_arm64_exception_unhandled, %function

syntra_arm64_exception_unhandled:
syntra_arm64_exception_entry:
    /* Mask exceptions before touching the exception frame. */
    msr daifset, #0xf

    /* Allocate the ABI-defined, 16-byte-aligned context frame. */
    sub sp, sp, #288

    /* Preserve every general-purpose register before using scratch regs. */
    stp x0,  x1,  [sp, #0]
    stp x2,  x3,  [sp, #16]
    stp x4,  x5,  [sp, #32]
    stp x6,  x7,  [sp, #48]
    stp x8,  x9,  [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30, [sp, #240]

    /* Capture architectural exception state. */
    mrs x9, sp_el0
    str x9, [sp, #248]

    mrs x9, elr_el1
    str x9, [sp, #256]

    mrs x9, spsr_el1
    str x9, [sp, #264]

    mrs x9, esr_el1
    str x9, [sp, #272]

    mrs x9, far_el1
    str x9, [sp, #280]

    /*
     * Keep the captured frame on the exception stack for diagnosis.
     * Do not ERET or enable IRQs until a real dispatcher and return path
     * are implemented and validated.
     */
.Lexception_halt:
    wfe
    b .Lexception_halt

.size syntra_arm64_exception_entry, . - syntra_arm64_exception_entry
.size syntra_arm64_exception_unhandled, . - syntra_arm64_exception_unhandled

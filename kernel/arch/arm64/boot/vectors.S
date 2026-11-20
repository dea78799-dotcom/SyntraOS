/*
 * SyntraOS ARM64 Exception Vector Table
 * Version: 1.0.0
 *
 * EL1 vector table foundation.
 *
 * All exception classes currently enter a masked fatal handler.
 * IRQ dispatch is intentionally deferred until the GIC and a
 * complete interrupt acknowledgement path are implemented.
 */

.section .text.vectors, "ax"
.balign 2048

.global syntra_arm64_vectors
.type syntra_arm64_vectors, %function

syntra_arm64_vectors:
    /* Current EL with SP0: synchronous, IRQ, FIQ, SError. */
    b syntra_arm64_exception_unhandled
    .space 124
    b syntra_arm64_exception_unhandled
    .space 124
    b syntra_arm64_exception_unhandled
    .space 124
    b syntra_arm64_exception_unhandled
    .space 124

    /* Current EL with SPx: synchronous, IRQ, FIQ, SError. */
    b syntra_arm64_exception_unhandled
    .space 124
    b syntra_arm64_exception_unhandled
    .space 124
    b syntra_arm64_exception_unhandled
    .space 124
    b syntra_arm64_exception_unhandled
    .space 124

    /* Lower EL using AArch64: synchronous, IRQ, FIQ, SError. */
    b syntra_arm64_exception_unhandled
    .space 124
    b syntra_arm64_exception_unhandled
    .space 124
    b syntra_arm64_exception_unhandled
    .space 124
    b syntra_arm64_exception_unhandled
    .space 124

    /* Lower EL using AArch32: synchronous, IRQ, FIQ, SError. */
    b syntra_arm64_exception_unhandled
    .space 124
    b syntra_arm64_exception_unhandled
    .space 124
    b syntra_arm64_exception_unhandled
    .space 124
    b syntra_arm64_exception_unhandled
    .space 124

.size syntra_arm64_vectors, . - syntra_arm64_vectors

.global syntra_arm64_exception_unhandled
.type syntra_arm64_exception_unhandled, %function

syntra_arm64_exception_unhandled:
    /* Keep all exception classes masked while no dispatcher exists. */
    msr daifset, #0xf

.Lexception_halt:
    wfe
    b .Lexception_halt

.size syntra_arm64_exception_unhandled, . - syntra_arm64_exception_unhandled

/*
 * SyntraOS ARM64 Exception Vector Table
 * Version: 1.1.0
 *
 * EL1 vector table with a common register-context capture and restore path.
 *
 * The common entry saves x0-x30 and architectural return state, calls the
 * C exception/scheduler bridge, then restores either the interrupted frame
 * or the selected thread's saved frame before ERET.
 */

.section .text.vectors, "ax"
.balign 2048

.global syntra_arm64_vectors
.type syntra_arm64_vectors, %function

syntra_arm64_vectors:
    b .Lvector_0
    .space 124
    b .Lvector_1
    .space 124
    b .Lvector_2
    .space 124
    b .Lvector_3
    .space 124
    b .Lvector_4
    .space 124
    b .Lvector_5
    .space 124
    b .Lvector_6
    .space 124
    b .Lvector_7
    .space 124
    b .Lvector_8
    .space 124
    b .Lvector_9
    .space 124
    b .Lvector_10
    .space 124
    b .Lvector_11
    .space 124
    b .Lvector_12
    .space 124
    b .Lvector_13
    .space 124
    b .Lvector_14
    .space 124
    b .Lvector_15
    .space 124

.size syntra_arm64_vectors, . - syntra_arm64_vectors

/*
 * Each vector stub preserves the interrupted x0 in a 16-byte scratch slot
 * and passes its architectural vector index in x0 to the common entry.
 */
.Lvector_0:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #0
    b syntra_arm64_exception_entry

.Lvector_1:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #1
    b syntra_arm64_exception_entry

.Lvector_2:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #2
    b syntra_arm64_exception_entry

.Lvector_3:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #3
    b syntra_arm64_exception_entry

.Lvector_4:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #4
    b syntra_arm64_exception_entry

.Lvector_5:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #5
    b syntra_arm64_exception_entry

.Lvector_6:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #6
    b syntra_arm64_exception_entry

.Lvector_7:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #7
    b syntra_arm64_exception_entry

.Lvector_8:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #8
    b syntra_arm64_exception_entry

.Lvector_9:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #9
    b syntra_arm64_exception_entry

.Lvector_10:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #10
    b syntra_arm64_exception_entry

.Lvector_11:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #11
    b syntra_arm64_exception_entry

.Lvector_12:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #12
    b syntra_arm64_exception_entry

.Lvector_13:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #13
    b syntra_arm64_exception_entry

.Lvector_14:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #14
    b syntra_arm64_exception_entry

.Lvector_15:
    msr daifset, #0xf
    sub sp, sp, #16
    str x0, [sp]
    mov x0, #15
    b syntra_arm64_exception_entry

/*
 * Context layout must match syntra_arm64_exception_context_t:
 *   x0-x30       0..240
 *   SP_EL0          248
 *   ELR_EL1         256
 *   SPSR_EL1        264
 *   ESR_EL1         272
 *   FAR_EL1         280
 *   vector_index    288
 *   reserved        296
 *
 * The 304-byte frame is 16-byte aligned. The interrupted x0 is in the
 * scratch slot immediately above it, at [sp, #304].
 */
.global syntra_arm64_exception_entry
.global syntra_arm64_exception_unhandled
.type syntra_arm64_exception_entry, %function
.type syntra_arm64_exception_unhandled, %function

syntra_arm64_exception_unhandled:
syntra_arm64_exception_entry:
    msr daifset, #0xf
    sub sp, sp, #304

    /* Save vector index, recover original x0, then preserve x1-x30. */
    str x0, [sp, #288]
    ldr x0, [sp, #304]
    str x0, [sp, #0]
    str x1, [sp, #8]
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

    /* Clear the reserved ABI slot. */
    str xzr, [sp, #296]

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
     * Pass the saved frame to the exception and scheduler bridge.
     * The bridge may return a different valid frame when a thread switch
     * is required. IRQ/FIQ/SError policy remains owned by the C layer.
     */
    /*
     * Keep the outgoing frame address in a callee-saved register. The C
     * bridge preserves x19, and the selected frame restores the real x19
     * value before ERET.
     */
    mov x19, sp
    mov x0, sp
    bl syntra_arm64_exception_handle_and_select
    cbz x0, .Lexception_use_current_frame
    cmp x0, x19
    b.eq .Lexception_use_current_frame
    mov sp, x0

    /*
     * The outgoing stack is now inactive. This hook runs on the successor
     * stack and only publishes a bounded switch-away event.
     */
    bl syntra_arm64_thread_switch_complete

.Lexception_use_current_frame:
    /* Return only from validated SVC or acknowledged IRQ handlers. */
    ldr x9, [sp, #296]
    movz x10, #0x5254
    movk x10, #0x5241, lsl #16
    movk x10, #0x4e54, lsl #32
    movk x10, #0x5359, lsl #48
    cmp x9, x10
    b.eq .Lexception_return
    movz x10, #0x5552
    movk x10, #0x4554, lsl #16
    movk x10, #0x5152, lsl #32
    movk x10, #0x4952, lsl #48
    cmp x9, x10
    b.ne .Lexception_halt

.Lexception_return:

    /* Restore the saved EL0 stack pointer before ERET. */
    ldr x9, [sp, #248]
    msr sp_el0, x9
    ldr x9, [sp, #264]
    msr spsr_el1, x9
    ldr x9, [sp, #256]
    msr elr_el1, x9

    ldp x0,  x1,  [sp, #0]
    ldp x2,  x3,  [sp, #16]
    ldp x4,  x5,  [sp, #32]
    ldp x6,  x7,  [sp, #48]
    ldp x8,  x9,  [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    ldr x30, [sp, #240]
    add sp, sp, #320
    eret

.Lexception_halt:
    wfe
    b .Lexception_halt

.size syntra_arm64_exception_entry, . - syntra_arm64_exception_entry
.size syntra_arm64_exception_unhandled, . - syntra_arm64_exception_unhandled

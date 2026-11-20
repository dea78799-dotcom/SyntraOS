.section .text.user_entry, "ax"
.align 4
.global syntra_arm64_enter_user_context
.type syntra_arm64_enter_user_context, %function
/* x0 = prepared frame; x1 = dedicated EL1 exception stack top. */
syntra_arm64_enter_user_context:
    /* Keep asynchronous exceptions masked until the first EL0 instruction. */
    msr daifset, #0xf
    isb
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!
    stp x29, x30, [sp, #-16]!
    adrp x9, syntra_el0_saved_kernel_sp
    add x9, x9, :lo12:syntra_el0_saved_kernel_sp
    mov x10, sp
    str x10, [x9]

    mov x18, x0
    ldr x9, [x18, #248]
    msr sp_el0, x9
    ldr x9, [x18, #264]
    msr spsr_el1, x9
    ldr x9, [x18, #256]
    msr elr_el1, x9
    mov sp, x1
    isb
    ldp x0, x1, [x18, #0]
    ldp x2, x3, [x18, #16]
    ldp x4, x5, [x18, #32]
    ldp x6, x7, [x18, #48]
    ldp x8, x9, [x18, #64]
    ldp x10, x11, [x18, #80]
    ldp x12, x13, [x18, #96]
    ldp x14, x15, [x18, #112]
    ldr x16, [x18, #128]
    ldr x17, [x18, #136]
    ldp x19, x20, [x18, #152]
    ldp x21, x22, [x18, #168]
    ldp x23, x24, [x18, #184]
    ldp x25, x26, [x18, #200]
    ldp x27, x28, [x18, #216]
    ldp x29, x30, [x18, #232]
    ldr x18, [x18, #144]
    eret
.size syntra_arm64_enter_user_context, . - syntra_arm64_enter_user_context

.align 4
.global syntra_arm64_el0_return_trampoline
.type syntra_arm64_el0_return_trampoline, %function
syntra_arm64_el0_return_trampoline:
    adrp x9, syntra_el0_saved_kernel_sp
    add x9, x9, :lo12:syntra_el0_saved_kernel_sp
    ldr x9, [x9]
    mov sp, x9
    ldp x29, x30, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ret
.size syntra_arm64_el0_return_trampoline, . - syntra_arm64_el0_return_trampoline

.section .bss
.align 3
.global syntra_el0_saved_kernel_sp
syntra_el0_saved_kernel_sp:
    .zero 8

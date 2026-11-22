.section .user.text, "ax"
.align 4
.global syntra_arm64_user_hello
.type syntra_arm64_user_hello, %function
syntra_arm64_user_hello:
    mov x0, #0
    svc #0x44
    svc #0x45
.Luser_hello_idle:
    wfe
    b .Luser_hello_idle
.size syntra_arm64_user_hello, . - syntra_arm64_user_hello

.align 4
.global syntra_arm64_user_fault_halt
.type syntra_arm64_user_fault_halt, %function
syntra_arm64_user_fault_halt:
.Luser_fault_idle:
    wfe
    b .Luser_fault_idle
.size syntra_arm64_user_fault_halt, . - syntra_arm64_user_fault_halt

.align 4
.global syntra_arm64_user_write_code_fault
.type syntra_arm64_user_write_code_fault, %function
syntra_arm64_user_write_code_fault:
    adr x0, .Luser_write_fault_target
    mov x1, #1
.Luser_write_fault_target:
    str x1, [x0]
    svc #0x45
.Luser_write_fault_idle:
    wfe
    b .Luser_write_fault_idle
.size syntra_arm64_user_write_code_fault, . - syntra_arm64_user_write_code_fault

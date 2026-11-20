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

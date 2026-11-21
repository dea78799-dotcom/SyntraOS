# SyntraOS Scheduler

Version: 1.1.0
Status: Round-robin scheduling and ARM64 context switching are implemented and QEMU-validated; lifecycle and production integration remain in progress.

## 1. Responsibilities

The scheduler tracks runnable threads, selects the next thread, and maintains
thread lifecycle state. Architecture code saves and restores the integer CPU
exception frame. Stack allocation is owned by the memory subsystem.

## 2. Thread Control Block

Each TCB contains:
- Thread ID and priority metadata.
- READY, RUNNING, BLOCKED, or TERMINATED state.
- Entry function and argument.
- Caller-owned stack base and size.
- Architecture-owned saved CPU context pointer.

Thread creation currently accepts a caller-provided stack. Dynamic stack
allocation is intentionally deferred until Memory Management provides it.

## 3. Scheduling policy

Scheduler V1 uses deterministic round-robin selection across READY threads.
The priority field is retained as metadata but does not influence V1 ordering.
The ARM64 context-aware selector skips READY threads that have no saved CPU
context, preventing selection of metadata-only tasks that cannot be restored.

## 4. Lifecycle

- READY: eligible for CPU time.
- RUNNING: selected as the current thread.
- BLOCKED: temporarily ineligible until explicitly woken.
- TERMINATED: no longer eligible for scheduling.

## 5. Architecture integration

The ARM64 port saves and restores the integer exception frame and performs
thread switching through the exception-return path. New-thread frames are
initialized with a valid entry PC, stack, processor state, and ABI-aligned
stack pointer. QEMU currently validates timer-driven alternation between two
threads.

## 6. FP/SIMD context policy

The ARM64 exception frame does not currently save/restore V0-V31, FPCR, or
FPSR. Until a complete per-thread FP/SIMD context implementation is added:

- Kernel C is compiled with -mgeneral-regs-only.
- Boot clears CPACR_EL1.FPEN so EL0 and EL1 cannot use FP/SIMD instructions.
- CI checks the compile flag and QEMU boot verifies the CPACR policy.
- Programs that need FP/SIMD are not supported yet; access should trap rather
  than corrupt another thread's state.

This is an intentional temporary safety policy, not full FP/SIMD support.

## 7. Validation status

Host tests cover scheduler ordering, state transitions, context-aware
selection, and thread metadata. QEMU validates ARM64 exception return, timer
interrupts, and thread alternation. Thread termination/idle behavior,
FP/SIMD context preservation, and broader production lifecycle integration
remain open.

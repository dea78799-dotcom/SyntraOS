# SyntraOS Scheduler

Version: 1.0.0
Status: Core implementation in progress; ARM64 context switching pending.

## 1. Responsibilities

The scheduler tracks runnable threads, selects the next thread, and maintains
thread lifecycle state. Architecture code is responsible for saving and
restoring CPU registers. Stack allocation is owned by the memory subsystem.

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
A scheduling operation moves the previous RUNNING thread back to READY unless
it has been blocked or terminated, then selects the next READY thread.

## 4. Lifecycle

- READY: eligible for CPU time.
- RUNNING: selected as the current thread.
- BLOCKED: temporarily ineligible until explicitly woken.
- TERMINATED: no longer eligible for scheduling.

## 5. Architecture integration

The generic scheduler does not manipulate ARM64 registers. The ARM64 port must
save the complete exception frame into the current TCB and restore the selected
thread's frame before exception return. New-thread frames must be initialized
with a valid entry PC, stack, processor state, and ABI-aligned stack pointer.

## 6. Validation status

The host test suite covers round-robin order, state transitions, duplicate IDs,
caller-provided stack validation, and scheduler invariants.

Preemptive ARM64 switching, idle-thread behavior, and QEMU task alternation are
not complete until the architecture context-switch path is implemented and
verified.

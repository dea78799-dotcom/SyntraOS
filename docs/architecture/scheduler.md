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
allocation is intentionally deferred until Memory Management provides it. The
Scheduler does not own or free caller-provided stack storage.

## 3. Scheduling policy

Scheduler V1 uses deterministic round-robin selection across READY threads.
The priority field is retained as metadata but does not influence V1 ordering.
A scheduling operation moves the previous RUNNING thread back to READY unless
it has been blocked or terminated, then selects the next READY thread.

## 4. Lifecycle

- READY: eligible for CPU time.
- RUNNING: selected as the current thread.
- BLOCKED: temporarily ineligible until explicitly woken.
- TERMINATED: no longer eligible for scheduling; its TCB remains reserved until explicit reaping.

## 5. Safe reclamation

`scheduler_reap_terminated(thread_id)` releases a TERMINATED thread's Scheduler
slot only when it is no longer the current thread. It clears the saved CPU
context and all stack metadata, then makes the slot available for reuse.

The reaper does not free kernel or user stack memory: those buffers remain
owned by the creator. The creator may release or reuse them only after reaping
succeeds. External owners such as the Process Manager must first drop their
references to the Thread ID. In particular, release the terminated Process
record before reaping its associated Thread records. No automatic reaping is
performed from IRQ or exception paths.

The reported thread count includes terminated records that have not yet been
reaped. This keeps a Thread ID reserved until its external references can be
removed deliberately.

## 6. Architecture integration

The generic scheduler does not manipulate ARM64 registers. The ARM64 port must
save the complete exception frame into the current TCB and restore the selected
thread's frame before exception return. New-thread frames must be initialized
with a valid entry PC, stack, processor state, and ABI-aligned stack pointer.

## 7. Validation status

The host test suite covers round-robin order, state transitions, duplicate IDs,
caller-provided stack validation, explicit terminated-thread reaping, slot reuse,
full-table exhaustion/recovery, and scheduler invariants.

Preemptive ARM64 switching, idle-thread behavior, and QEMU task alternation are
not complete until the architecture context-switch path is implemented and
verified.

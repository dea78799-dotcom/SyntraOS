# SyntraOS Process Manager

Version: 0.1.0
Status: Process-to-thread ownership prototype; address-space and resource lifecycle integration pending.

## 1. Overview

The Process Manager maintains a fixed-capacity process table and associates each
process with scheduler threads. The current implementation provides process
creation and lookup, thread ownership metadata, aggregate process-state
calculation, and consistency validation.

This is a kernel-side metadata prototype. It is not yet a complete isolated
user-process implementation.

## 2. Implemented in the current prototype

- Fixed capacity of 64 process records.
- Non-zero process IDs and duplicate-ID rejection.
- Up to 32 scheduler thread IDs recorded per process.
- A scheduler thread may be attached to only one process.
- Attachment requires an existing scheduler thread and a non-terminated process.
- Process state can be refreshed from the states of its attached scheduler threads.
- Host tests cover multiple threads, duplicate ownership rejection, mixed thread
  states, process termination preconditions, and process/scheduler invariants.

## 3. Aggregate process-state rules

For a process with attached threads, `process_refresh_state()` derives the
process state using this precedence:

1. RUNNING, if at least one thread is RUNNING.
2. READY, otherwise if at least one thread is READY.
3. BLOCKED, otherwise if at least one thread is BLOCKED.
4. TERMINATED, only if every attached thread is TERMINATED.

This precedence describes the aggregate process state; it does not change any
scheduler thread. `process_set_state()` validates a requested state against
the derived scheduler state and rejects mismatches. A process with attached
threads can become TERMINATED only after all of its threads are terminated.

A process with no attached threads retains its metadata state. Thread creation,
stack allocation, scheduling, and state transitions remain Scheduler or
architecture responsibilities.

## 4. Not yet implemented

The process record does not yet own or manage:

- A protected virtual address space or page-table root.
- User and kernel resource handles or automatic resource cleanup.
- IPC endpoints or capability permissions.
- Exit status, parent/child relationships, or process wait semantics.
- Transactional process/thread creation and rollback.
- Per-process memory accounting or isolation enforcement.

The current thread association is an ownership record only. It does not allocate
threads, transfer stack ownership, perform context switching, or reclaim stacks.

## 5. Required integration sequence

1. Define process address-space ownership and its interaction with the ARM64 MMU.
2. Add explicit resource ownership and teardown with failure rollback.
3. Define process creation and thread creation as a coordinated lifecycle.
4. Add IPC endpoints and capability-based access checks.
5. Add tests for allocation failures, teardown ordering, and isolation boundaries.
6. Add QEMU validation after host-side lifecycle contracts are stable.

## 6. Safety and correctness requirements

- Reject invalid and duplicate process identifiers.
- Reject duplicate thread attachment, including cross-process ownership conflicts.
- Do not mark a process TERMINATED while any associated thread can still run.
- Do not reclaim stacks, page tables, or IPC resources while an execution context
  can still reference them.
- Failed process creation must leave no partially registered process or thread.
- Keep architecture-specific context switching outside the generic Process Manager.
- Treat process-table pointers as kernel-internal; unrelated subsystems must not
  mutate process records directly.

## 7. Current status

SyntraOS v0.1.0 includes a tested process-to-thread ownership prototype with
scheduler-derived aggregate states. It does not yet provide address-space
isolation, full user-process execution semantics, or complete resource
reclamation. Those remain future milestones.

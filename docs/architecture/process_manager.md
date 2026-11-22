# SyntraOS Process Manager

Version: 0.1.0
Status: Process/thread ownership and address-space ownership prototype.

## 1. Overview

The Process Manager maintains a fixed-capacity process table, associates scheduler
threads with processes, derives aggregate process state, and owns an optional
ARM64 address-space object. This is kernel-side lifecycle infrastructure; it is
not yet a complete isolated user-process implementation.

## 2. Implemented

- Fixed capacity of 64 process records and up to 32 thread IDs per process.
- Non-zero process IDs and duplicate-ID rejection.
- A scheduler thread may be attached to only one process.
- Process state is derived from scheduler thread state with the precedence
  RUNNING, READY, BLOCKED, then TERMINATED.
- A process may take ownership of one validated ARM64 address-space object.
  The address space must already have inherited the kernel mappings.
- An address-space object cannot be attached to two process records.
- `process_get_address_space_for_thread()` resolves the address space owned by
  the process containing a given scheduler thread.
- Process termination refuses to proceed while any attached thread is not
  terminated. After all threads have stopped, it destroys the owned address
  space and clears the process pointer.
- Process validation checks address-space validity, uniqueness, thread
  ownership, and scheduler invariants.

## 3. Address-space ownership contract

`process_attach_address_space()` transfers lifecycle ownership of the supplied
address-space object to the Process Manager. The object itself must remain at a
stable address for the duration of ownership. A process cannot attach a second
address space, and a single address-space object cannot be shared between
process records.

`process_terminate()` is the teardown path. It requires every associated
scheduler thread to be TERMINATED before it destroys the address space. The
address-space destructor releases process-owned pages and private page-table
pages while preserving borrowed kernel page-table subtrees.

Address-space activation is now available at the ARM64 architecture layer.
It switches TTBR0_EL1, performs a local full EL1/EL0 TLB invalidation, tracks the
active root, and rejects destruction of that root. QEMU boot tests exercise a
switch to an inherited process root and back to the kernel root. The ARM64 exception/scheduler return bridge now selects the next thread's
address space, or restores the kernel root for kernel threads. The active-root
tracker is single-core only; SMP coordination and remote TLB shootdown remain absent.

## 4. Not yet implemented

- Scheduler-driven TTBR0_EL1 switching and active address-space tracking.
- TLB invalidation/shootdown and SMP-safe lifecycle synchronization.
- Creation of user processes with mapped code, data, and user stacks.
- Per-process resource handles, automatic IPC cleanup, and capability policy.
- Exit status, parent/child relationships, wait semantics, and transactional
  process/thread creation.
- Thread stack ownership transfer and stack reclamation.

The thread association is still metadata ownership; it does not create or
schedule a thread, switch address spaces, or reclaim scheduler stacks.

## 5. Required integration sequence

1. Add user-process construction with code/data/stack mappings and connect
   those mappings to scheduler-created EL0 threads.
2. Add QEMU tests that execute separate EL0 address spaces and verify permission
   faults and cross-process isolation.
3. Add SMP-safe active-root tracking and remote TLB shootdown before SMP use.
4. Add process resource ownership, IPC capabilities, and failure rollback.

## 6. Correctness requirements

- Reject invalid and duplicate process identifiers.
- Reject duplicate thread attachment and duplicate address-space ownership.
- Do not terminate a process while any attached thread can still run.
- Do not reclaim an address space while its root can be active on a CPU.
- Keep architecture-specific context switching outside the generic Process
  Manager.
- Treat process-table pointers as kernel-internal; unrelated subsystems must not
  mutate process records directly.

## 7. Version status

SyntraOS remains v0.1.0 (Early Development). The current implementation provides
process/thread metadata and tested ownership of inactive ARM64 address spaces.
It does not yet provide hardware-enforced per-process isolation or full user
process execution semantics.

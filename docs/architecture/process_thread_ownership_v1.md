# Process–Thread Ownership V1

## Scope

This phase records the exact generation-aware process handle on each scheduler
thread and provides a coordinator for process-owned thread creation. It does
not implement thread removal, stack reclamation, process teardown cascades,
IPC authentication, process-slot reuse, or SMP synchronization.

## API boundary

New process-owned threads must be created through
`process_thread_create_kernel()` or `process_thread_create_user()` in
`kernel/process/process_thread.h`. The coordinator resolves the supplied
handle through the Process Manager, rejects invalid or terminated processes,
and then commits the thread to the scheduler.

The scheduler retains legacy creation APIs for bootstrap and compatibility
threads. Such threads have `has_process_owner == 0` and a zeroed owner handle.
Owned threads have `has_process_owner == 1`; both the slot and generation
must match when querying ownership.

## Synchronization

The coordinator holds an outer local-IRQ critical section across process
validation and scheduler insertion. The Process Manager and Scheduler use
nested save/restore operations, preserving the already-masked interrupt state.
This is an atomicity mechanism for the current single-core ARM64 target only.
It is not an SMP lock and does not mask FIQ.

## Lifetime and resource ownership

Kernel and user stack buffers, and the optional architecture CPU context,
remain caller-owned. The scheduler reserves registered stack ranges until
scheduler reset. A terminated process is not automatically terminated or
reaped with its threads. Process reclamation must remain disabled while
threads can still reference its handle.

## Validation

The dedicated ProcessThreadOwnershipTest covers valid kernel and user thread creation, multiple
threads per process, exact slot+generation matching, invalid/stale handles,
terminated-process rejection, owner-specific enumeration, failed-create
atomicity, and compatibility with unowned scheduler threads. Full CI also
builds Debug and Release configurations and verifies the ARM64 ELF/QEMU path.

## Explicit limitations

- No thread exit/reaping or slot reuse.
- No stack/context deallocation.
- No automatic process-to-thread termination cascade.
- No authenticated IPC endpoints.
- No SMP-safe locking or production-readiness claim.


## ARM64 QEMU integration

The ARM64 image now compiles the Process Manager and Process/Thread coordinator.
The boot smoke test initializes the manager after the scheduler context-switch
test, creates a process-owned kernel thread, and verifies its owner handle and
owner-specific count through snapshot APIs. The probe thread is registered but
not executed. This checks runtime linkage and metadata coordination on QEMU; it
does not establish user-mode process isolation or thread teardown semantics.

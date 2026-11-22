# SyntraOS Process Manager

Version: 0.1.0
Status: Process/thread ownership, address-space ownership, and QEMU EL0 execution prototype.

## 1. Overview

The Process Manager maintains a fixed-capacity process table, associates scheduler
threads with processes, derives aggregate process state, and owns an optional
ARM64 address-space object. The current implementation demonstrates hardware
translation-root switching and one fixed EL0 process smoke test on QEMU. It is
not yet a general-purpose process loader or complete process environment.

## 2. Implemented

- Fixed capacity of 64 process records and up to 32 thread IDs per process.
- Non-zero process IDs and duplicate-ID rejection.
- A scheduler thread may be attached to only one process.
- A user thread cannot be attached unless the target process already owns a
  validated address space; process validation preserves this invariant.
- Process state is derived from scheduler thread state with the precedence
  RUNNING, READY, BLOCKED, then TERMINATED.
- A process may take ownership of one validated ARM64 address-space object.
  The address space must already have inherited the kernel mappings.
- An address-space object cannot be attached to two process records.
- process_get_address_space_for_thread() resolves the address space owned by
  the process containing a given scheduler thread.
- The ARM64 exception/scheduler return bridge activates the selected thread's
  process root before restoring its context, and restores the kernel root for
  unassociated kernel threads.
- Process termination refuses to proceed while any attached thread is not
  terminated. After all threads have stopped and the root is inactive, it
  destroys the owned address space and clears the process pointer.
- Process validation checks address-space validity, uniqueness, thread
  ownership, and scheduler invariants.

## 3. QEMU verification

The ARM64 QEMU boot tests cover two levels of address-space execution:

1. Two kernel-mode threads attached to different processes are scheduled under
   different TTBR0_EL1 roots. Each thread verifies its own root, then the
   scheduler returns to the kernel root.
2. A fixed EL0 user program is copied into a PROCESS-owned code page. Its code
   mapping is Read-Only+Executable, while its separate user stack is
   Read-Write+Execute-Never. The scheduler activates the process root, the
   program executes two SVC calls from EL0, and the user-thread exit trampoline
   terminates the thread. The scheduler then restores the kernel root and the
   Process Manager releases the process address space.
3. A separate EL0 test attempts to write into its Read-Only+Executable code
   page. QEMU reports a lower-EL Data Abort; the handler terminates only that
   user thread through the scheduler-safe trampoline, restores the kernel root,
   and releases the faulting process address space.

These tests demonstrate the configured QEMU ARM64 path only. They do not prove
physical-device support, SMP safety, or a general executable-loading facility.

## 4. Address-space ownership contract

process_attach_address_space() transfers lifecycle ownership of the supplied
address-space object to the Process Manager. The object itself must remain at a
stable address for the duration of ownership. A process cannot attach a second
address space, and a single address-space object cannot be shared between
process records.

process_terminate() is the teardown path. It requires every associated
scheduler thread to be TERMINATED before it destroys the address space. The
address-space destructor releases process-owned pages and private page-table
pages while preserving borrowed kernel page-table subtrees. Destruction rejects
the locally active root.

TTBR0 activation performs a local full EL1/EL0 TLB invalidation. Active-root
tracking and TLB invalidation are single-core V1 mechanisms; SMP coordination
and remote TLB shootdown remain absent.

## 5. Not yet implemented

- General ELF loading, executable validation, relocation, and multi-segment
  user-image construction.
- Per-process heap and stack allocation services beyond the fixed smoke test.
- Complete user-fault attribution and exit reporting, execute-from-NX and
  unmapped-access tests, process exit status, parent/child relationships, wait
  semantics, and restart policies.
- Per-process resource handles, automatic IPC cleanup, and capability policy.
- Transactional process/thread creation and rollback.
- Thread stack ownership transfer and stack reclamation.
- SMP-safe active-root tracking, remote TLB shootdown, and lifecycle synchronization.

Thread association remains metadata ownership: it does not allocate scheduler
stacks or provide a general application lifecycle. Architecture-specific
context switching remains in the ARM64 exception/scheduler layer, not the
generic Process Manager.

## 6. Required integration sequence

1. Build a validated user-image loader with explicit code, data, and stack
   segments and a defined user-process ABI.
2. Add process-scoped syscalls, resource handles, and capability checks.
3. Expand fault-isolation tests to unmapped access and execute-from-NX stack
   pages, and add per-process fault reporting; write-to-code termination is
   already covered by the QEMU smoke test.
4. Add process resource cleanup and transactional creation rollback.
5. Add SMP-safe active-root tracking and remote TLB shootdown before SMP use.

## 7. Version status

SyntraOS remains v0.1.0 (Early Development). It now has a tested QEMU prototype
for per-process TTBR0 switching and one EL0 user thread with separate code and
stack mappings. It does not yet provide a general-purpose executable loader,
complete process ABI, comprehensive fault isolation, or physical-device support.

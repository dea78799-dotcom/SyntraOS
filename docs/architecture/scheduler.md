# SyntraOS Scheduler

Version: 0.1.0
Implementation status: V1 scheduler prototype; ARM64 context-switch path validated in QEMU

## 1. Responsibilities

The scheduler maintains a bounded set of thread records, selects runnable
threads, and tracks READY, RUNNING, BLOCKED, and TERMINATED states. The
architecture layer owns exception-frame construction, saving, and restoration.
Stack memory is caller-owned; the scheduler records its address ranges but
does not allocate or free stack memory.

## 2. Thread Control Block

Each V1 thread record contains:
- A nonzero thread ID and priority metadata.
- Thread lifecycle state.
- Entry function and argument.
- Caller-provided EL1 stack base and size.
- Optional EL0 stack base and size for a user thread.
- An architecture-owned saved CPU-context pointer.

The table is fixed at 32 thread records. Stack sizes must be at least 1024
bytes and stack bases must be 16-byte aligned. Registered EL1 and EL0 stack
ranges are checked for overlap and remain reserved until scheduler reset.
The current lifecycle coordinator supports deferred reclamation for explicitly
owned process kernel-thread stacks. The bounded Thread Reaper consumes exact
post-switch-away proofs, reclaims owned stacks, retires TCBs, and allows
generation-safe reuse of fully retired slots. Borrowed stacks are never freed
by the reaper.

## 3. Scheduling policy

V1 uses deterministic round-robin selection across READY threads. Priority is
stored as metadata and does not affect selection order. The context-aware
selection path skips READY records that do not have a saved CPU context;
metadata-only records remain available to the policy-only test API.

## 4. Lifecycle and limitations

- READY: eligible for selection.
- RUNNING: currently selected by the scheduler.
- BLOCKED: not eligible until explicitly woken.
- TERMINATED: no longer eligible for selection.

The scheduler does not own arbitrary caller-provided stack buffers and does not
reclaim BORROWED stacks. Process-owned kernel-stack reclamation is coordinated
through the Memory V2 allocation token and exact process/thread owner handles.
User-thread resource reclamation and address-space teardown remain outside this
lifecycle path.

## 5. ARM64 integration and synchronization

The ARM64 exception bridge saves the interrupted context and selects a
thread context for exception return. The QEMU ARM64 CI path exercises the
context-switch test and verifies the expected boot/worker behavior.

The scheduler now exposes copy-out inspection APIs
(scheduler_get_thread_snapshot and scheduler_get_current_snapshot) and
controlled context mutation/selection APIs. The ARM64 boot and exception
bridge use these APIs instead of retaining mutable thread-record pointers.
Legacy mutable-pointer accessors remain for compatibility and must not be used
by new kernel code.

Public scheduler operations now save and mask local IRQ state with
syntra_irq_save()/syntra_irq_restore() around metadata reads and mutations.
This follows the existing single-core ARM64 page-allocator critical-section
pattern and is safe for nested calls because each operation restores the
caller's previous DAIF state.

This is local interrupt exclusion, not an SMP lock. It does not serialize
multiple CPU cores, FIQ handlers, or arbitrary concurrent execution. Legacy
mutable-pointer accessors return after IRQ state is restored; callers must not
retain or mutate those pointers. New kernel code must use snapshot and
controlled-mutation APIs. QEMU validation is not an SMP-safety proof. Before
enabling multiple cores or broader exception classes, define and test a
separate cross-core synchronization contract.

## 6. Validation scope

Host unit tests cover round-robin order, state transitions, duplicate IDs,
stack alignment and size checks, stack overlap and overflow rejection,
user/kernel stack separation, context-aware selection, and scheduler
invariants. ARM64 CI additionally validates the kernel ELF, QEMU boot, and
the current context-switch test path.

These tests do not establish SMP safety, complete thread cleanup, dynamic
stack allocation, process isolation, or production readiness.

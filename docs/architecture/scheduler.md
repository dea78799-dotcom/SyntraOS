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
There is no thread-reaping or stack-release API yet.

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

V1 does not reclaim terminated thread records or stack ranges. The scheduler
does not yet own process membership, thread resources, or address spaces.

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

These copy-out APIs prevent callers from mutating scheduler records through
an inspection result; they do not themselves provide synchronization. The
scheduler data structure currently has no internal lock or IRQ-save
critical-section wrapper. Do not assume its APIs are safe against arbitrary
concurrent callers or SMP execution. The current QEMU validation is not an
SMP-safety proof. Before enabling broader preemption or multiple cores, define
and test a consistent synchronization contract for every state mutation and
for APIs that expose mutable thread-record pointers.

## 6. Validation scope

Host unit tests cover round-robin order, state transitions, duplicate IDs,
stack alignment and size checks, stack overlap and overflow rejection,
user/kernel stack separation, context-aware selection, and scheduler
invariants. ARM64 CI additionally validates the kernel ELF, QEMU boot, and
the current context-switch test path.

These tests do not establish SMP safety, complete thread cleanup, dynamic
stack allocation, process isolation, or production readiness.

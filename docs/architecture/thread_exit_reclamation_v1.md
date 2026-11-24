# Thread Exit and Resource Reclamation V1 — Design Audit

Version target: SYNTRA OS v0.1.0
Status: Design audit only; no reaper or allocator ownership is implemented.
Base: Process termination cascade V1

## 1. Audit conclusion

Thread termination and resource reclamation must be separate phases. A TCB marked
`THREAD_TERMINATED` is no longer eligible for scheduling, but that state alone
does not prove that the CPU has stopped using its stack or that no saved
exception frame still points into that stack.

Do not free or reuse a thread's kernel stack, user stack, CPU context, or TCB
until the architecture layer has switched to a different execution context and
the reaper can prove that the terminated thread is not current and has no
remaining scheduler/exception references.

## 2. Existing implementation findings

### Scheduler and TCB

- The scheduler stores a fixed array of 32 TCBs and a monotonically increasing
  `thread_count`; no removal or slot-reuse API exists.
- Each TCB stores caller-provided kernel stack bounds, optional user stack
  bounds, a saved CPU-context pointer, process ownership metadata, and state.
- Stack overlap checks reserve all registered stack ranges until scheduler
  reset, including ranges belonging to terminated threads.
- The scheduler's current-thread metadata is `current_slot`; the exception
  bridge uses snapshots and `scheduler_schedule_next_context()` to select a
  saved context.

### ARM64 thread-return path

- `syntra_arm64_context_initialize()` places `syntra_arm64_thread_exit()`
  in the initial frame's x30 return address.
- `syntra_arm64_thread_exit()` currently calls
  `scheduler_terminate_current()` and then waits with `WFE`.
- The timer IRQ exception bridge can later select another READY thread when
  context switching is active. The exit trampoline itself does not synchronously
  switch stacks or prove that the old stack is no longer referenced.
- If no eligible context exists, the current exit path remains waiting. This
  is a safe stop for the prototype, not a complete thread-exit/reaper protocol.

### Process termination interaction

- Process termination marks matching owned TCBs terminated and clears
  `current_slot` when it matches.
- If the terminated thread is executing at that moment, metadata mutation does
  not instantly move the CPU off its stack. The next architecture scheduling
  boundary must discard that context before reclamation is permitted.
- Therefore the process cascade must never free stack or context memory.

### Resource ownership

- Scheduler creation APIs accept caller-owned memory.
- No allocation token, allocator identity, release callback, or ownership flag
  exists for kernel stacks, user stacks, or CPU contexts.
- The page allocator currently exposes page allocation; this audit does not
  establish that arbitrary caller stack buffers were allocated by it or can be
  returned to it.

## 3. Required lifecycle model

Use two distinct milestones:

1. **TERMINATED** — execution is no longer eligible to run. This is a scheduler
   state and may be set by normal return, explicit exit, process cascade, or
   fault policy.
2. **REAPABLE** — the architecture has switched away, the thread is not the
   current execution context, no saved context/exception path references its
   stack, and the resource owner has authorized release.

A thread must not become REAPABLE merely because its TCB says TERMINATED.

## 4. Proposed architecture boundary

- Thread exit is requested through a non-returning architecture-aware path.
- For a running thread, the exit path records termination and enters the
  scheduler/exception return path. It must not return to the terminated entry.
- A separate reaper context performs cleanup only after a context-switch
  completion boundary confirms the old stack is inactive.
- Reaping removes the TCB from active lookup/scheduling and releases only
  resources explicitly transferred to a recognized allocator.
- Caller-owned buffers remain caller-owned and are never freed by the kernel.
- User stack and kernel stack lifetimes are tracked independently.
- CPU context memory is released only after the architecture no longer needs
  the saved frame.
- Thread ID reuse requires a generation-aware thread handle; a plain numeric
  thread ID must not silently refer to a different lifetime.

## 5. Required invariants

- The current thread's active kernel stack is never freed.
- No exception-return frame points into reclaimed memory.
- A terminated TCB cannot transition back to READY, RUNNING, or BLOCKED.
- A process cascade may mark threads terminated but cannot reclaim them inline.
- A reaper cannot run on the stack it is about to release.
- A stack range is removed from overlap reservations only after successful
  resource release or explicit confirmation that the caller retains it.
- Failed cleanup leaves the TCB and all unreleased resource metadata intact.
- Reaping is idempotent and cannot double-free a resource.
- Process slots remain occupied until all owned threads are reaped and process
  resources are independently proven quiescent.

## 6. Implementation phases

### Phase A — Exit-path correctness
- Audit the ARM64 timer/exception return contract.
- Make thread return and explicit thread exit converge on one non-returning
  scheduler handoff.
- Add tests proving a returned thread is never selected again.
- Define behavior when no successor is runnable (idle thread requirement).

### Phase B — Resource ownership metadata
- Introduce explicit borrowed-versus-owned resource descriptors.
- Define allocator-specific release operations for kernel stack, user stack,
  and context storage.
- Do not infer ownership from pointer ranges or allocation size.

### Phase C — Deferred reaper
- Add a dedicated reaper queue populated only after switch-away confirmation.
- Reap from a different, stable kernel stack.
- Validate no-current/no-context-reference invariants before each release.
- Keep failures retryable without losing resource metadata.

### Phase D — Thread handle and slot reuse
- Introduce generation-aware thread handles before reusing TCB slots or IDs.
- Add stale-handle rejection and generation-wrap policy.
- Only then consider process reaping and process-slot reuse.

## 7. Required tests before implementation is considered complete

- Kernel entry returns and the architecture switches to another thread.
- Explicit exit and natural return converge on the same terminal path.
- Current thread is never reaped while its stack is active.
- A terminated thread with a saved context cannot be reaped.
- After a confirmed switch-away, a reaper on another stack can reclaim owned
  resources exactly once.
- Borrowed resources are never freed.
- Kernel and user stacks are independently accounted for.
- Failed allocator release retains retry metadata.
- Process cascade queues all owned threads but does not release any resource.
- Stale thread handles fail after a slot is eventually reused.
- No-successor behavior is deterministic and does not resume a terminated
  thread.

## 8. Explicit non-goals

This document does not implement memory freeing, thread-table compaction,
thread-ID reuse, process-slot reuse, SMP locking, user-space isolation, or a
production-ready idle/reaper subsystem. The existing v0.1.0 behavior remains a
single-core QEMU prototype.

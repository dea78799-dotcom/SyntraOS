# Thread Switch-Away Proof V1 — D2 Architecture Gate

Version target: SYNTRA OS v0.1.0  
Status: Source audit and implementation contract only. No switch-away callback, completion queue, reaper, or resource release is implemented.  
Parent: Thread Handle V1.1 and Deferred Thread Reaper V1

## 1. Decision

D2 must establish an architecture-level proof that a thread's kernel stack is no longer active before any future reaper can release it.

The existing thread handle `{slot, generation}` is the identity carried by that proof. Numeric thread IDs and raw TCB pointers are not acceptable proof identities.

This document is a gate, not authorization to free memory. The current source does not yet preserve the outgoing thread handle across every transition that clears `current_slot`, and it has no post-stack-switch completion hook. Therefore, no runtime reclamation code may be added until those gaps are closed and tested.

## 2. Source audit findings

The audit was performed against the D1.1 head that contains the handle-returning scheduler and process/thread APIs.

### 2.1 Scheduler loses the outgoing identity on several transitions

- `scheduler_terminate_current_unlocked()` marks the current TCB `THREAD_TERMINATED` and immediately sets `current_slot = -1`. It does not return or preserve the exact thread handle.
- `scheduler_block_current_unlocked()` marks the current TCB blocked and clears `current_slot`, also without preserving its handle.
- `scheduler_terminate_process_threads()` marks matching owner threads terminated and clears `current_slot` if the current slot belongs to that process. It does not publish which exact thread lifetime was detached.
- `scheduler_schedule_next_with_context_unlocked()` changes `current_slot` while selecting a saved context, but there is no explicit outgoing-handle record or switch sequence.

Consequently, a later call to `scheduler_get_current_handle()` cannot recover the terminated or blocked thread after `current_slot` has been cleared.

### 2.2 Thread exit remains on the old stack until an interrupt selects another context

`syntra_arm64_thread_exit()` calls `scheduler_terminate_current()` and then waits in a non-returning `WFE` loop. The thread's stack is still active during this wait. A timer IRQ must reach the exception path and select a successor before the old stack can become inactive.

The IRQ bridge currently tries to save the current context by querying the scheduler's current snapshot. That query cannot identify the exiting thread after `scheduler_terminate_current()` cleared `current_slot`. This is expected for a terminated context that must not resume, but it also confirms that the outgoing handle must have been captured before the scheduler transition.

### 2.3 The assembly has a candidate proof boundary, but no completion event

In `kernel/arch/arm64/boot/vectors.S`:

1. The vector entry builds the exception frame on the interrupted thread's stack.
2. It calls `syntra_arm64_exception_handle_and_select()` while still using that stack.
3. If the C bridge returns a successor frame, assembly executes `mov sp, x0`.
4. Register restoration and `eret` then use the selected frame.

The instruction that changes `SP_EL1` to the successor frame is the earliest candidate boundary at which the outgoing kernel stack is no longer the active stack. A callback before that instruction is unsafe for reclamation.

There is currently no callback after `mov sp, x0`, and no completion record is emitted. The source therefore provides no switch-away proof today.

### 2.4 The no-successor path correctly fails closed

When no runnable saved context exists, the C bridge invalidates the return cookie. Assembly enters its non-returning exception halt loop instead of restoring the terminated context.

That path must never emit a switch-away completion. The CPU has not moved to a successor stack; the old stack remains active. Its resources must remain allocated indefinitely until a future, separately proven recovery path exists.

## 3. Required D2 protocol

### 3.1 Capture the outgoing identity before scheduler detachment

Every transition that can detach the executing thread must preserve its exact handle before changing `current_slot` or the thread state.

The initial single-core design must cover at least:

- normal IRQ preemption;
- explicit thread termination;
- blocking the current thread;
- process termination that includes the current thread;
- any future scheduler operation that replaces the executing context.

The record must include the exact thread handle and a monotonically increasing switch sequence or equivalent unique completion token. It must not be reconstructed later from `current_slot`.

A single pending outgoing record may be sufficient for the current single-core prototype, but only if nested/re-entrant transitions are prohibited or explicitly handled. The implementation must assert that a second outgoing transition cannot overwrite an unconsumed record.

### 3.2 Publish completion only after the stack switch

The assembly path may call a small completion hook only after it has loaded the successor frame pointer into `SP_EL1`.

The hook may publish a bounded completion record containing:

- exact `syntra_thread_handle_t`;
- matching switch sequence/token;
- a completion state indicating that the successor stack is active.

The hook must not free memory, remove the TCB, call the general allocator, or run the reaper. It is an evidence-publication step only.

The hook must execute on the successor stack. It must not access local variables or pointers that reside on the outgoing stack. The saved successor exception frame must remain intact while the hook runs.

### 3.3 No successor means no proof

If the C bridge returns no successor and assembly enters the halt path:

- do not call the completion hook;
- do not mark the thread SWITCHED_AWAY;
- do not enqueue it as REAPABLE;
- do not release its kernel stack, user stack, or context storage.

A timeout, scheduler metadata change, cleared `current_slot`, or `THREAD_TERMINATED` state is not a substitute for the architectural completion event.

## 4. Completion record and validation contract

A future completion record must be validated against the live scheduler entry before it can advance lifecycle state.

Required checks:

1. Handle slot is in range and generation is nonzero.
2. Exact slot and generation match the TCB.
3. TCB is already `THREAD_TERMINATED` or has an explicitly supported detached state.
4. Completion token matches the pending outgoing transition.
5. Completion has not already been consumed.
6. The completed handle is not the currently executing handle.
7. The event was emitted only from the post-`SP_EL1`-switch assembly path.

Duplicate, stale, mismatched, or out-of-order records must fail closed. They must never cause a second completion or release.

The queue must be bounded. If it is full, the implementation must preserve the pending proof and keep resources allocated; it must not drop the only completion record or perform inline cleanup.

## 5. Relationship to resource ownership and the reaper

A valid switch-away event is necessary but not sufficient for reclamation.

Before a reaper can release anything, the resource-ownership layer must provide immutable descriptors with exact allocator provenance. The current TCB stores stack addresses and sizes as caller-owned metadata; those fields alone do not authorize `memory_release()`.

The current memory-pool API requires the exact allocation base and block count. A future owned-stack descriptor must preserve those exact values and prove that ownership was explicitly transferred. The embedded ARM64 exception frame is part of the kernel-stack allocation and must not be released separately. A user stack is a separate resource and requires its own ownership and mapping-lifecycle proof.

The reaper must run on a stable kernel stack that is not among the candidate resources. It may consume completion records later in normal kernel context. It must never run from the exception callback or on the stack it may release.

## 6. Single-core limitation

The current kernel uses local IRQ masking as a single-core critical-section mechanism. It is not SMP synchronization.

The first implementation must explicitly remain single-core. The pending outgoing record, completion queue, scheduler TCB lookup, and memory allocator must all follow one documented serialization model. No lock-free or cross-core safety claim is permitted.

Before SMP, this protocol needs cross-core ownership, ordering, and synchronization rules for scheduler state, completion publication, allocator metadata, and stack migration.

## 7. Required validation before runtime enablement

### Host tests

- Terminating the current thread preserves its exact handle as the outgoing identity.
- Blocking the current thread preserves its exact handle.
- Process termination preserves the exact current-thread handle.
- Normal preemption captures the old handle before selecting the successor.
- A second outgoing transition cannot overwrite an unconsumed record.
- Stale handles, wrong generations, invalid tokens, duplicates, and out-of-order events are rejected.
- Queue saturation retains the proof and does not authorize cleanup.

### ARM64/QEMU tests

- A worker exits naturally and the boot thread resumes on its own stack.
- The completion hook observes the successor stack as active before publishing the event.
- The outgoing handle and token in the event match the thread that just switched away.
- No completion event is emitted before the assembly stack-pointer switch.
- The no-successor halt path emits no completion event.
- The completion hook does not alter the selected exception frame or its return registers.

### Reaper integration tests, later phase only

- TERMINATED without a completion event is never REAPABLE.
- A completion event with a stale handle or invalid token cannot enter the reaper.
- No resource is released from IRQ/exception context.
- Borrowed resources are never passed to an allocator release function.
- Partial cleanup is idempotent and cannot double-release a resource.

## 8. Explicit non-goals

This D2 gate does not implement a switch-away callback, completion queue, reaper worker, resource descriptor, allocator integration, TCB removal, thread-slot reuse, process reaping, or SMP synchronization.

The next runtime milestone must implement and test the outgoing-handle capture and post-stack-switch completion event only. Actual resource release remains disabled until ownership descriptors and reaper-context guarantees are independently validated.

## 9. Reference checked with Context7

The GNU C Language Manual sections on type alignment and pointer arithmetic were checked while reviewing the stack-frame and resource-descriptor boundary:

- https://www.gnu.org/software/c-intro-and-ref/manual/c-intro-and-ref.html

The relevant constraints are that typed pointers must satisfy the target type's alignment and pointer arithmetic must remain within the relevant allocated object. These constraints support the requirement to validate stack-frame bounds/alignment and to preserve exact allocation descriptors rather than reconstructing ownership from arbitrary pointer arithmetic.

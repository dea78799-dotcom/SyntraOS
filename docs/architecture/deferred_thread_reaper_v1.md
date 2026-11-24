# Deferred Thread Reaper V1 — Phase C Design

Version target: SYNTRA OS v0.1.0  
Status: Design proposal only; no reaper queue, stack reclamation, or TCB removal is implemented.  
Base: Thread Resource Ownership V1

## 1. Objective

Define a safe deferred-reclamation boundary for terminated threads. The reaper must never run on a stack it may release, and a TERMINATED state must never be treated as proof that the CPU has stopped referencing a thread's resources.

This phase is design-only. It does not authorize memory release or alter scheduler behavior.

## 2. Lifecycle states and proof boundary

The lifecycle is deliberately split into independent facts:

- TERMINATED: scheduler policy will not select the thread as runnable.
- SWITCHED_AWAY: the architecture has completed a context switch to a different execution context and the old thread's kernel stack is no longer active.
- REAPABLE: all architecture, scheduler, and ownership checks pass; the resource owner may submit the thread to deferred cleanup.
- REAPED: every owned resource has been released or explicitly retained by its owner, and the TCB is no longer in active lookup.

TERMINATED alone never implies SWITCHED_AWAY, REAPABLE, or REAPED.

A scheduler metadata update, clearing current_slot, selecting a next TCB, or returning from a C function is not sufficient proof of SWITCHED_AWAY.

## 3. Required architecture completion event

The ARM64 switch path must provide a completion event only after execution is running on the successor's kernel stack. The event must identify the exact exiting thread lifetime, not only a reusable numeric thread ID.

The event must not be emitted:
- before the stack pointer has changed to the successor stack;
- from code still executing on the exiting thread's stack;
- when the exception path is returning to the same interrupted context;
- when no successor context exists and the CPU enters the prototype halt path.

The event should carry a generation-aware thread handle. Until thread handles exist, the implementation must not use this event to release resources or reuse IDs.

A future implementation must inspect the assembly context-switch sequence and establish the exact instruction boundary at which the old stack is inactive. A C-level callback before the final stack switch is not acceptable.

## 4. Reaper execution context

The reaper must execute on a dedicated, stable kernel stack that is not owned by any thread currently being reaped.

Initial implementation constraints:
- one reaper context on the existing single-core ARM64 prototype;
- no SMP assumptions or lock-free queue claims;
- the reaper's own stack and context are permanently reserved for the prototype;
- no reaper work runs from IRQ context;
- IRQ handling may publish a small completion record only after the architecture completion boundary is proven;
- the reaper drains records later from normal kernel execution.

If no reaper context is scheduled, completed records remain queued and resources remain allocated. Memory pressure must never force unsafe inline reclamation.

## 5. Queue record and idempotency

A future queue record must contain:
- generation-aware thread handle;
- switch-away completion sequence or equivalent monotonic proof token;
- immutable resource descriptor references;
- per-resource release state;
- enqueue state preventing duplicate completion publication.

The record must be validated against the live TCB before any release. A stale handle, mismatched completion token, or duplicate queue record must fail closed without touching memory.

Do not store only a raw TCB pointer in a queue record because TCB storage may eventually be reused.

## 6. Reap transaction

The reaper performs a staged transaction:

1. Dequeue or inspect one candidate without destroying its retry metadata.
2. Resolve the exact generation-aware thread handle.
3. Confirm TERMINATED and SWITCHED_AWAY.
4. Confirm the thread is not current and no active/saved architecture context references its kernel stack.
5. Validate each resource descriptor against the allocator contract from Phase B.
6. Release one resource at a time through its owning allocator.
7. Mark each successful release exactly once.
8. Retain failed resources and the TCB record for retry.
9. Remove the TCB from active lookup only after all required owned resources are resolved.
10. Permit slot/ID reuse only in the later generation-handle phase.

A failure at any step must preserve enough metadata to retry safely. Never roll back a successful free by restoring a live pointer.

## 7. Resource-specific rules

- Kernel stack: release only after execution has moved to the reaper stack and no saved exception frame points into it.
- User stack: release independently only after the process/address-space owner confirms it is no longer mapped or referenced. The current prototype's user stack mapping is not a complete process address-space lifecycle.
- Embedded ARM64 context frame: it is part of the kernel stack allocation and must not be released separately.
- Borrowed buffers: never release; record the resource as externally owned and resolved only by explicit owner acknowledgement or permanent retention.
- Reaper stack/context: never enter the ordinary thread-reap queue.

## 8. Synchronization assumptions

The current kernel uses local IRQ masking for single-core critical sections. This is not SMP synchronization.

The first implementation must state and enforce a single-core-only contract. Before SMP support, the queue, TCB lookup, allocator metadata, and switch-away completion publication need a defined cross-core synchronization and memory-ordering design.

Do not claim that volatile fields or local IRQ masking provide cross-core safety.

## 9. Failure handling

- Missing switch-away proof: keep the thread TERMINATED and retain all resources.
- No reaper available: leave the record pending; do not free inline.
- Stale handle or invalid token: reject candidate and report an invariant failure.
- Allocator release failure: retain only the unreleased descriptor and retry state.
- Partial success: never repeat successful releases; continue only with resources not yet released.
- Duplicate completion: deduplicate by exact thread handle and completion token.
- Queue capacity exhausted: do not drop the only proof record; use a bounded backpressure/error path that leaves resources allocated.

## 10. Required tests before enabling the reaper

- A terminated current thread is not queued as REAPABLE before the stack switch.
- The completion event is emitted only after the successor stack is active.
- A no-successor halt emits no switch-away completion.
- The reaper never runs on a candidate's stack.
- Stale thread handles and mismatched completion tokens are rejected.
- Duplicate completion records cannot cause double release.
- Borrowed resources are never passed to an allocator release function.
- Kernel stack and independent user stack are accounted separately.
- Embedded context storage is not double-freed.
- Allocator failure preserves retry metadata.
- Partial cleanup never repeats successful release operations.
- Queue saturation leaves resources allocated and reports backpressure.
- No TCB slot or numeric ID is reused before the generation-handle phase.

## 11. Explicit non-goals

This document does not implement a reaper, queue, switch-away callback, allocator release, TCB removal, thread-ID reuse, process reaping, SMP synchronization, or a production-ready memory-pressure policy. SYNTRA OS remains a single-core QEMU prototype at v0.1.0.

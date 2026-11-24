# D3.3 Deferred Thread Reaper Runtime Design V1

Version target: SYNTRA OS v0.1.0  
Status: Design-only; no reaper task, queue, TCB lifecycle, or allocator release code is implemented.  
Parent design: D3.2 PR #81, branch `design/d3-resource-descriptor-api-v1`  
Scope: single-core ARM64 prototype, kernel-owned kernel stacks from Memory Pool only.

## 1. Decision

Use a dedicated normal-kernel reaper execution context on a permanently reserved stack, a bounded candidate queue keyed by exact thread handle and D2 switch-away sequence, and an explicit validate-then-release state machine.

The reaper is not an IRQ handler and is not called from the ARM64 post-switch hook. The hook remains metadata-only. The reaper never runs on a candidate's stack.

No candidate is released unless all of the following are independently true:
- scheduler state is TERMINATED;
- the exact generation-aware thread handle resolves;
- an exact D2 switch-away event has been accepted for that handle;
- the event sequence matches the candidate record;
- the candidate is not current and no architecture path can resume it;
- the resource descriptor is kernel-owned, valid, and bound to the same thread handle;
- the reaper is executing on its reserved stack;
- the allocator validates the exact allocation token and owner.

TERMINATED, SWITCHED_AWAY, and REAPABLE remain separate lifecycle facts.

## 2. Existing event boundary

D2 currently provides:
- exact `syntra_thread_handle_t`;
- monotonically increasing nonzero 64-bit switch-away sequence;
- bounded 128-entry FIFO;
- publication only after ARM64 installs a different successor `SP_EL1`;
- no allocation, release, logging, or TCB mutation in the post-switch hook.

The D2 FIFO is proof-event transport, not a reaper work queue. It may contain events for ordinary preemption and blocking as well as termination. A switch-away event for a non-terminated thread is not a reap candidate.

The current QEMU boot test consumes D2 events. Before production reaping is enabled, the test-only consumer must be separated from or replaced by a single authoritative event dispatcher. Two independent consumers must never race to drain the same FIFO.

## 3. Event-to-candidate handoff

Use two bounded stages with different meanings:

1. **D2 proof FIFO (capacity 128):** records that an exact thread lifetime left the CPU stack.
2. **Reap candidate queue (capacity SYNTRA_MAX_THREADS, currently 32):** records terminated threads whose exact proof event has been accepted and whose resource cleanup should be attempted.

Proposed dispatcher contract:
- inspect the FIFO head without losing it;
- resolve the exact handle against the scheduler;
- if the thread is not TERMINATED, acknowledge and discard the event as non-reapable;
- if TERMINATED, reserve/deduplicate a candidate record containing handle + exact sequence;
- acknowledge the D2 event only after the candidate record is safely stored;
- if candidate queue has no capacity, do not acknowledge the D2 event; stop draining and leave resources allocated;
- if the handle is stale or the event is inconsistent, record an invariant diagnostic, quarantine the event, and never release memory.

The current `scheduler_take_switch_away_event()` is pop-on-read. A production dispatcher must either reserve candidate capacity before calling it or replace it with a peek/ack protocol. It must not pop first and then risk losing the only proof event.

Deduplication:
- same handle + same sequence: duplicate; do not add a second candidate;
- same handle + different sequence for a TERMINATED lifetime: invariant failure; quarantine without release;
- different generation: distinct lifetime and never interchangeable;
- raw TCB pointers are not queue identities.

The fixed candidate capacity is tied to the current 32-slot scheduler limit. One TCB lifetime can have at most one accepted terminal reap candidate because the thread is no longer runnable after TERMINATED. If that invariant changes, queue sizing and deduplication must be redesigned.

## 4. Reaper execution context

The reaper runs only as a dedicated normal-kernel execution context:
- reserved static kernel stack and context;
- not allocated from the ordinary per-thread Memory Pool path;
- excluded from the candidate queue by role and immutable identity;
- never terminated or reclaimed by the ordinary thread reaper;
- no candidate's stack may be the current stack;
- no work runs inside IRQ context or the ARM64 post-switch hook.

The implementation must prove at runtime/test time that the current stack pointer lies within the reserved reaper stack before invoking an owned release. If that check fails, the reaper stops and leaves the candidate queued.

If the reaper is not scheduled, resources remain allocated. There is no memory-pressure fallback that frees stacks inline.

## 5. Candidate record

A candidate record is fixed-size and contains no owning raw TCB pointer.

Proposed logical fields:
- candidate state: FREE, QUEUED, PROCESSING, RETRY, QUARANTINED;
- exact thread handle (slot + generation);
- exact D2 switch-away sequence;
- bounded attempt counter/diagnostic code;
- deduplication identity;
- no caller-controlled allocator pointer or block count.

Resource descriptors remain stored by value in the TCB and are copied into a local bounded snapshot only after exact handle resolution. The descriptor's allocation token and owner thread handle must match the candidate.

The candidate queue itself must not become a second owner of stack memory. It stores identity/proof only; the allocator token remains in the validated TCB resource descriptor.

## 6. Reaper state machine

```text
FREE
  -> QUEUED
  -> PROCESSING
      -> REAPED_RESOURCES   (all eligible resources resolved)
      -> RETRY              (transient failure; preserve descriptor)
      -> QUARANTINED        (identity/proof/ownership invariant failure)
```

Required checks, in order:

1. Confirm the current execution context is the reserved reaper.
2. Resolve the exact thread handle; reject stale or mismatched generation.
3. Confirm TCB state is TERMINATED.
4. Confirm stored terminal switch-away sequence exactly matches the candidate sequence.
5. Confirm candidate is not current and no scheduler/architecture return path can restore it.
6. Confirm kernel stack is not the reaper stack and no active stack range overlaps it.
7. Validate resource descriptor version, role, ownership, state, base/range, and allocation token.
8. Confirm allocator record is THREAD_OWNED by this exact thread handle.
9. Mark the resource RELEASE_PENDING under the single-core critical section.
10. Invoke the exact-token owner-checked allocator release.
11. Mark RELEASED only after allocator success, within the same serialized operation.
12. When every required resource is RELEASED or explicitly RETAINED, mark the thread lifecycle REAPED/retired.
13. Only after resource resolution may the TCB leave active lookup. Keep a tombstone and do not reuse its slot in this phase.

Any failed identity, proof, ownership, range, or stack-context check results in QUARANTINED with no release attempt. A transient allocator failure preserves the token and resource descriptor for a later retry.

## 7. Resource rules for first implementation

| Resource | D3 first implementation policy |
|---|---|
| Kernel stack allocated through trusted Memory Pool API | Eligible after all proof checks |
| Caller-provided kernel stack | BORROWED; never freed by reaper |
| User stack | BORROWED/RETAINED; no free until VM unmap/reference ownership exists |
| Embedded ARM64 context frame | Subrange of kernel stack; never separately freed |
| Reaper stack/context | Permanently reserved; never a candidate |
| Physical pages/page tables | Outside D3 thread-stack reclamation |

The first implementation should therefore release at most one resource per supported kernel thread: its Memory Pool-owned kernel stack. This reduces partial-release complexity while preserving the descriptor model for future resources.

## 8. Queue capacity, backpressure, and retry

- Candidate queue capacity is fixed at 32, matching `SYNTRA_MAX_THREADS`.
- Duplicate records do not consume extra slots.
- If the queue is full, the dispatcher stops consuming D2 events; it never drops the only proof event.
- If the D2 FIFO is full, retain D2's existing fail-closed behavior; never manufacture proof or free resources.
- On transient release failure, preserve the candidate and rotate it to the tail so another candidate can progress.
- A retry must not repeat a resource already marked RELEASED.
- Invariant failures move the candidate to QUARANTINED and preserve all unresolved allocations.
- No busy-spin retry loop. Retry occurs on a later normal reaper pass.
- Queue counters and diagnostic snapshots may be added, but logging is not performed in IRQ or post-switch context.

If the dispatcher cannot safely reserve candidate capacity before consuming a D2 event, the event API must gain a peek/ack or reservation protocol first.

## 9. TCB retirement and slot policy

Resource release and TCB slot reuse are different operations.

After every resource is RELEASED or explicitly RETAINED:
- mark lifecycle REAPED;
- reject the thread from runnable selection and active lookup;
- retain a small tombstone with the old exact handle for stale-handle diagnostics;
- do not clear the slot for reuse in D3;
- do not increment generation or permit slot reuse in D3.

The 32-slot limit may therefore be exhausted after enough thread lifetimes. This is intentional fail-closed prototype behavior. A separate audited generation/slot-reuse phase is required before reclaiming TCB slots.

## 10. Synchronization and execution assumptions

- Single-core ARM64 only.
- Local IRQ masking serializes dispatcher, scheduler, candidate queue, and allocator transitions.
- Nested IRQ-save/restore must preserve the previous mask state.
- This is not SMP synchronization.
- No lock-free guarantee.
- No allocator calls from IRQ context, exception assembly, or post-switch hook.
- The reaper's candidate processing runs only in normal kernel context.
- Critical-section duration and allocator scan cost must be measured before enabling runtime reclamation.

Before SMP, all queue, TCB, allocator, and ownership state require cross-core locking and memory-ordering design.

## 11. Required tests before implementation

### Event handoff
- non-terminal switch event is not queued for reaping;
- terminal event is accepted only for exact handle + sequence;
- duplicate same-sequence event is deduplicated;
- different sequence for the same terminal lifetime is quarantined;
- stale generation is rejected;
- candidate-full backpressure does not pop/drop the D2 head event;
- D2 FIFO overflow never creates a candidate without proof.

### Reaper context and eligibility
- reaper refuses to run outside its reserved stack;
- candidate kernel stack never overlaps reaper stack;
- terminated without switch-away proof is never released;
- switch-away event for non-terminated thread is never released;
- candidate that is current or resumable is never released;
- borrowed kernel/user stacks are never sent to owned allocator release.

### Allocator and retry
- exact allocation token and owner required;
- stale token/address reuse cannot release a new allocation;
- wrong owner rejected;
- successful release occurs once;
- transient failure retains retry metadata;
- partial success never repeats a successful resource release;
- quarantined candidate never releases memory.

### Retirement
- TCB remains active until all resource descriptors are resolved;
- REAPED thread is not runnable or returned by active lookup;
- slot is not reused;
- stale handle cannot resolve to a later thread.

### QEMU
- worker exits, successor runs, exact D2 proof is dispatched;
- reaper executes on reserved stack;
- reaper releases only the owned kernel stack;
- borrowed resources remain allocated;
- boot thread and reaper stack are never reclaimed;
- no-successor halt emits no proof and performs no release.

## 12. Explicit non-goals

This design does not implement:
- D2 FIFO peek/ack or dispatcher changes;
- candidate queue or reaper execution context;
- allocation token APIs;
- resource descriptor storage in TCB;
- actual memory release;
- user-stack unmapping or address-space teardown;
- process reaping;
- TCB slot reuse;
- SMP;
- physical hardware support.

D3.3 is a design contract only. D3.4 host tests and D3.5 ARM64/QEMU integration remain required before any real thread-stack release is enabled.

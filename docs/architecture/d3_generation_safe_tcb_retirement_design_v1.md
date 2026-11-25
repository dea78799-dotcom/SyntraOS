# D3.5.4 — Generation-Safe TCB Retirement and Slot Reuse Design

Target: SYNTRA OS v0.1.0  
Status: design only; no scheduler slot reuse is enabled by this document.  
Parent implementation: D3.5.3 proof-gated owned stack reclamation (PR #93).

## 1. Current limitation

The scheduler currently uses a fixed 32-entry TCB array with append-only publication. Handles are `{slot, generation}`, but published generations are currently fixed at 1 and slots are not reused. D3.5.3 releases an eligible owned kernel stack and clears the terminated TCB's stack/context references, but deliberately retains the TCB record.

The next phase must not simply decrement `thread_count` or overwrite a TERMINATED record. Switch-away events, reaper candidates, process ownership records, and delayed callers can still contain old handles.

## 2. Required invariants

1. A handle resolves only when its slot is ACTIVE and both slot and generation match exactly.
2. A retired handle becomes invalid before the slot can be reserved for a new thread.
3. A slot generation is never zero and never silently wraps.
4. A TCB may be retired only when it is TERMINATED, is not current, has no pending switch ticket, has no matching switch-away FIFO event, and has no live stack/context/resource references.
5. The reaper may retire only a candidate whose proof has been consumed and whose resource state is safe. A QUARANTINED candidate permanently blocks retirement until an explicit recovery policy exists.
6. Reusing a slot never makes an old event, process reference, candidate, or public handle identify the new thread.
7. No TCB memory is freed: the table is static. Retirement clears the record and changes slot metadata only.
8. Single-core local IRQ serialization is the only synchronization guarantee in this phase; SMP remains out of scope.

## 3. Slot metadata model

Add a per-slot generation array and explicit slot state:

- `FREE`: available for reservation; no published TCB exists.
- `RESERVED`: held by the one unpublished scheduler transaction.
- `ACTIVE`: published TCB, including READY/RUNNING/BLOCKED/TERMINATED lifecycle states.
- `EXHAUSTED`: generation reached `UINT32_MAX`; slot is permanently unavailable rather than wrapping.

Generation starts at 1. On successful retirement, increment the generation before marking the slot FREE. If the current generation is `UINT32_MAX`, mark EXHAUSTED and never reuse that slot. A failed unpublished reservation returns to FREE without publishing a handle; whether to advance generation on abort must be decided and tested before implementation.

Keep a high-water bound for scanning the static table. Do not assume all slots below the bound are occupied. Existing `thread_count` semantics must be audited before changing its meaning; if compatibility requires it to remain a high-water count, add a separate active-count API rather than silently changing callers' expectations.

## 4. Allocation and publication

All creation paths must reserve a slot through one helper:

1. Search for a FREE slot below the high-water bound.
2. If none exists, append at the high-water bound if capacity remains.
3. Read that slot's current nonzero generation.
4. Mark it RESERVED and build the unpublished TCB with the exact handle.
5. For owned stacks, preserve the existing staged allocation → owner bind → attach → THREAD_OWNED commit ordering.
6. Publish the TCB as the final visibility operation and mark the slot ACTIVE.
7. On failure, abort only the exact reservation and restore the slot to FREE without exposing a public handle.

Direct legacy insertion and process-owned insertion must share the same slot reservation logic. No path may assume `reserved_thread.handle.slot == thread_count` once reuse is enabled.

## 5. Retirement gate

Introduce a reaper-facing retirement operation that accepts an exact candidate identity and proof sequence. Before retiring, it must verify:

- candidate state is an explicitly retire-eligible state, never QUEUED or QUARANTINED;
- candidate handle generation and sequence are nonzero;
- exact TCB resolves and is TERMINATED;
- current slot differs from candidate slot;
- the TCB's stored switch-away sequence equals the candidate sequence;
- no pending switch ticket names this handle;
- no switch-away FIFO entry names this exact handle+generation;
- owned stack resources were reclaimed by D3.5.3, or an explicitly approved borrowed-stack retirement policy applies;
- `stack_base`, `stack_size`, allocation token, `cpu_context`, entry and argument no longer expose a resource that may be used;
- user-thread retirement remains rejected until EL0 stack ownership and cleanup are designed.

After validation, clear the complete TCB record, advance the slot generation (or mark EXHAUSTED), and publish FREE/EXHAUSTED state atomically under the scheduler IRQ guard. A failed check changes nothing.

## 6. Reaper candidate lifecycle

Extend the candidate lifecycle without reusing a candidate record prematurely:

`FREE → QUEUED → RECLAIMED → RETIRED`

Failure path:

`QUEUED → QUARANTINED`

- `RECLAIMED` means the owned kernel stack was released and TCB resource pointers were cleared.
- `RETIRED` means the scheduler TCB was removed from the active slot table and its generation advanced.
- `QUARANTINED` is terminal for automatic processing. It must never be silently reset to FREE.
- The reaper must preserve the old handle in diagnostic history until retirement succeeds.
- Candidate compaction/removal must not invalidate indices held by callers; prefer stable candidate IDs or a bounded queue with explicit head/tail semantics.

## 7. Compatibility and audit requirements

Before implementation, audit every use of:

- `thread_count`, `next_slot`, `current_slot`;
- `scheduler_find_slot`, `scheduler_resolve_handle_unlocked`;
- `scheduler_stack_range_conflicts`;
- process-owned thread enumeration and process termination cascade;
- legacy numeric-ID APIs;
- reserved-thread append-only assumptions;
- scheduler validation and all tests that assume contiguous occupied slots.

Numeric thread IDs remain compatibility identifiers and must not be used as slot identity. All new lifecycle paths use exact handles.

## 8. Required test matrix

### Slot lifecycle
- first publication uses generation 1;
- retire invalidates the old handle;
- next reservation reuses the same free slot with generation +1;
- failed unpublished reservation does not publish or corrupt slot state;
- exhausted generation never wraps and slot remains unavailable.

### Stale identity rejection
- old handle cannot snapshot, mutate, wake, terminate, or reclaim the new TCB;
- old process/thread references cannot resolve a reused slot;
- stale FIFO event cannot acknowledge or alter a new generation;
- stale reaper candidate cannot retire the new TCB;
- duplicate retirement is rejected.

### Scheduler behavior
- round-robin skips FREE and EXHAUSTED slots;
- current slot is never retired;
- high-water and active-count semantics are stable;
- reservation/publish/abort remains atomic;
- scheduler validation checks every slot state and generation.

### Resource lifecycle
- owned stack must be reclaimed before retirement;
- borrowed stack retirement clears scheduler references only after proof;
- user thread is rejected;
- quarantine blocks retirement;
- no stack or embedded context frame is released twice.

## 9. Rollout boundary

D3.5.4 is design-only. No slot reuse, TCB retirement, generation increment, or candidate compaction is implemented by this PR. Implementation starts only after the source audit and compatibility decisions are reviewed. D3.5.3's append-only behavior remains authoritative until then.


## 10. Source audit — implementation constraints

Audit performed against the D3.5.4 parent branch on 2026-11-25.

### Confirmed scheduler assumptions

- `thread_count` is currently both the number of published TCBs and the exclusive upper bound for every scheduler scan. The table is contiguous and append-only today. Preserve its existing getter as a high-water bound; introduce a separate active count rather than changing its meaning.
- `scheduler_find_slot()` currently scans every index below `thread_count` and compares numeric IDs. It must ignore non-ACTIVE slots.
- `scheduler_resolve_handle_unlocked()` currently accepts any matching TCB in the high-water range. It must additionally require slot state ACTIVE and the per-slot generation to match.
- Stack-overlap checks, scheduling scans, state validation, process-owned thread enumeration, process termination, and snapshot lookup all iterate to `thread_count`. Each must skip FREE and EXHAUSTED slots.
- Round-robin selection currently uses modulo `thread_count`. It must retain the high-water scan bound while skipping holes; it must not modulo the active count.
- The direct metadata/thread creation path currently writes at `threads[thread_count]` and increments the count. It must reserve a FREE slot or append a new slot through one shared helper.
- The owned-thread transaction currently assigns `reserved_thread.handle.slot = thread_count`, validates that exact append-only assumption, and publishes into `threads[thread_count]`. This is a mandatory conversion point: reservation must mark a specific slot RESERVED, and publish/abort must resolve that exact reservation.
- The scheduler's single pending switch ticket and 128-entry switch-event FIFO are separate from TCB occupancy. Retirement must scan the entire live FIFO range for an exact handle+generation match; checking only the head is insufficient.
- Switch-event acknowledgement is FIFO-head-only. Retirement must not consume events on behalf of the reaper.

### Reaper and process constraints

- Reaper candidate storage is a fixed 128-entry append-only array indexed by callers. Dispatch rollback assumes a just-appended tail entry. Candidate compaction would invalidate indices and is not acceptable.
- Candidate state already includes QUEUED, QUARANTINED, and RECLAIMED, but not RETIRED. Candidate count currently acts as a high-water count and does not reclaim indices.
- Reaper dispatch acknowledges terminal and nonterminal switch events after recording or classifying them. The scheduler retirement gate still needs to defend against any exact old-generation event that remains queued.
- Process ownership is stored in each TCB as an exact process slot+generation pair. Process termination and process-thread enumeration scan the scheduler high-water range and therefore must ignore retired holes.
- The process manager has its own generation-aware slot table. This phase must not alter process-slot lifecycle or conflate process and thread generations.

### Required implementation order

1. Add per-thread-slot state and generation metadata, plus an active-thread count while retaining the high-water meaning of `thread_count`.
2. Convert every TCB creation path—including direct insertion and the unpublished owned-thread transaction—to a common reserve/publish/abort path.
3. Convert every scheduler, stack-conflict, validation, and process-owned scan to skip non-ACTIVE slots.
4. Add exact-proof TCB retirement with pending-ticket and full-FIFO exclusion, then expose it only through the reaper.
5. Add tests for slot holes, stale handles/events/candidates, generation exhaustion, reservation abort, and the unchanged high-water getter before enabling slot reuse.

No scheduler behavior is changed by this audit commit. The implementation remains gated until all five steps and the full host/ARM64 CI matrix pass.

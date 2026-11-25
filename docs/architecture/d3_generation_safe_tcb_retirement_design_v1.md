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

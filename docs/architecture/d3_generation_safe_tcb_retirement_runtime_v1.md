# D3.5.6 — Generation-Safe TCB Retirement Runtime

Target: SYNTRA OS v0.1.0  
Implementation slice: PR #97, stacked on PR #96.  
Synchronization: single-core local IRQ masking only; SMP is explicitly unsupported.

## Scope

This slice adds the final retirement gate for a kernel TCB after the existing D3.5.3 owned-stack reaper has reclaimed the stack. It does not enable slot reuse or change the high-water meaning of `scheduler_get_thread_count()`.

The reaper candidate lifecycle gains:

`FREE → QUEUED → RECLAIMED → RETIRED`

The existing `QUARANTINED` failure path remains terminal for automatic processing.

## Retirement preconditions

The reaper may request retirement only for a candidate that is:

- in `RECLAIMED` state;
- backed by an exact terminal switch-away proof with nonzero handle generation and sequence;
- being processed on the already-bound reaper execution context.

The scheduler independently verifies:

- exact active slot and generation resolve;
- TCB state is `THREAD_TERMINATED` and it is not current;
- stored switch-away sequence equals the candidate proof;
- no pending switch ticket names the exact handle;
- no live entry in the complete switch-event FIFO names the exact handle;
- the owned stack has already been reclaimed and all stack, allocation, user-stack, context, entry, and argument references are cleared;
- the candidate is not a user thread;
- process-owner metadata is internally consistent: ownerless TCBs carry a zero owner handle, while owned TCBs carry an in-range slot and nonzero generation. Process thread counts are derived by scanning ACTIVE TCBs, so clearing the TCB and decrementing active count under the same scheduler IRQ guard removes the ownership association atomically.

The retirement API does not free memory. It clears the static TCB record, decrements active count, and invalidates the old handle while holding the scheduler IRQ guard.

## Generation transition

For a successful retirement:

- if the generation is below `UINT32_MAX`, increment it and then publish slot state `FREE`;
- if the generation is `UINT32_MAX`, preserve that value and publish `EXHAUSTED`;
- never permit generation zero or wraparound.

The high-water count is unchanged. Creation remains append-only in this slice, so retired holes are skipped by all scheduler scans but are not reused yet. Slot reuse is a later, separately tested change.

## Failure behavior

Any failed scheduler gate leaves the TCB, slot metadata, generation, and candidate state unchanged. The candidate remains `RECLAIMED` so the reaper can preserve its proof history and retry only if the blocking condition is transient.

Borrowed-stack candidates remain unsupported by automatic retirement in this slice. The current reaper already quarantines them during owned-stack reclamation, so they cannot reach `RECLAIMED`. User-thread retirement is also rejected. Process-owned kernel TCBs may be retired: the process thread view is derived from ACTIVE scheduler TCBs rather than a separate retained membership list, and retirement clears owner metadata while removing the TCB from the active set in the same IRQ-guarded operation.

## Compiler and synchronization note

GCC's current documentation for `__atomic` built-ins was checked through Context7. This implementation does not introduce compiler atomics: the current supported scheduler contract is single-core, with local IRQ masking serializing scheduler and reaper state. GCC's atomic built-ins may emit runtime helper calls when a target operation is not lock-free, so adding them without a deliberate freestanding ARM64 linkage and SMP design would be inappropriate here.

## Validation added

- Reaper candidate transitions from `RECLAIMED` to `RETIRED` only after scheduler proof/resource checks pass.
- Old handle snapshot and state mutation are rejected after retirement.
- Slot generation increments and state becomes `FREE`.
- High-water count remains unchanged while active count decreases.
- Scheduler validation accepts a cleared free hole.
- Duplicate retirement is rejected.
- A borrowed-stack candidate remains quarantined and cannot be retired.
- Process-owned TCB retirement preserves scheduler validation by checking owner metadata before clearing it atomically with ACTIVE-slot removal.

Generation-exhaustion and slot-reuse tests belong to the later slot-reuse stage; no test-only state mutation hook is added here.


## D3.5.7 — Safe slot reuse rollout

- Reservation scans FREE slots below the high-water mark before extending it.
- Only a FREE slot with a nonzero generation and a fully cleared TCB record is eligible.
- RESERVED, ACTIVE, and EXHAUSTED slots are never selected.
- Publishing a reused slot increments active count but does not change high-water count; publishing the tail slot advances high-water count.
- Aborting a reservation clears the TCB record and returns the unpublished slot to FREE without advancing its generation because no handle was published.
- Process-owned staged reservations may occupy a retired hole; validation recognizes the exact RESERVED slot both below and at the high-water boundary.
- Retirement increments generation before FREE publication. UINT32_MAX transitions to EXHAUSTED and can never be reused.
- Stale handles are rejected by exact generation comparison after reuse.

The scheduler remains single-core and IRQ-serialized. This change does not add SMP safety or lock-free behavior.


## D3.5.8 — Reused-slot reservation rollback coverage

- Exercise the process-owned staged reservation path against a previously retired FREE hole.
- Validate scheduler invariants while the hole is RESERVED and the staged TCB remains unpublished.
- Abort the reservation and verify the slot returns to FREE without advancing its generation, because no handle was published.
- Re-run scheduler validation before the same hole is committed by a subsequent owned-thread creation.

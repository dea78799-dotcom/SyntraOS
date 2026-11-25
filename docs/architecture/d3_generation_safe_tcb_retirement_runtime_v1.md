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
- the candidate is not a user thread.

The retirement API does not free memory. It clears the static TCB record, decrements active count, and invalidates the old handle while holding the scheduler IRQ guard.

## Generation transition

For a successful retirement:

- if the generation is below `UINT32_MAX`, increment it and then publish slot state `FREE`;
- if the generation is `UINT32_MAX`, preserve that value and publish `EXHAUSTED`;
- never permit generation zero or wraparound.

The high-water count is unchanged. Creation remains append-only in this slice, so retired holes are skipped by all scheduler scans but are not reused yet. Slot reuse is a later, separately tested change.

## Failure behavior

Any failed scheduler gate leaves the TCB, slot metadata, generation, and candidate state unchanged. The candidate remains `RECLAIMED` so the reaper can preserve its proof history and retry only if the blocking condition is transient.

Borrowed-stack candidates remain unsupported by automatic retirement in this slice. The current reaper already quarantines them during owned-stack reclamation, so they cannot reach `RECLAIMED`. User-thread retirement is also rejected.

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

Generation-exhaustion and slot-reuse tests belong to the later slot-reuse stage; no test-only state mutation hook is added here.

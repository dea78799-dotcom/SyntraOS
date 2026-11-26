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

At this point in the rollout, generation exhaustion and slot reuse are specified but not yet exercised by the integration test. D3.5.10 below adds a host-only boundary hook and verifies the complete retirement path.


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


## D3.5.8 — Rejected owned-thread creation preserves retired slots

- Exercise the public process-owned thread creation API with an invalid stack size.
- Verify argument validation rejects the request before any slot reservation or memory allocation begins.
- Confirm the retired slot remains FREE with its generation unchanged.
- Re-run scheduler validation, then let the existing valid owned-thread creation test reuse and publish the same hole.
- The internal staged-reservation rollback remains covered by the coordinator's cleanup path; it is not exposed as a public scheduler API solely for testing.


## D3.5.9 — Owned-thread transaction rollback fault injection

The host test build defines `SYNTRA_HOST_TESTING` for the host-only `syntra_kernel` library. This enables a one-shot, internal fault-injection seam in the process/thread coordinator. The hook is not declared in a public kernel header and is not compiled into the separate freestanding ARM64 ELF target.

The reaper reclamation integration test injects a failure after each successful transaction stage:

1. STAGED stack allocation, before TCB reservation;
2. unpublished TCB reservation, before allocation-owner binding;
3. allocation-owner binding, before scheduler stack attachment;
4. scheduler stack attachment, before allocation commit;
5. THREAD_OWNED allocation commit, before READY publication.

For every injected failure, the coordinator must return the injected error after successful cleanup. Tests verify that active allocation count and used blocks return to their pre-call values, allocator and scheduler validation remain successful, the retired slot remains FREE with the same generation, active scheduler thread count is unchanged, and the process-owned thread count is unchanged.

The fifth case specifically verifies the post-commit rollback path: the unpublished TCB reservation is aborted first, then the exact THREAD_OWNED allocation is released using its owner token. Earlier cases exercise staged and RESERVED allocation cleanup. The hook is one-shot so a test failure cannot leak into a later transaction.

This is deterministic host fault injection, not a production runtime failure API. It does not claim SMP coverage; the coordinator remains serialized by local IRQ masking on the supported single-core design.


## D3.5.10 — Generation exhaustion boundary test

A host-only scheduler test hook seeds an unused FREE tail slot at
`UINT32_MAX`. The hook requires an initialized scheduler, a nonzero target
generation, an in-range slot at or below the high-water boundary, FREE state,
and a fully cleared TCB. It is compiled only with `SYNTRA_HOST_TESTING` and
is not part of the freestanding ARM64 image or a public scheduler API.

The integration test then creates a process-owned kernel thread in that slot,
runs the normal terminal switch-away, reaper dispatch, owned-stack reclaim,
and TCB retirement sequence, and verifies:

- the old exact handle no longer resolves;
- retirement preserves generation `UINT32_MAX` and publishes
  `SYNTRA_THREAD_SLOT_EXHAUSTED`, never wrapping to zero;
- scheduler validation and active/high-water accounting remain consistent;
- a subsequent thread creation skips the exhausted slot and appends a new
  tail slot; and
- the exhausted slot remains unchanged after the later allocation.

The test exercises the actual reaper-to-retirement path. It does not introduce
production mutation controls, alter generation policy, or add SMP guarantees.


## D3.5.11 — Reuse retired reaper candidate records

The reaper candidate table is bounded by `SYNTRA_MAX_THREADS`. A RETIRED
candidate no longer owns a live TCB or resource and is therefore eligible for
reuse by a later terminal event. QUARANTINED candidates remain permanently
reserved for diagnostics and are never recycled automatically. QUEUED and
RECLAIMED candidates remain unavailable until the corresponding cleanup and
retirement protocol completes.

Dispatch searches for a FREE or RETIRED record before extending the
high-water count. If every record is live or quarantined, it leaves the
scheduler FIFO head untouched. Nonterminal switch-away events are acknowledged
without consuming candidate capacity. If acknowledgement fails after a
candidate slot was reused, the previous record is restored exactly; newly
appended records are cleared and the high-water count is rolled back.

Host integration coverage performs 40 sequential owned-thread
create/switch-away/reclaim/retire cycles—more than the 32-entry candidate
capacity—while asserting the table high-water count stays stable, the same
retired candidate record is recycled, and a quarantined record remains
unchanged. This remains a single-core IRQ-serialized design; it does not claim
SMP support.


## D3.5.12 — Candidate reuse rollback on FIFO acknowledgement failure

Host-only failure injection forces the scheduler's exact switch-away FIFO
acknowledgement to fail once, after dispatch has staged a terminal event into
an already RETIRED candidate record. The integration test verifies that the
candidate's previous proof, state, and reason are restored exactly, the
candidate-table high-water count is unchanged, and the original FIFO head
remains pending. A second dispatch then acknowledges the same event and
completes reclamation and retirement normally.

The injection hook is compiled only for `SYNTRA_HOST_TESTING`; it is absent
from the freestanding ARM64 kernel. This test covers the reused-record rollback
path and does not alter production scheduler behavior.


## D3.5.13 — Full candidate table preserves the FIFO head

A host-only fixture fills the bounded candidate table with QUARANTINED
records, then presents a real terminal switch-away event at the scheduler FIFO
head. Dispatch must report no processed event, leave the candidate high-water
count at its fixed capacity, and preserve the exact FIFO head (handle,
sequence, and terminal intent). Every quarantined record is checked to remain
reserved. The fixture and its state-seeding hook are excluded from the
freestanding ARM64 build; production dispatch continues to fail closed when no
FREE or RETIRED candidate is available.


## D3.5.14 — Nonterminal events do not consume candidate capacity

A host integration test blocks a runnable borrowed-stack thread, publishes
its nonterminal switch-away proof, and dispatches exactly one event on the
bound reaper context. The test verifies the FIFO event is acknowledged, no
candidate is created for that handle, candidate high-water count is unchanged,
`discarded_nonterminal` and `acknowledged_events` each increment exactly once,
and the queued-terminal count is unchanged. After these assertions, the
fixture separately resumes and terminates the blocked thread, publishes its
terminal proof, and reclaims its owned stack before retiring its TCB. This
cleanup keeps later integration checks isolated; it is not part of the
nonterminal-event accounting assertion. Production dispatch behavior is
unchanged.


## D3.5.15 — Duplicate terminal proof replay is idempotent

A host-only scheduler fixture re-enqueues the exact terminal switch-away proof
after the reaper has already accepted it into a QUEUED candidate. Dispatch
recognizes the matching handle, sequence, and terminal intent as a duplicate,
acknowledges the replay, and increments duplicate/acknowledgement statistics
without allocating or modifying a candidate. The integration test verifies
that candidate identity, proof, state, reason, and table high-water count are
unchanged, while `duplicate_events` and `acknowledged_events` each increase
exactly once.

The event replay hook is compiled only with `SYNTRA_HOST_TESTING` and is
excluded from the freestanding ARM64 kernel. Production duplicate handling is
not changed by this test.


## D3.5.16 — Conflicting terminal proof quarantines the candidate

The host handoff integration fixture replays a terminal event for an already
known thread handle but increments its sequence. This is not an idempotent
duplicate: dispatch must preserve the original proof, transition the existing
QUEUED candidate to QUARANTINED with
`SYNTRA_REAPER_REASON_SEQUENCE_CONFLICT`, and acknowledge the conflicting
FIFO head exactly once. The test checks that the candidate table high-water
count does not grow, the original proof is not overwritten, queued accounting
decreases, quarantine accounting increases, and the event is removed from the
FIFO. The subsequent nonterminal-event assertions verify that the conflict
does not interfere with unrelated switch-away processing.

The fixture uses the existing `SYNTRA_HOST_TESTING` event replay hook. No
production reaper or scheduler behavior is changed.


## D3.5.17 — Repeated conflicting proofs preserve quarantine

After D3.5.16 quarantines a candidate for a sequence conflict, the host
integration fixture submits another terminal proof for the same handle with a
second distinct sequence. Dispatch must acknowledge the new FIFO event while
preserving the candidate's original proof, QUARANTINED state, and
`SYNTRA_REAPER_REASON_SEQUENCE_CONFLICT` reason. The candidate-table
high-water count remains unchanged, and quarantine accounting remains one
rather than counting the same candidate again. Acknowledgement accounting
increases exactly once for the additional event.

This is regression coverage for an already-quarantined record. It introduces
no production logic change and uses only the existing host-test event replay
hook.


## D3.5.18 — Quarantined candidates reject reclaim and retirement

The host reclamation integration test creates an owned thread, captures its
terminal switch-away proof, and then injects a second terminal proof for the
same handle with a conflicting sequence. Dispatch must quarantine the existing
candidate while retaining the original proof.

From the already-bound reaper execution context, the test calls both
`thread_reaper_reclaim_candidate()` and
`thread_reaper_retire_candidate()` for that candidate. Both operations must
return `SYNTRA_STATUS_PERMISSION_DENIED`. The candidate's state, reason, and
original proof must remain unchanged; the terminated TCB and its
`THREAD_OWNED` allocation must remain intact; and reclaim/retirement
accounting must not advance.

This closes the quarantine-operation guard at the public reaper API boundary.
The test uses the host-only event replay hook and makes no production logic
changes.


## D3.5.19 — Quarantine diagnostics survive candidate reuse stress

The reclamation integration test captures the complete proof identity and
quarantine reason for a candidate already quarantined after a failed resource
reclaim. It then runs forty create, terminal-switch, dispatch, reclaim, and
retire cycles through a separate RETIRED candidate slot.

The test verifies that the reusable slot is not the quarantined slot and
compares the quarantined record's state, reason, handle, sequence, and terminal
intent after every new dispatch and again after all cycles. The candidate-table
high-water count remains fixed. This confirms that normal RETIRED-slot reuse
does not overwrite reserved quarantine diagnostics, even across more cycles
than the table capacity.

This is host integration coverage only. No production reaper logic changes.


## D3.5.20 — Reaper accounting remains consistent during candidate reuse

The forty-cycle candidate-reuse stress test snapshots reaper statistics before
and after each lifecycle phase. Dispatch must add exactly one queued candidate,
one acknowledged event, and one bound-context dispatch without changing
quarantine or cumulative reclamation/retirement counters. Reclaim must remove
that queued count and increment reclaimed exactly once. Retirement must
increment retired exactly once while preserving the acknowledgement count and
all failure counters.

Each cycle also verifies that the pre-existing quarantined diagnostic count
remains stable. These assertions cover both per-cycle deltas and cumulative
counter consistency under repeated reuse. This is host integration coverage
only; production scheduler and reaper logic are unchanged.


## D3.5.21 — Ordered FIFO accounting across a later ACK failure

The host integration test publishes two terminal switch-away proofs in FIFO
order. It dispatches and fully reclaims/retires the first proof before attempting
the second. A one-shot ACK failure is injected for the second proof after its
candidate slot has become reusable.

The failed dispatch must preserve the second FIFO head and restore the reusable
candidate record byte-for-byte at the field level (proof, state, and reason).
The dispatch-attempt counter advances, while queued, acknowledged, reclaimed,
retired, and quarantine counters remain unchanged by the failed ACK. Retrying
must consume that same FIFO head exactly once, reuse the same candidate slot,
and leave lifecycle counters balanced after reclamation and retirement.

This is host-only regression coverage; production scheduler and reaper logic
are unchanged.

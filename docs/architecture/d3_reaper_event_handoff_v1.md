# D3.5.1 — Switch-Away Proof Handoff and Candidate Queue

Target: SYNTRA OS v0.1.0  
Status: bounded proof handoff implemented; actual resource reclamation remains disabled.  
Base: D3.4C owned kernel thread coordinator (PR #90).

## Scope

This phase separates the scheduler's architecture proof FIFO from the future reaper's candidate queue. It does not create a reaper execution context and does not free stacks, remove TCBs, or reuse slots.

## Scheduler FIFO contract

The scheduler exposes three operations:

- `scheduler_peek_switch_away_event()` copies the FIFO head without consuming it.
- `scheduler_ack_switch_away_event(event)` consumes only if the exact handle and sequence still match the FIFO head.
- `scheduler_take_switch_away_event()` remains an atomic compatibility dequeue; the new peek/ack pair is used by the dispatcher.

The exact acknowledgement prevents a consumer from accidentally removing a different proof event after inspecting the head. The scheduler FIFO remains bounded and retains its fail-closed overflow behavior.

## Candidate dispatcher

`thread_reaper_dispatch(max_events, processed_out)` runs in normal kernel context under the existing single-core local-IRQ serialization.

For each event:

1. Check candidate capacity before peeking or acknowledging the scheduler FIFO.
2. Copy the exact FIFO head.
3. Resolve the exact thread handle to a TCB snapshot.
4. Discard a valid event if the TCB is not TERMINATED; preemption and blocking are not terminal reaping requests.
5. Queue a TERMINATED event by exact handle and sequence.
6. Quarantine stale handles and conflicting sequences.
7. Acknowledge only the exact event after its disposition is safely recorded.

If the candidate queue is full, the dispatcher stops without consuming the FIFO head. Candidate records contain identity and proof only; they contain no TCB pointer, stack address, allocation token, or allocator resource.

## Candidate states and diagnostics

- `QUEUED`: exact terminal proof accepted for a live matching TCB.
- `QUARANTINED`: stale identity or conflicting sequence; no release is permitted.
- Duplicate exact handle + sequence events are acknowledged without creating a second record.
- A second sequence for the same handle quarantines the existing record.

The queue capacity is fixed at `SYNTRA_MAX_THREADS` (32). D3.5.1 does not retire candidate records; capacity exhaustion therefore applies backpressure and preserves later scheduler events.

## Safety boundary

This phase never calls `memory_release_owned_allocation()`. A queued candidate is not yet proof that the current execution stack is the dedicated reaper stack, nor does it prove every resource descriptor is releasable. The allocator-owned stack remains THREAD_OWNED and accounted.

The next phase must provide a permanently reserved reaper execution context, validate its active stack range, validate exact TCB and allocator ownership, and only then implement one-time exact-token release. Borrowed kernel stacks, user stacks, and the embedded ARM64 context frame remain excluded.

## Validation

Host tests exercise terminal proof queueing, exact handle/sequence preservation, nonterminal event discard, allocator retention, and scheduler/allocator invariants. ARM64/QEMU must continue to validate the existing post-`SP_EL1` proof publication path. No physical hardware validation is claimed.

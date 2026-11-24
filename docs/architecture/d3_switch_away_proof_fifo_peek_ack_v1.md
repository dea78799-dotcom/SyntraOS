# D3.5.1 — Switch-Away Proof FIFO Peek/Ack

Status: runtime prerequisite implemented; deferred reclamation remains disabled.

## Problem

D2 publishes exact `{thread_handle, sequence}` switch-away proof events into a bounded FIFO after ARM64 installs a successor `SP_EL1`. The legacy `scheduler_take_switch_away_event()` is destructive: it removes the head immediately. A future dispatcher must not pop the only proof event before it has secured a slot in the separate reap-candidate queue.

## API

- `scheduler_peek_switch_away_event(out)` copies the current FIFO head without changing queue state.
- `scheduler_ack_switch_away_event(expected)` removes the head only if both thread slot/generation and the 64-bit sequence exactly match the supplied proof token.
- `scheduler_take_switch_away_event(out)` remains available as a compatibility pop for the current single diagnostic consumer.

A mismatched acknowledgement returns `SYNTRA_STATUS_PERMISSION_DENIED` and leaves the FIFO unchanged. Invalid output/identity arguments are rejected. An empty FIFO returns `SYNTRA_STATUS_ERROR`.

## Required future dispatcher protocol

1. Peek the proof FIFO head.
2. Resolve and validate the exact thread handle and terminal state.
3. Reserve or deduplicate a candidate record in the bounded reaper queue.
4. Acknowledge the exact proof event only after candidate storage is guaranteed.
5. If candidate capacity is unavailable, do not acknowledge; leave the proof at the FIFO head and retain all resources.

The current QEMU boot diagnostic continues using the compatibility `take` API. Before enabling reclamation, it must be replaced by or coordinated with one authoritative dispatcher; two independent consumers must never drain the same proof FIFO.

## Scope limits

- No reaper candidate queue or reaper execution context is implemented here.
- No memory is released and no TCB is removed or reused.
- The proof FIFO remains bounded at 128 events and retains its fail-closed saturation behavior.
- Local IRQ masking remains single-core only; this is not an SMP queue protocol.
- The peek/ack pair is not a multi-consumer reservation primitive. The eventual dispatcher must be the sole production consumer and must perform candidate-capacity reservation before acknowledgement.

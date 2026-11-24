# Thread Switch-Away Proof V1 — Runtime Implementation

Version target: SYNTRA OS v0.1.0  
Status: D2 runtime proof publication implemented; resource reclamation remains disabled.  
Parent design: `docs/architecture/thread_switch_away_proof_v1.md`

## 1. Runtime scope

This milestone captures the exact outgoing thread identity and publishes a bounded switch-away event only after the ARM64 vector assembly has installed a different successor `SP_EL1`.

It does not free memory, remove TCBs, reuse thread slots, or run a reaper.

## 2. Scheduler record

The scheduler stores one pending outgoing record:

- exact `syntra_thread_handle_t` (`slot` and `generation`);
- monotonically increasing nonzero 64-bit sequence;
- pending-valid state.

After the assembly handoff, the completion hook moves that record into a fixed 128-entry FIFO event ring. Each event contains the exact handle, sequence, and a terminal-intent bit captured when the outgoing transition is staged. The terminal bit distinguishes an exit handoff from ordinary preemption or blocking even if the TCB is inspected later, after its state has changed.

The queue is bounded. It does not overwrite old events. The scheduler refuses to stage a new context switch when the queue has no capacity. For an ordinary preemption where the current thread is still RUNNING, the ARM64 bridge resumes the current frame without switching. If the executing thread has already been blocked or terminated and no successor can be selected, the bridge keeps the return cookie invalid and enters the existing fail-closed halt path.

Sequence exhaustion, an existing unconsumed pending record, invalid handles, and queue saturation all fail closed.

## 3. Capture points

The scheduler captures outgoing identity before clearing or replacing `current_slot` for:

- `scheduler_block_current()`;
- `scheduler_terminate_current()`;
- `scheduler_set_thread_state()` when detaching the current thread;
- `scheduler_terminate_process_threads()` when the current thread belongs to the terminated process;
- normal context-aware preemption when a different context is selected.

The ARM64 scheduler-start SVC is a special bootstrap case. The boot TCB is intentionally BLOCKED so round-robin selection cannot resume it before workers wake it. The exception bridge saves its current exception frame, selects a worker, then explicitly stages the exact blocked boot-thread handle before returning the worker context to assembly.

A blocked thread can be woken before the architecture installs a successor stack. If context-aware scheduling selects that same exact handle again, the scheduler cancels its pending proof ticket. This prevents a later unrelated context switch from incorrectly publishing proof for a thread that never left the CPU.

## 4. ARM64 proof boundary

The vector entry saves the outgoing frame and calls `syntra_arm64_exception_handle_and_select()` on the outgoing stack.

Assembly keeps the original frame address in callee-saved `x19`. If the bridge returns a different context pointer, assembly:

1. compares the selected frame with the original frame;
2. installs the selected frame pointer with `mov sp, x0`;
3. calls `syntra_arm64_thread_switch_complete()` on the successor stack;
4. restores the successor's saved registers and exception-return state.

If the bridge returns NULL or the original frame pointer, the completion hook is not called. In particular, the no-successor path returns the original frame with an invalid return cookie, so assembly halts without publishing a false switch-away event.

The hook only calls `scheduler_complete_switch_away()`. It performs no logging, allocation, deallocation, or TCB mutation. The boot scheduler test drains the event FIFO later, from the resumed boot thread, and validates nonzero handles and strictly increasing sequence values.

## 5. Validation

Host tests cover:

- blocked and terminated outgoing identities;
- exact handle and sequence publication;
- normal context-aware selection;
- explicit blocked boot-thread staging;
- no-successor behavior producing no completion event.

ARM64/QEMU validation covers the scheduler-start handoff, timer-driven worker switching, natural worker return, successor handoff, proof-event publication, and the existing desktop framebuffer smoke test.

## 6. Safety limits

- A switch-away event proves only that the outgoing kernel stack is no longer the active `SP_EL1`.
- It does not prove allocator ownership, user-stack unmapping, process teardown completion, or that every other resource is safe to release.
- The event FIFO currently has no production reaper consumer. The QEMU scheduler test drains it as a test consumer.
- No stack or context memory is freed.
- No TCB is removed and no thread slot is reused.
- The current IRQ masking model is single-core only; no SMP guarantee is made.

D3 must add immutable allocator-provenance resource descriptors and a stable-stack reaper before any memory reclamation is permitted.

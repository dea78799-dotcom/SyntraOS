# D3.5.2 — Terminal Switch-Away Proof Correlation Audit

Status: audit finding; reaper implementation remains blocked.
Base: D2 switch-away proof runtime, D3.3 deferred reaper design, D3.5.1 FIFO peek/ack.

## Executive finding

The current D2 FIFO proves that a particular thread handle left the active ARM64 kernel stack at a particular sequence. It does not encode why the thread switched away or whether that proof still belongs to the thread's terminal lifetime state when a delayed consumer later inspects the FIFO.

The current event payload contains only:

- thread slot and generation;
- monotonically increasing switch-away sequence.

The FIFO may contain ordinary preemption and blocking events as well as terminal switch events. A future dispatcher that simply peeks an event, resolves the TCB, and observes `THREAD_TERMINATED` can misclassify an older non-terminal event as proof for a later termination if the thread was resumed and terminated after the old event was published.

This is a safety blocker: no reaper may release a stack based only on the current TCB state plus an arbitrary queued event for the same handle.

## Call-site audit

| Scheduler path | Current behavior | Terminal correlation |
|---|---|---|
| `scheduler_block_current_unlocked()` | Stages proof, then marks current thread BLOCKED | Non-terminal |
| `scheduler_terminate_current_unlocked()` | Stages proof, then marks current thread TERMINATED | Terminal |
| `scheduler_set_thread_state_unlocked()` | Stages whenever current thread changes to any non-RUNNING state | Must distinguish BLOCKED from TERMINATED |
| Context-aware preemption | Stages outgoing thread when a different context is selected | Non-terminal preemption |
| `scheduler_terminate_process_threads()` | Stages only the current thread belonging to the process, then marks every matching thread TERMINATED | Non-current owned threads receive no new terminal event |
| `scheduler_prepare_switch_away()` | Explicitly stages BLOCKED or TERMINATED thread | Depends on exact architecture handoff semantics |

## Required correction before candidate dispatch

The next runtime design must bind proof to the terminal lifetime, not infer terminality from a delayed snapshot.

At minimum:

1. Distinguish terminal-intent switch records from ordinary block/preemption records.
2. Invalidate any remembered switch-away proof when a thread becomes current again.
3. Preserve the exact latest switch-away proof sequence on the TCB or an equivalent immutable identity record.
4. When a non-current thread is terminated by process cascade, prove it had already left the CPU and has not been resumed since; otherwise quarantine/retain it.
5. For a current thread terminated by the scheduler, publish terminal proof only after the architecture post-SP handoff.
6. Ensure a stale event for an earlier non-terminal switch can never satisfy a later terminal candidate.
7. Keep the peek → reserve candidate capacity → exact ack protocol; do not pop the only proof before candidate storage is secured.

The final representation may use a terminal bit plus a per-thread last-switch sequence/epoch, or a separate scheduler-owned terminal proof record. It must be selected only after the state transitions and ARM64 vector paths are reviewed together.

## Required tests

- A thread is preempted, its non-terminal proof remains queued, the thread runs again, and it is later terminated: the old event must not authorize reclamation.
- A blocked thread's event cannot be interpreted as terminal proof.
- A current thread terminated through `scheduler_terminate_current()` receives terminal proof only after successor SP_EL1 is installed.
- Process termination with multiple owned threads: current and non-current threads receive distinct valid proof treatment; no non-current thread is accepted without a prior still-valid switch-away proof.
- A thread resumed after a recorded switch-away invalidates the earlier proof for later terminal use.
- Stale handle, mismatched sequence, and duplicate event remain fail-closed.
- No successor/halt path produces no post-switch terminal proof.
- Existing QEMU boot diagnostic remains a single consumer until a production dispatcher replaces it.

## Reaper gate

Do not implement candidate-to-free processing until terminal proof correlation is implemented and tested. The existing D3.3 constraints remain in force: a dedicated stable reaper stack/context, no reaping in IRQ/post-switch context, exact allocator token and owner checks, no user-stack release, no TCB slot reuse, and single-core-only synchronization.

No memory is released by this audit. No reaper behavior is enabled.

# SYNTRA OS Thread Management

Version: 0.1.0
Status: Generation-safe TCB retirement and process-owned kernel-stack reclamation are implemented for the supported single-core prototype.

## 1. Overview

A thread is the smallest unit of CPU execution managed by the SYNTRA OS
scheduler. The current scheduler prototype stores bounded TCB records,
supports READY/RUNNING/BLOCKED/TERMINATED metadata, and integrates an ARM64
exception-context selection path.

## 2. Current thread information

The current TCB stores:
- Thread ID and priority metadata.
- Thread lifecycle state.
- Entry function and argument.
- Caller-provided kernel stack bounds.
- Optional user stack bounds.
- Saved architecture CPU-context pointer.
- Optional generation-aware process owner handle.

The scheduler table is fixed at 32 entries. Fully reaped slots may be reused
with an advanced generation; slots at generation UINT32_MAX are permanently
EXHAUSTED. Only explicitly allocator-owned process kernel stacks participate
in the reaper reclamation path. Borrowed stacks remain caller-owned.

## 3. Thread states

- READY: eligible for scheduler selection.
- RUNNING: selected as the current scheduler thread.
- BLOCKED: waiting for an explicit wake operation.
- TERMINATED: no longer eligible for scheduling.

TERMINATED does not mean that stack memory is immediately safe to release.
The CPU may still be executing on that stack until an architecture scheduling
boundary switches to another saved context.

## 4. Current exit behavior

ARM64 initial kernel contexts use `syntra_arm64_thread_exit()` as their return
trampoline. It marks the current TCB terminated and waits for the timer/exception
bridge to select another context. The architecture bridge publishes an exact
post-switch-away event only after installing a successor stack.

The dedicated Thread Reaper consumes that proof, validates process ownership,
reclaims only an explicitly owned kernel stack, clears the TCB's resource
references, and retires the TCB. Process termination cascade still marks all
threads with the exact owner handle terminated; reclamation is deferred until
each thread independently satisfies the switch-away and ownership gates.

## 5. Resource ownership

Thread creation supports caller-provided buffers and a process-owned kernel
stack path backed by Memory V2. The reaper releases only the exact committed
allocation after switch-away proof and owner validation. Borrowed stacks,
user-stack resources, and arbitrary CPU-context storage are not reclaimed by
this path.

## 6. Next lifecycle design

The separate design audits
[`thread_exit_reclamation_v1.md`](thread_exit_reclamation_v1.md) and
[`thread_resource_ownership_v1.md`](thread_resource_ownership_v1.md) define
the two-stage lifecycle and the explicit allocator-ownership contract:

1. TERMINATED: the thread is no longer runnable.
2. REAPABLE: the architecture has switched away and no active or saved context
   can reference the thread's resources.

The reaper runs on a different stable kernel stack, validates switch-away
completion, releases only explicitly owned resources, and retains quarantined
metadata if cleanup fails. Generation-aware handles prevent stale references
when retired TCB slots are reused. RETIRED candidate records are recyclable;
QUARANTINED records remain reserved for diagnostics.

## 7. Security and limitations

This prototype does not establish full process isolation, SMP safety, user
thread reclamation, or production readiness. Local IRQ masking is a single-core
serialization mechanism, not a cross-core lock. See
[`scheduler.md`](scheduler.md) and
[`thread_exit_reclamation_v1.md`](thread_exit_reclamation_v1.md) for the
current architecture constraints and implementation gates.

# SYNTRA OS Thread Management

Version: 0.1.0
Status: Scheduler prototype implemented; complete lifecycle and reclamation remain in design.

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

The scheduler table is fixed at 32 entries. Registered stack ranges remain
reserved until scheduler reset. Thread records and stack resources are not
currently reaped or reused.

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
bridge to select another context. This is prototype behavior; it is not yet a
complete synchronous exit or deferred-reaping protocol.

Process termination cascade marks all threads with the exact owner handle
terminated. It does not free stacks, saved contexts, or TCB records.

## 5. Resource ownership

Thread creation currently accepts caller-owned buffers. The scheduler records
their ranges but does not allocate or free them. There is no allocator token or
resource-release callback for kernel stacks, user stacks, or CPU contexts.
The kernel must not free a resource unless ownership was explicitly transferred
to a compatible allocator.

## 6. Next lifecycle design

The separate design audits
[`thread_exit_reclamation_v1.md`](thread_exit_reclamation_v1.md) and
[`thread_resource_ownership_v1.md`](thread_resource_ownership_v1.md) define
the two-stage lifecycle and the explicit allocator-ownership contract:

1. TERMINATED: the thread is no longer runnable.
2. REAPABLE: the architecture has switched away and no active or saved context
   can reference the thread's resources.

A future reaper must run on a different stable kernel stack, validate
switch-away completion, release only explicitly owned resources, and retain
retry metadata if cleanup fails. Generation-aware thread handles are required
before TCB slots or thread IDs can be reused.

## 7. Security and limitations

This prototype does not establish full process isolation, SMP safety, automatic
resource reclamation, or production readiness. See
[`scheduler.md`](scheduler.md) and
[`thread_exit_reclamation_v1.md`](thread_exit_reclamation_v1.md) for the
current architecture constraints and implementation gates.

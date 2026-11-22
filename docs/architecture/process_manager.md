# SyntraOS Process Manager

Version: 0.1.0
Status: Metadata prototype; scheduler and address-space integration pending.

## 1. Overview

The Process Manager maintains the initial process table. The current implementation
uses a fixed-capacity array and provides process creation, lookup, state changes,
termination, and process-count queries.

## 2. Implemented in the current prototype

- Fixed capacity of 64 process records.
- Non-zero process IDs and duplicate-ID rejection.
- READY, RUNNING, BLOCKED, and TERMINATED metadata states.
- Process lookup and state-transition API.
- Host tests for initialization, creation, lookup, duplicate IDs, state changes,
  termination, and capacity limits.

## 3. Not yet implemented

The current process record contains only an ID, priority, and state. It does not yet
own or reference:

- One or more scheduler threads.
- A protected virtual address space or page-table root.
- User and kernel resource handles.
- IPC endpoints or capability permissions.
- Exit status, parent/child relationships, or resource-reclamation metadata.

Changing a process state currently does not automatically change any scheduler thread
state. Process termination does not yet reclaim memory or terminate associated threads.

## 4. Required integration sequence

1. Define the ownership relationship between a process and its threads.
2. Add validated thread-to-process association without weakening scheduler invariants.
3. Make process state transitions coordinate with associated scheduler threads.
4. Define process address-space ownership and its interaction with the ARM64 MMU.
5. Add explicit resource teardown and failure rollback.
6. Add host tests for partial creation failures, lifecycle transitions, and consistency
   between process and scheduler state.
7. Add QEMU validation only after the host-side lifecycle contract is stable.

## 5. Safety and correctness requirements

- Reject invalid and duplicate process identifiers.
- Do not expose mutable process-table internals to unrelated subsystems.
- Do not mark a process TERMINATED while its threads remain runnable.
- Do not reclaim stacks, page tables, or IPC resources until no execution context can
  reference them.
- Failed process creation must leave no partially registered process or thread.
- Keep architecture-specific context switching outside the generic process manager.

## 6. Current status

This is a metadata-only prototype, not a complete process manager. The next engineering
milestone is process-to-thread ownership and lifecycle integration. Address-space
isolation and complete resource reclamation remain later work.

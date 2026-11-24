# SyntraOS Process Manager

Version: 0.1.0
Implementation status: V1 metadata prototype

## 1. Purpose

The Process Manager currently tracks a bounded set of process records. It is an early kernel prototype, not yet a complete process lifecycle or isolation subsystem.

## 2. Implemented in V1

- Initialize a fixed-capacity table of up to 64 process records.
- Create a process record with a nonzero ID and priority.
- Reject duplicate process IDs and capacity overflow.
- Look up a process by ID.
- Track READY, RUNNING, BLOCKED and TERMINATED metadata states.
- Enforce basic state transitions among READY, RUNNING, BLOCKED and TERMINATED; TERMINATED records cannot be restarted.

## 3. Not implemented yet

The current `syntra_process_t` contains only process ID, priority and state. It does not yet own or manage:

- An address space or page-table root.
- A process-specific memory map or resource table.
- Threads or scheduler ownership.
- File descriptors, handles or kernel objects.
- Credentials, capabilities or access-control policy.
- Parent/child relationships, exit status or process reaping.
- SMP/FIQ-safe synchronization, thread ownership, or lifecycle synchronization.

A process record is therefore metadata only. Creating one does not create an independently executable or isolated process.

## 4. Security and isolation

Per-process virtual address-space isolation remains a design requirement, not a V1 capability. Do not treat process IDs as authenticated identities or as an authorization boundary.

## 5. Integration

The kernel initialization sequence initializes the Process Manager, but the current Process Manager API is not yet connected to Scheduler resource ownership or IPC endpoint authorization.

## 6. Validation scope

The process unit tests cover initialization, record creation and lookup, duplicate-ID rejection, state changes, termination metadata, and snapshot-copy isolation. These tests do not demonstrate address-space isolation, process execution, permission enforcement or concurrent safety.

## 7. V1 status

The metadata prototype is implemented and unit-tested. Full process creation, isolation, resource ownership, cleanup and security enforcement remain future work.


## Snapshot API

New code should use `process_get_snapshot()` to copy process metadata into
caller-owned storage. The snapshot is independent of the manager's internal
record and cannot be used to mutate manager state. The legacy
`process_get()` mutable-pointer accessor remains for compatibility only and
must not be used by new code. Snapshot copying does not itself provide synchronization. Public operations use
local IRQ masking on the current single-core ARM64 target; this is not SMP or
FIQ protection.


## Synchronization

Public Process Manager operations now mask local IRQ exceptions while
reading or mutating the fixed process table, using the same nested
DAIF-preserving pattern as the page allocator. This only serializes access
against local IRQ re-entry on the current single-core ARM64 target. It is not
an SMP lock and does not protect FIQ or multiple CPU cores.

New code should use `process_get_snapshot()`. The compatibility
`process_get()` accessor returns a pointer after IRQ state has been restored;
do not retain or mutate that pointer.


## Generation-aware identity API (V2 Phase 1)

The first V2 identity increment adds `syntra_process_handle_t`, containing a
32-bit slot and a 32-bit generation. Each occupied process-table slot stores
its current generation. New code should create a process with `process_create_with_handle()` to receive
a generation-aware handle directly. Existing records can be resolved from a
legacy ID with `process_get_handle()`. Callers can then use `process_get_snapshot_by_handle()` or
`process_set_state_by_handle()`. Both handle APIs reject out-of-range slots,
zero generations, unoccupied slots, and generation mismatches.

The initial generation is one. Slot reuse is deliberately disabled: there is
no reaping API and no resource-ownership cleanup contract yet. Consequently,
generation advancement and stale-handle rejection after actual slot reuse
remain unimplemented. This phase establishes validation primitives only.

Legacy integer-ID APIs remain for migration compatibility. The mutable
`process_get()` pointer remains unsafe to retain or mutate after the function
returns; new code should use generation-aware handles and caller-owned
snapshots. The API still provides metadata tracking, not process isolation or
authenticated identity.

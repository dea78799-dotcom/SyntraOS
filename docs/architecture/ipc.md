# SyntraOS Inter-Process Communication (IPC)

Version: 0.1.0
Implementation status: V1 bounded message-queue prototype

## 1. Purpose

The current IPC module provides a small, fixed-capacity message queue as an early kernel prototype. It is not yet a secure, process-authenticated IPC subsystem.

## 2. Implemented in V1

- Initialize a static queue with a capacity of 64 messages.
- Copy message payloads into kernel-owned queue entries.
- Support payloads from 1 through 256 bytes.
- Record caller-supplied sender and receiver IDs.
- Receive and remove the first queued message matching a receiver ID.
- Reject zero IDs, null pointers, zero-length messages and oversized payloads.
- Return an error when the queue is full or no matching message exists.

## 3. Not implemented yet

- Validation that sender and receiver IDs refer to live processes.
- Authentication of the sender identity from the calling execution context.
- Endpoint creation, ownership and access-control permissions.
- Capabilities or policy-based authorization.
- Per-process queues, blocking receive, wait queues or notifications.
- Message prioritization, quotas or denial-of-service controls.
- SMP-safe locking or interrupt-safe queue synchronization.
- Lifecycle cleanup when a process terminates.

The sender ID is currently caller-supplied metadata. It must not be trusted as proof of identity.

## 4. Security model

The current queue is suitable only for controlled prototype tests. It must not be used as a security boundary between mutually untrusted processes until identity binding, endpoint authorization, resource limits and synchronization are implemented and tested.

## 5. Validation scope

The IPC unit tests cover initialization, message enqueue/dequeue, sender and receiver metadata, payload copying, queue count and rejection of a zero-length message. They do not demonstrate permission enforcement, process authentication, isolation, concurrency safety or resilience against malicious senders.

## 6. V1 status

The bounded message-queue prototype is implemented and unit-tested. Secure process-integrated IPC remains future work.


## Synchronization

The public IPC initialization, send, receive, and pending-count operations
now preserve and mask local IRQ state around message-table access. This
prevents local IRQ re-entry from observing a partially updated queue on the
current single-core ARM64 target. It is not an SMP lock and does not provide
cross-core synchronization.

IPC still accepts caller-supplied nonzero sender and receiver IDs. It does
not verify that either process exists, authenticate the sender, enforce
endpoint permissions, or automatically remove messages when a process
terminates. These are intentionally deferred until Process Manager identity,
lifecycle, and cleanup contracts are designed.

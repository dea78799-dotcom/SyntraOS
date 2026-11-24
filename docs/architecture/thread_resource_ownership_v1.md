# Thread Resource Ownership V1 — Phase B Design Audit

Version target: SYNTRA OS v0.1.0
Status: Design proposal; no allocator ownership transfer or resource release is implemented.
Base: ARM64 Thread Exit Correctness V1

## 1. Decision summary

The scheduler must distinguish a recorded address range from ownership of the
memory in that range. A stack pointer, size, or saved-context pointer does not
prove that the kernel may release the referenced storage.

Phase B will introduce explicit resource descriptors, but this document does
not authorize the scheduler or reaper to free memory. Release remains disabled
until a descriptor can be validated against the allocator that created it and
the architecture has marked the thread REAPABLE.

Initial implementation should support only the existing general memory-pool
allocator. Physical-page ownership is deferred because the current page
allocator exposes one-page-at-a-time allocation and no contiguous multi-page
stack allocation contract.

## 2. Existing allocator contracts

### General memory pool

- `memory_allocate(blocks)` allocates a contiguous run of fixed 4096-byte
  blocks from the static memory pool.
- It returns a base pointer and records the exact allocation size internally.
- `memory_release(address, blocks)` requires the exact base pointer and exact
  block count. A mismatched size or interior pointer is rejected.
- The API does not return an opaque allocation token or generation.
- The allocator is not a thread-resource owner today; callers currently pass
  arbitrary caller-owned stack buffers to scheduler creation.

Implication: an owned descriptor must preserve both the exact base pointer and
exact block count. A byte size alone is insufficient to call the release API
safely. The descriptor must not be forged from a raw pointer.

### Physical page allocator

- `syntra_page_alloc(out)` allocates exactly one 4096-byte physical page.
- `syntra_page_free(address)` releases one aligned page previously allocated.
- The allocator tracks allocated pages in a bitmap, but exposes no allocation
  object, generation, contiguous-run API, or multi-page release operation.
- The boot page-table builder currently uses this allocator for individual
  page-table pages.

Implication: do not use this allocator for multi-page kernel or user stacks
until a contiguous-run API and ownership token are designed. Do not infer that
adjacent successful allocations are physically contiguous.

## 3. Stack and context relationship

- Kernel-thread initial CPU context is placed at the top of its kernel stack by
  `syntra_arm64_context_initialize()`.
- User-thread initial CPU context is also placed on its kernel stack by
  `syntra_arm64_user_context_initialize()`.
- The TCB's `cpu_context` is therefore an interior pointer into the kernel
  stack for these constructors, not a separate allocation.
- User stack storage is independent from the kernel stack.
- A kernel thread has no separate user-stack resource.

Implication: never represent the context frame as independently owned when it
resides inside the kernel stack. Doing so would create duplicate release claims
for overlapping memory. The kernel-stack descriptor owns the whole allocation;
the context pointer is a borrowed subrange/reference into it.

## 4. Proposed resource descriptor

The first implementation should use a closed allocator-kind enum and a
role-specific descriptor. Suggested shape (not yet a public ABI):

```c
typedef enum {
    SYNTRA_RESOURCE_BORROWED = 0,
    SYNTRA_RESOURCE_MEMORY_POOL_BLOCKS = 1
} syntra_resource_allocator_t;

typedef enum {
    SYNTRA_THREAD_RESOURCE_KERNEL_STACK = 1,
    SYNTRA_THREAD_RESOURCE_USER_STACK = 2
} syntra_thread_resource_role_t;

typedef struct {
    void *base;
    size_t size_bytes;
    unsigned int allocation_units; /* exact memory-pool block count */
    syntra_resource_allocator_t allocator;
    syntra_thread_resource_role_t role;
    unsigned int flags;
} syntra_thread_resource_t;
```

Required validation rules:

- BORROWED descriptors never invoke a release function.
- MEMORY_POOL_BLOCKS descriptors require non-NULL base, nonzero exact block
  count, and `size_bytes == allocation_units * SYNTRA_MEMORY_BLOCK_SIZE`.
- Multiplication must be overflow-checked before comparison.
- Only known allocator and role enum values are accepted.
- A kernel-stack resource must fully contain the saved context frame range.
- A user-stack resource must not overlap the kernel-stack resource.
- Context frames are references into the kernel-stack resource, never a
  separately releasable resource for the current ARM64 context constructors.
- Descriptor validation must happen before TCB insertion; failure leaves the
  scheduler table and allocation untouched.
- Ownership transfer must be explicit at creation. A normal legacy
  `scheduler_create_thread()` call remains BORROWED.
- The descriptor records the allocator's exact release units; the reaper must
  not reconstruct units from an arbitrary caller-provided byte count.

## 5. Proposed API boundary

Prefer an ownership-aware creation API in the process/thread coordinator rather
than adding allocator calls to scheduler policy code.

Candidate flow:

1. Caller allocates a stack from the memory pool.
2. Caller builds the ARM64 initial context inside that stack.
3. Caller submits a descriptor explicitly marked MEMORY_POOL_BLOCKS.
4. Coordinator validates process identity and descriptor structure.
5. Scheduler stores immutable resource metadata with the TCB.
6. Exit marks the thread TERMINATED only.
7. Architecture switch-away completion changes lifecycle eligibility to
   REAPABLE.
8. A separate reaper validates the descriptor and invokes
   `memory_release(base, allocation_units)` exactly once.

The scheduler must not call `memory_release()` directly. This preserves the
separation between scheduling policy, allocation policy, and deferred cleanup.

## 6. Failure and retry policy

- Failed descriptor validation: no TCB insertion and no ownership transfer.
- Failed scheduler insertion: caller retains ownership; the coordinator must
  not release the caller's allocation as a side effect.
- Failed switch-away proof: no release attempt.
- Failed allocator release: retain descriptor and TCB state for retry; do not
  mark the resource released.
- Successful release: mark that exact resource descriptor released before any
  subsequent reaper pass can attempt it again.
- A second release attempt must be rejected by lifecycle metadata before it
  reaches the allocator.
- Kernel and user stacks are accounted independently and may be released only
  after all references to each resource are gone.

## 7. Explicit deferrals

- Physical-page stack allocation and contiguous page-run allocation.
- Opaque allocator tokens and allocation-generation validation.
- Separate context allocation; current contexts are embedded in kernel stacks.
- Stack canaries, guard pages, and stack-overflow recovery.
- SMP-safe allocator and reaper synchronization.
- TCB removal, thread-handle generations, and slot reuse.
- Actual memory release from the thread-exit path.

## 8. Required tests before enabling ownership transfer

- Borrowed stack descriptors never invoke allocator release.
- Valid memory-pool descriptor matches exact base and block count.
- Wrong block count, interior pointer, overflowed size, unknown allocator, and
  unknown role are rejected before TCB insertion.
- Kernel stack contains the context frame range.
- Context frame cannot be double-counted as a separate allocation.
- User and kernel stack descriptors are non-overlapping and independently
  tracked.
- Failed scheduler insertion leaves caller ownership unchanged.
- Failed allocator release retains retry metadata.
- Successful release is invoked exactly once.
- A TERMINATED but not REAPABLE thread never reaches allocator release.

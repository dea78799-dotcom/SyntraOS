# ARM64 Kernel VA Mapping API Contract

Status: design contract; kernel-stack mapping API and MMU activation are not implemented.

## 1. Scope

The Kernel VA reservation registry owns virtual address ranges in the proposed
64–96 GiB arena. It does not own physical pages and does not install page-table
descriptors. A future mapping layer must consume an exact live reservation and
must never infer a physical address from a virtual address.

This contract is limited to EL1 kernel mappings. It must not reuse the EL0
address-space mapping API or change the process upper-half policy.

## 2. Required mapping attributes

For kernel stack mappings, every usable page must be:
- 4-KiB granule, Normal memory.
- EL1 read/write.
- EL0 inaccessible.
- Execute-never at EL1 and EL0.
- Guard pages left invalid (no descriptor).
- Backed by physical pages whose allocator owner is the expected kernel-stack
  owner; the mapping layer must validate ownership before publishing mappings.

The reservation's guard pages are address-space reservations only. They are
not mapped and must remain translation faults after MMU activation.

## 3. API shape

The implementation should expose separate operations, not a generic user/kernel
flag:
- `kernel_map_stack(reservation, physical_pages, page_count)`
- `kernel_unmap_stack(reservation, physical_pages, page_count)`

The API must validate that:
- The reservation descriptor exactly matches a live registry entry.
- `usable_size` equals `page_count * 4096`, with checked arithmetic.
- Every VA and PA is page-aligned and the PA range is representable.
- Every backing page has the expected owner.
- The requested range lies entirely inside the reservation's usable range.
- No existing valid descriptor overlaps the target VA range.

## 4. Transactional map requirement

Mapping is all-or-nothing. A failure must not leave any leaf descriptor from
the requested range installed and must not consume or release the caller's
physical pages.

The page-table builder now provides:
- A paired allocate/release/resolve initialization path through
  `syntra_arm64_page_table_init_ex()`; the legacy initializer remains for
  compatibility and has no release callback.
- Two-phase mapping preparation: all required intermediate tables are
  prepared before any leaf descriptor is published.
- A bounded journal for newly created intermediate tables. Preparation failure
  rolls back attached tables in reverse order when a release callback exists.
- If a release callback fails, the descriptor is restored and rollback stops,
  preserving the still-owned table and its ancestor links for retry or
  higher-level cleanup.

This closes the previously documented partial-leaf publication risk for
intermediate-table allocation failures. It is not a concurrency-safe SMP
transaction, and the journal is bounded. Callers that require reliable
reclamation must use `init_ex()` with an owner-aware release callback.

The future kernel mapping layer must still validate the exact reservation,
physical-page ownership, mapping attributes, and guard-page boundaries before
calling the builder. It must not treat successful table preparation as proof
that backing pages are owned correctly.

## 5. Transactional unmap requirement

Unmap must first validate the complete range and every expected leaf descriptor,
including the exact PA and attributes. It must not clear any descriptor until
the entire range passes preflight.

After preflight, it may clear leaves and reclaim empty L3/L2 tables. It must
never reclaim a table containing unrelated mappings or borrowed kernel-root
entries. Physical stack pages are released by the stack allocator only after
the mapping layer confirms unmap and required TLB maintenance.

The generic page-table builder still has no generic unmap API. The existing
address-space user-page unmap is a separate EL0 lifecycle operation and must
not be reused as a kernel-stack unmap implementation.

## 6. TLB and active-root rules

The current system does not activate the MMU in its normal QEMU boot path.
Nevertheless, the future API must define TLB behavior before runtime use:
- If the target address space is active, invalidate the affected VA range using
  the correct ASID/VMID context and required barriers.
- If inactive, record that no local active translation can exist; SMP shootdown
  is out of scope until multi-core support exists.
- Do not free page-table or backing pages until invalidation completion.

## 7. Tests required before integration

Host tests must cover:
- EL1 RW, EL0 denied, PXN/UXN stack leaf attributes.
- Guard-page translation faults.
- Exact reservation token and owner validation.
- Misaligned, overflowed, short, and oversized backing ranges.
- Existing mapping conflict with no descriptor mutation.
- Allocation failure at each intermediate-table preparation point.
- Resolver failure and invalid allocator-result cleanup.
- Rollback of newly attached tables, including release-callback failure.
- Unmap preflight failure with no partial clear.
- Reclaim only empty tables; preserve neighboring mappings.
- Stale/double unmap rejection.

The page-table builder currently has host regression coverage for several
allocation-failure, preparation rollback, retry, and release-failure cases.
The remaining kernel-stack-specific tests above are prerequisites for stack
mapping integration.

QEMU testing must later prove actual translation behavior and guard-page faults.
A host software walker alone is not hardware/MMU validation.

## 8. Current implementation boundary

Implemented groundwork:
- Bounded kernel VA reservation registry in the proposed 64–96 GiB arena.
- Page-table allocate/release/resolve callback support.
- Two-phase intermediate-table preparation and bounded rollback journal.
- Owner-aware page-table reclamation in boot and address-space setup.
- EL0 address-space validation, user-page unmap, and retry-safe destruction.

Not implemented:
- Kernel-stack map/unmap API consuming reservation tokens.
- Generic page-table unmap and empty-table pruning for kernel mappings.
- TLB invalidation/shootdown integration.
- MMU activation in the normal boot path.
- Kernel stack allocator integration with virtual mappings.

Therefore, the current code is not ready for active-MMU kernel-stack mapping.

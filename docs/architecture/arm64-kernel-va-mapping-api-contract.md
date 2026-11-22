# ARM64 Kernel VA Mapping API Contract

Status: design contract; no mapping implementation or MMU activation.

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

The current page-table builder preflights existing leaf conflicts, but its
allocation callback has no matching release callback. It can allocate
intermediate tables while constructing a range, so an allocation/resolve/
descriptor failure after construction begins may leave empty intermediate
tables behind. A simple loop around the existing map API is therefore not a
sufficient transactional implementation.

Before adding the kernel mapping API, the page-table builder must gain a
well-defined table-page reclamation contract (allocate/release/resolve), or
implement a two-phase plan that allocates and validates every required table
page before publishing any parent descriptor. Rollback must restore all
parent and leaf descriptors and release only newly allocated table pages.

## 5. Transactional unmap requirement

Unmap must first validate the complete range and every expected leaf descriptor,
including the exact PA and attributes. It must not clear any descriptor until
the entire range passes preflight.

After preflight, it may clear leaves and reclaim empty L3/L2 tables. It must
never reclaim a table containing unrelated mappings or borrowed kernel-root
entries. Physical stack pages are released by the stack allocator only after
the mapping layer confirms unmap and required TLB maintenance.

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
- Allocation failure at every intermediate-table allocation point.
- Resolver and descriptor-construction failures.
- Full rollback of parent descriptors and newly allocated tables.
- Unmap preflight failure with no partial clear.
- Reclaim only empty tables; preserve neighboring mappings.
- Stale/double unmap rejection.

QEMU testing must later prove actual translation behavior and guard-page faults.
A host software walker alone is not hardware/MMU validation.

## 8. Current implementation gap

The current `syntra_arm64_page_table_map()` provides overlap preflight and a
software-walker verifier, but it has no table-page release callback and no
unmap API. Its current preflight prevents known leaf overlaps; it does not
provide full rollback for every mid-build allocation failure. This gap must
be closed before stack mappings are integrated.

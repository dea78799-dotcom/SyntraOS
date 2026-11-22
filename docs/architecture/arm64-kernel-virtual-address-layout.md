# ARM64 Kernel Virtual Address Layout (Proposal v2)

Status: design proposal only. No page tables changed and MMU activation remains disabled.
Base revision: stack-registry-capacity-tests-v9.
Architecture scope: AArch64, 4-KiB granule, 39-bit TTBR0 VA regime (T0SZ=25).

## 1. Verified address-regime constraints

The current MMU helper declares:
- 4-KiB pages and 512 table entries.
- 39-bit VA and 48-bit PA.
- Three 9-bit translation indices plus 12-bit page offset.
- The address-space module reserves the upper 256-GiB range for EL0:
  `SYNTRA_ARM64_USER_VA_BASE = 1 << 38` (256 GiB),
  `SYNTRA_ARM64_USER_VA_LIMIT = 1 << 39` (512 GiB).
- Kernel-root inheritance currently copies root entries 0..127 and requires entries
  128..511 of the kernel root to be zero. With the current L1 root granularity of 1 GiB,
  entries 0..127 cover 0..128 GiB—not 0..256 GiB. Therefore inherited kernel mappings
  must stay within 0..128 GiB under this mechanism.
- Root entries 128..255 cover 128..256 GiB and are a reserved gap under the current
  inheritance validation. Root entries 256..511 cover the existing EL0 range.
- The existing MMU mapping-plan builder is identity-oriented for RAM, kernel sections,
  DTB, and discovered MMIO. This proposal does not relocate or change existing mappings.

## 2. Proposed address partition

All values are hexadecimal and ranges are half-open.

| Region | Range | Purpose | Status |
|---|---|---|---|
| Low-address identity mappings | 0x000000000000–0x000000FFFFFFFF | Broad low-address envelope for bootstrap/platform mappings; not a claim that every address is currently mapped | Reserved envelope; actual ranges are plan-driven |
| Kernel VA arena | 0x0000001000000000–0x0000001800000000 | Future dynamic kernel mappings, including thread stacks | Proposed |
| Kernel-inherited region | 0x0000000000000000–0x0000002000000000 | Root entries 0..127 (0–128 GiB); bootstrap and future kernel mappings | Constraint of current inheritance |
| Reserved root-entry gap | 0x0000002000000000–0x0000004000000000 | Root entries 128..255 (128–256 GiB) | Reserved by current validation |
| EL0 user region | 0x0000004000000000–0x0000008000000000 | Existing user VA range | Existing API |

The proposed arena is 32 GiB, from 64 GiB to 96 GiB. It lies inside the currently
inherited 0–128-GiB region and is far above the current low physical identity ranges.
The 32-GiB size and base are proposals only; exact overlaps with RAM, reserved-memory,
linker, and mapping-plan ranges must be audited before implementation.

### Mapping-plan audit (current v9 baseline)

- The CI QEMU command configures 512 MiB of RAM. The physical RAM base and full range
  remain FDT-derived; the CI script does not assert the complete FDT RAM range.
- The memory-map discovery splits RAM into typed regions and boot later reserves the
  complete linker-defined kernel image. The DTB and FDT reservations are also tracked.
- The mapping-plan builder creates identity mappings for RAM, RESERVED, and RECLAIMABLE
  regions as Normal RW/XN, then adds kernel text/rodata/data/BSS/stack with explicit
  permissions, the DTB read-only, and discovered UART/fw_cfg/VirtIO/GIC windows as
  Device RW/XN. Conflicting overlaps are rejected.
- Existing host mapping-plan fixtures place the kernel near 0x40200000 and MMIO below
  0x10000000; the proposed 64–96-GiB VA arena does not overlap those fixture ranges.
  This fixture result is not a substitute for checking the runtime FDT map.
- The proposed arena is not physical RAM and must not be identity-mapped or allocated
  as physical backing. It is only a future virtual-address reservation.

VA does not imply backing PA. API records must distinguish virtual and physical addresses.

## 3. Arena subdivision (proposal)

A future reservation registry manages all ranges; no caller maps directly without a
reservation.

- Kernel thread stacks: dynamic reservations, with guard-page space when supported.
- Kernel heap: separate future sub-arena; outside this milestone.
- Temporary mapping window: reserved for future, not implemented.
- Unassigned arena capacity: unavailable until explicitly granted by registry.

V1 stacks are fixed-size and page-rounded; no automatic growth. Guard faults are
diagnostic and recovery policy is separate.

## 4. Mapping attributes

- Stack usable pages: Normal memory, EL1 read/write, execute-never, EL0 inaccessible.
- Guard page: no valid descriptor.
- Kernel text: read-only, executable.
- Kernel rodata: read-only, execute-never.
- Kernel data/BSS: read/write, execute-never.
- MMIO: Device, read/write as required, execute-never.
- User mappings: remain in existing upper-half range with explicit user permissions.

No mapping is activated by this proposal.

## 5. Reservation rules

The future VA registry must reject zero-size, unaligned, overflow, out-of-regime,
overlapping, stale, or double-release requests. It must track owner, allocation ID,
requested and rounded size, and full reserved range including guard pages. It must use a
documented deterministic allocation policy, bounded capacity, and clean exhaustion.
Release is forbidden until unmapping is confirmed and references are gone. The arena
must be reserved before any dynamic mapping operation.

## 6. Current blockers

1. Confirm exact QEMU FDT RAM/reserved ranges and current identity mappings.
2. Confirm selected T0SZ/TTBR0 regime and target CPU support.
3. Define kernel root creation and identity-map L1 entries.
4. Implement EL1-only map/unmap; current user-map APIs require PROCESS-owned pages.
5. Implement page-table page allocation/resolution and ownership for kernel root.
6. Add VA reservation registry and rollback tests.
7. Define invalidation and synchronization; SMP/remote TLB shootdown is out of scope.
8. Do not consume arena until QEMU proves translation active.

## 7. Validation gates

Host tests must cover boundaries, overflow, alignment, overlaps, guard arithmetic,
exhaustion, release/reuse, stale release, and no mutation on failure. QEMU tests later
must prove actual mapped-stack access, EL0 denial, NX, guard fault, context switching,
and leak-free cleanup. Host tests/cross-compile alone are not hardware evidence.

## 8. Decision record

Reviewable proposal, not final ABI. Changing base/size before implementation is permitted
with boundary tests and compatibility review. This document-only PR must not enable MMU
or alter current mappings.

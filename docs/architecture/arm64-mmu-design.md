# SyntraOS ARM64 MMU Design

Status: Design baseline, not yet enabled
Target: ARMv8-A AArch64, EL1
Initial platform: QEMU `virt` (platform details must be confirmed from the supplied FDT)

## 1. Purpose

This document defines the safety and compatibility requirements for adding EL1 stage-1
translation, page-table management, and physical-to-virtual MMIO mappings.

The current ARM64 memory allocator is a fixed 4 KiB block pool. It is not a page-table
allocator and does not provide physical/virtual address translation. The MMU work must
therefore remain a separate architecture subsystem until a physical page source and
its ownership rules are defined.

No GIC register access may be enabled until MMIO mappings with Device memory attributes
are available and validated.

## 2. Verified current state

- The linker places the kernel at physical-looking address `0x40200000`.
- The boot entry sets SP, installs VBAR_EL1, clears BSS, and calls the C boot stage.
- The boot entry checks that CurrentEL is EL1.
- The boot entry does not read SCTLR_EL1 and does not establish an MMU state contract.
- The linker exports kernel start/end and stack boundaries, but no page-table region.
- The existing memory module exposes a 4 KiB fixed-block pool and allocation diagnostics.
- The FDT parser can discover device register ranges, but this does not make those ranges
  safely accessible through the CPU address space.
- The FDT parser does not yet expose a validated RAM/reserved-memory map for the MMU subsystem.

These facts are not proof that the current image has booted on QEMU or physical hardware.

### CI and boot-test audit

The current `.github/workflows/ci.yml` has two relevant jobs:

- Host Debug and Release builds plus their unit tests.
- ARM64 cross-compilation, ELF architecture verification, and a QEMU boot smoke test.

The ARM64 job installs QEMU and GDB, starts the ELF on `virt` with a Cortex-A57 CPU and
GICv3, and checks the return status at a GDB breakpoint after
`syntra_arm64_boot_main`. The test succeeds only when the boot function returns
`SYNTRA_STATUS_OK`.

The test currently validates boot-stage execution and FDT/GIC discovery under this
emulated configuration. It does not prove MMU activation, actual GIC MMIO access,
interrupt delivery, or physical-device boot. The QEMU test also does not yet validate
the DTB's full RAM map against linker reservations.

The ARM64 toolchain targets generic AArch64 bare-metal compilation with
`-march=armv8-a`; the QEMU test selects Cortex-A57 explicitly.

The entry assembly supplies the DTB pointer as `0x40000000`, matching the current QEMU
ELF/bare-metal boot setup. This is a platform-and-loader assumption, not a general
ARM64 boot contract, and must change if the loader or boot mode changes.


## 3. Boot contract required before enabling translation

The ARM64 entry path must eventually document and validate all of the following:

1. Entry exception level is EL1.
2. The state of SCTLR_EL1.M (MMU enable) is known.
3. The address regime and translation tables inherited from an earlier stage are known,
   or the boot contract explicitly requires translation disabled.
4. The kernel image, active stack, BSS, vector table, and DTB remain accessible during
   the transition.
5. The transition code executes from memory that remains mapped at the same address
   until the branch to the post-MMU execution address.
6. Interrupts remain masked until vector handling and required device mappings are ready.

The implementation must not silently assume that the MMU is disabled. If the selected
boot protocol cannot guarantee that state, entry code must detect and reject unsupported
states rather than overwrite unknown translation settings.

## 4. Initial translation design

The first implementation should use:

- AArch64 EL1 stage-1 translation.
- 4 KiB translation granule, subject to checking the CPU's supported granules.
- A conservative initial virtual-address size selected from the target's supported
  address size and represented consistently in TCR_EL1.
- Identity mapping for the minimal bootstrap range: kernel image, stack, page tables,
  vector table, and DTB.
- Explicit MAIR_EL1 indices for Normal memory and Device memory.
- Device-nGnRnE for conservative MMIO mappings unless platform requirements justify
  another Device type.
- Read/write and execute permissions chosen per region; executable kernel text must
  not be writable, and data/MMIO must not be executable.
- Explicit barriers and TLB invalidation in the architecturally required order when
  translation settings change.

The exact TCR_EL1, MAIR_EL1, descriptor, and SCTLR_EL1 bit values must be derived from
the selected translation regime and reviewed against the ARM architecture reference
before implementation. They must not be guessed or copied from an unrelated board.

## 5. Page-table ownership and memory source

Page tables require physically contiguous, 4 KiB-aligned pages that remain reserved
for the lifetime of the mappings that reference them.

The existing general-purpose block pool must not be treated as physical memory until
its backing address, alignment, reservation, and lifetime are explicitly established.

The first page-table implementation should use a bounded, statically reserved bootstrap
region or a dedicated physical-page allocator with explicit ownership. The choice must
be validated against linker symbols and the QEMU RAM map before any table is installed.

Allocation failure must return an error. It must never produce a partially installed
translation regime.

## 6. Mapping API requirements

The eventual API should distinguish physical and virtual addresses and expose operations
equivalent to:

- Initialize translation metadata without enabling the MMU.
- Create and validate page-table roots.
- Map a physical range at a virtual range with explicit memory type and permissions.
- Unmap a virtual range.
- Translate a virtual address for diagnostics/tests where supported.
- Report page-table configuration and whether hardware translation is active.

Mapping validation must reject zero-sized ranges, unaligned ranges where alignment is
required, arithmetic overflow, unsupported attributes, conflicting mappings, and
overlapping virtual ranges that would create ambiguous ownership.

MMIO mappings must use Device attributes and must never be executable. Device addresses
must come from the FDT or another validated platform description, not hard-coded GIC
constants.

## 7. Required implementation stages

### Stage A: Design and boot-state audit

- Confirm the actual QEMU invocation and loader/boot protocol.
- Confirm the entry register contract and MMU state.
- Confirm linker load addresses, RAM bounds, and DTB placement.
- Define the bootstrap page-table reservation and identity-mapped ranges.

Exit condition: the boot transition can be described without relying on unknown
register state or guessed physical addresses.

### Stage B: Descriptor and table primitives

- Define descriptor masks and address extraction helpers.
- Validate alignment, address width, table-level indices, and overflow.
- Add host-side tests for descriptor construction and range validation.
- Do not write translation registers in this stage.

Exit condition: deterministic host tests cover valid and invalid descriptors.

### Stage C: Bootstrap page tables

- Reserve aligned table pages.
- Build identity mappings for all currently executing code/data and the DTB.
- Apply correct Normal memory attributes to RAM and Device attributes only to validated
  device ranges.
- Verify all table pages and mapped ranges are inside known RAM/platform regions.

Exit condition: table contents pass host tests and static validation.

### Stage D: Controlled MMU activation

- Add an explicit architecture entry point that validates preconditions.
- Program MAIR_EL1, TCR_EL1, and TTBR0_EL1 in the reviewed order.
- Perform required barriers and TLB invalidation.
- Enable SCTLR_EL1.M only after all required identity mappings exist.
- Keep a safe failure path and keep interrupts masked during transition.

Exit condition: an actual QEMU boot test demonstrates execution continues after enabling
translation. A successful cross-compile alone does not satisfy this condition.

### Stage E: MMIO mapping and GIC continuation

- Add map/unmap support for FDT-discovered MMIO ranges.
- Verify Device attributes and non-executable permissions.
- Only then implement GIC register access and interrupt-controller initialization.

Exit condition: QEMU boot test confirms valid MMIO mappings before any GIC access.

## 8. Testing and safety rules

- Existing host tests must remain unchanged in behavior.
- New descriptor and mapping tests must run in the normal host CI.
- ARM64 cross-build success is reported separately from QEMU boot success.
- No physical-device flashing is part of this milestone.
- Do not enable GIC hardware access as a shortcut around missing MMU support.
- Do not remove or wholesale-rewrite existing kernel files to introduce this subsystem.

## 9. Open questions that block hardware activation

- Exact QEMU command line and firmware/loader path used by CI.
- Whether the image is entered with MMU disabled and at EL1.
- Actual RAM and reserved-memory ranges reported by the QEMU FDT.
- Whether the current linker address and DTB address match the selected boot protocol.
- Whether the target CPU supports the chosen 4 KiB granule and address size.

Until these are answered, the design remains a specification and hardware translation
must remain disabled.

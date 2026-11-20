# SyntraOS ARM64 Boot Integration

Version: 1.1.0  
Status: Design Proposal  
Target: ARM64  
Initial Platform: QEMU `virt`

## 1. Overview

This document defines the initial boot integration
design for SyntraOS on ARM64.

The first milestone targets a reproducible
development environment using QEMU ARM64 `virt`.

The implementation must preserve the existing
host-side build and test environment.

## 2. Objectives

The initial boot integration must provide:

- An ARM64 kernel entry point.
- A defined kernel memory layout.
- A linker script for the ARM64 kernel image.
- A reproducible cross-compilation environment.
- Basic early serial output for debugging.
- A QEMU boot test.
- Separate host and ARM64 build configurations.

## 3. Initial Target Platform

The first supported development platform is:

- Machine: QEMU `virt`
- Architecture: AArch64
- Initial CPU model: Cortex-A53, subject to validation
  against the installed QEMU build.
- Memory: Configurable QEMU RAM
- Output: Virtual serial console

The CPU model is explicit so that the test environment
does not silently depend on QEMU's default CPU.

Real mobile hardware is not part of V1.

Hardware-specific boot integration will be handled
in later platform-specific stages.

## 4. Boot Method

The initial implementation will use QEMU's
Generic Loader to load an ARM64 ELF kernel image.

The Generic Loader supports ELF executable files.
When `cpu-num=0` is supplied, QEMU loads the image
through CPU 0's address space and sets CPU 0's
program counter to the executable entry point.

The loader must be able to:

- Load the ARM64 ELF kernel image.
- Select the boot CPU.
- Set the initial program counter from the ELF entry.
- Transfer execution to the kernel entry point.

The loader option `cpu-num=0` must be used only
for the boot image because it also sets the CPU PC.

The initial boot command is a design example:

```sh
qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a53 \
  -m 256M \
  -nographic \
  -device loader,file=build/arm64/syntraos.elf,cpu-num=0
```

This command is not yet validated against a built
SyntraOS ELF image. QEMU version, CPU availability,
ELF load segments, and serial routing must be tested
before it is adopted by the build system.

The boot design must not assume that the CPU
always starts at a specific exception level.

The kernel entry code must inspect the current
exception level before architecture initialization.
The initial implementation must document which
exception levels and security state it supports.

## 5. QEMU virt Memory and Device Discovery

For the QEMU `virt` machine:

- Flash memory begins at `0x00000000`.
- RAM begins at `0x40000000`.
- The board generates a Device Tree Blob (DTB).
- For bare-metal boot, QEMU places the DTB at the
  beginning of RAM, currently `0x40000000`.

The kernel must reserve the DTB memory region
and must not overwrite it while setting up its
initial stack, BSS, or other memory sections.

The linker layout and ELF load segments must be
checked to ensure they do not overlap the DTB region.
The actual DTB size must be read from its header;
the design must not assume a fixed DTB length.

For this Generic Loader bare-metal path, the DTB
location is not automatically passed in register
`x0` as it is for the AArch64 Linux boot protocol.
The initial QEMU virt implementation must obtain
the DTB address using the documented bare-metal
location and validate the DTB header before parsing it.

The DTB is the source of truth for device addresses,
interrupt lines, and other platform configuration.
Only the documented RAM and flash base addresses
should be treated as fixed for this board.
Other device locations must be discovered from
the DTB rather than copied as unexplained constants.

## 6. Boot Sequence

The proposed boot sequence is:

1. QEMU initializes the virtual machine.
2. QEMU's Generic Loader loads the ARM64 ELF image.
3. QEMU sets CPU 0's PC to the ELF entry point.
4. The entry code inspects the initial CPU state.
5. The entry code establishes the required
   execution environment and initial stack.
6. The entry code preserves or obtains the DTB address.
7. The kernel validates the DTB before using it.
8. The kernel enters the C initialization stage.
9. ARM64 architecture initialization is performed.
10. Early serial output reports boot progress.
11. The kernel reports its current initialization status.

Memory management, interrupt handling,
and scheduler activation will be integrated
in later milestones.

## 7. Proposed Source Layout

The proposed ARM64 boot-related structure is:

kernel/
  arch/
    arm64/
      arch.c
      arch.h
      exception.c
      exception.h
      registers.c
      registers.h
      boot/
        entry.S
        linker.ld

The existing ARM64 architecture files
must remain intact unless a specific change
is reviewed and approved.

## 8. Kernel Entry Point

The ARM64 entry point will be implemented
in `entry.S`.

Responsibilities:

- Provide the initial execution symbol.
- Inspect and document the initial exception level.
- Establish a valid stack without overwriting the DTB.
- Preserve the DTB address for the C initialization stage.
- Transfer control to the C kernel entry function.

The entry code must not assume that
the stack or runtime environment is already
configured by the host build system.

The implementation must not assume that
QEMU's Generic Loader initializes CPU system
registers or performs the kernel's architecture setup.

## 9. Linker Script

The linker script will define:

- Kernel entry symbol.
- Kernel load address.
- Text section.
- Read-only data section.
- Initialized data section.
- Zero-initialized data section.
- Stack and alignment requirements.
- Symbols marking the kernel image boundaries.

Memory addresses must be validated
against the selected QEMU platform.

The linker layout must avoid the DTB region
and any memory reserved by the boot environment.

No arbitrary memory layout will be treated
as a verified hardware specification.

## 10. Cross-Compilation

The ARM64 target will use a dedicated
bare-metal cross-compilation toolchain.

The toolchain configuration must define:

- Target architecture.
- Compiler.
- Assembler.
- Linker.
- Target-specific compiler flags.
- Target-specific linker flags.

The ARM64 build must remain separate
from the existing host CMake presets.

The exact toolchain and ABI flags must be selected
and tested before implementation. The build must
not mix host objects with ARM64 kernel objects.

## 11. Early Debug Output

V1 will provide basic serial output
for boot diagnostics.

The initial implementation will target
the QEMU `virt` machine's PL011 UART.

The UART node and its register address must
be discovered from the generated DTB.
The implementation must not rely on a hard-coded
UART address copied from a different QEMU version
or machine configuration.

The serial driver must be isolated from
the generic kernel logging interface.

A hardware-independent logging redesign
is outside the scope of this milestone.

## 12. Build Separation

The project must preserve two independent paths:

### Host Build

- Existing host-debug preset.
- Existing host-release preset.
- Existing unit tests.
- Existing CI behavior.

### ARM64 Build

- ARM64 cross-compilation.
- ARM64 assembly.
- ARM64 linker script.
- ARM64 kernel ELF image.
- QEMU boot validation.

ARM64-specific assembly must not be included
in the host static library.

## 13. Testing Strategy

The implementation will be validated in stages:

1. Verify cross-compiler availability.
2. Verify that the installed QEMU build supports
   the selected `virt` machine and CPU model.
3. Compile the ARM64 entry assembly.
4. Link the ARM64 ELF image.
5. Inspect the ELF architecture, load segments,
   and entry symbol.
6. Check that ELF load segments do not overwrite
   the DTB region.
7. Load the image in QEMU using the Generic Loader.
8. Verify execution reaches the kernel entry point.
9. Validate the DTB header and discover the UART.
10. Verify early serial output.
11. Verify the existing host build and tests remain valid.

A successful host build does not prove
that the ARM64 kernel can boot.

A successful ELF link does not prove
that the kernel can execute.

Boot success must be verified through QEMU,
including observable output from the kernel.

## 14. Compatibility Requirements

The implementation must:

- Preserve existing host functionality.
- Preserve existing unit tests.
- Avoid unnecessary changes to existing modules.
- Keep ARM64-specific code isolated.
- Avoid claiming hardware support before verification.
- Keep the DTB and boot memory reservations intact.
- Avoid assuming device addresses not guaranteed
  by the selected QEMU machine documentation.

## 15. Out of Scope for V1

The following features are not included:

- Physical smartphone boot support.
- Real hardware device drivers.
- GIC interrupt controller initialization.
- Hardware timer initialization.
- Context switching.
- Preemptive scheduling.
- User-space process execution.
- Full memory management activation.

These features will be developed
in separate milestones.

## 16. Completion Criteria

ARM64 Boot Integration V1 is complete when:

- The ARM64 kernel ELF image builds successfully.
- The ELF entry point is verified.
- QEMU loads and executes the kernel.
- The initial CPU state is handled as documented.
- The DTB is preserved and validated.
- The UART is discovered from the DTB.
- Early serial output is visible.
- Existing host builds remain functional.
- Existing host unit tests pass.
- CI reports the relevant build and test results.

## 17. Current Status

Design proposal only.

The QEMU Generic Loader and `virt` platform
behavior described above are based on QEMU's
published documentation.

The example command and SyntraOS-specific
memory layout have not yet been tested.

No boot assembly, linker script,
cross-compilation target, or QEMU boot
implementation is claimed to exist yet.

## 18. References

- QEMU Generic Loader documentation:
  https://qemu.readthedocs.io/en/master/system/generic-loader.html
- QEMU ARM `virt` machine documentation:
  https://qemu-project.gitlab.io/qemu/system/arm/virt.html

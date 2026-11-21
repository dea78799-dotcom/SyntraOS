# SyntraOS VirtIO-MMIO Discovery, Mapping, and Input Preparation v6

Status: FDT transport discovery, GIC interrupt metadata parsing, checked Device mapping, bounded identity reads, initial input-driver negotiation
Target: QEMU ARM64 `virt`

## Implemented

- Locate the first Device Tree node compatible with `virtio,mmio` and validate its first `reg` tuple.
- Resolve the node's `interrupt-parent` phandle to its interrupt-controller node.
- Validate the controller's `#interrupt-cells` property and parse the first GIC interrupt specifier.
- Convert GIC SPI specifiers to INTID 32 + SPI number and PPI specifiers to INTID 16 + PPI number; validate supported trigger flags.
- Preserve the interrupt INTID and trigger flags as metadata. No VirtIO interrupt is registered or enabled yet.
- Map the discovered transport as Device/RW/XN using the ARM64 MMU plan and verify it before and after MMU activation.
- Read bounded VirtIO identity registers only after the post-activation MMU audit.
- Classify DeviceID 18 as VirtIO Input.
- For a confirmed Input device, reset the transport, advance status through ACKNOWLEDGE and DRIVER, and negotiate an empty feature set.
- Handle the legacy 32-bit feature word and modern transport's upper feature word separately. For transport v2, set and verify FEATURES_OK.
- Keep the transport short of DRIVER_OK. No queue is published and no interrupt is enabled.

## Runtime boundary

The input device's interrupt specifier is parsed and validated, but the IRQ is not yet routed into the GIC/interrupt framework. No queue or event buffers exist, no input event is delivered, and no key/pointer input reaches the GUI.

## Validation

Host tests cover FDT phandle lookup and controller-property retrieval, alongside VirtIO metadata, bounded MMIO reads, identity decoding, and version-aware feature negotiation. The next full ARM64/QEMU CI run must verify the QEMU FDT interrupt metadata and all prior boot checks.

## Next stages

1. Confirm the QEMU VirtIO Input SPI INTID and trigger flags from the boot log.
2. Add safe GIC SPI trigger configuration and register the existing generic interrupt handler.
3. Add bounded contiguous DMA/event-queue memory allocation.
4. Configure the event queue and only then transition to DRIVER_OK.
5. Consume VirtIO input events and translate them to `syntra_input_event_t`.
6. Route keyboard/pointer events into the desktop/window interaction loop and test actual key delivery in QEMU.

All implementation is original SyntraOS code using the project's FDT, MMU, allocator, and interrupt components plus public VirtIO transport definitions. No proprietary vendor GUI source or third-party GUI toolkit is imported.

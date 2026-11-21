# SyntraOS VirtIO-MMIO Discovery, Mapping, and Input Preparation v7

Status: FDT discovery, GIC interrupt metadata, Device mapping, identity reads, initial negotiation, SPI trigger configuration
Target: QEMU ARM64 `virt`

## Implemented

- Discover the first VirtIO-MMIO Device Tree node and validate its first `reg` tuple.
- Resolve inherited `interrupt-parent` phandles, validate the controller's `#interrupt-cells`, and parse the first GIC SPI/PPI specifier and trigger flags.
- Confirm the QEMU Input transport uses GIC SPI INTID 48 with edge-rising flags 1.
- Map the VirtIO transport as Device/RW/XN and verify the page tables before and after MMU activation.
- Read the four identity registers only after the MMU audit; identify DeviceID 18 as VirtIO Input.
- Reset the Input transport, advance ACKNOWLEDGE/DRIVER, and negotiate no optional features.
- Configure the identified GIC SPI's ICFGR trigger bits: edge-rising for DT flags 1, level-high for flags 4. Unsupported polarity/trigger flags are rejected.
- Keep the interrupt disabled and the VirtIO device short of DRIVER_OK until the event queue is ready.

## Runtime boundary

The IRQ number and trigger mode are discovered and the GIC configuration is set, but the VirtIO interrupt is not registered or enabled. No event queue or buffers exist, no DRIVER_OK transition occurs, and no keyboard/pointer event reaches the GUI.

## Validation

CI #620 passed Host Debug/Release and ARM64/QEMU, including FDT phandle/inherited-property tests and QEMU confirmation of SPI INTID 48 / edge-rising metadata. The GIC trigger-register write and new required QEMU marker are a newer stage and require a fresh complete CI run.

## Next stages

1. Add a bounded contiguous DMA/event-queue allocation path.
2. Configure the event queue and publish its addresses.
3. Register the existing interrupt handler, enable the SPI only after the queue is ready, then set DRIVER_OK.
4. Consume used-ring entries and translate VirtIO input events to `syntra_input_event_t`.
5. Route keyboard/pointer events into the desktop/window interaction loop and test actual key delivery in QEMU.

All implementation is original SyntraOS code using project FDT, MMU, allocator, and interrupt components plus public VirtIO transport definitions. No proprietary vendor GUI source or third-party GUI toolkit is imported.

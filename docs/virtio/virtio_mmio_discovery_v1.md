# SyntraOS VirtIO-MMIO Discovery, Mapping, and Input Preparation v5

Status: FDT discovery, checked Device-memory mapping, bounded identity reads, initial input-driver negotiation
Target: QEMU ARM64 `virt`

## Implemented

- Locate the first Device Tree node compatible with `virtio,mmio` and validate its first `reg` tuple.
- Map the discovered region as Device/RW/XN using the ARM64 MMU mapping plan and 4-KiB page alignment.
- Verify mappings with the page-table walker before and after MMU activation.
- Read MagicValue, Version, DeviceID, and VendorID only after the post-activation MMU audit.
- Classify DeviceID 18 as VirtIO Input.
- For a confirmed Input device, reset the transport and advance the device status through ACKNOWLEDGE and DRIVER.
- Explicitly negotiate an empty feature set: SyntraOS claims no optional device feature until its implementation exists.
- Handle the 32-bit legacy feature word for transport version 1; select and clear the upper feature word only for transport version 2.
- For transport version 2, set and verify FEATURES_OK.
- Keep the transport short of DRIVER_OK. No queue is published and no interrupt is enabled.

## Safety and runtime boundary

The preparation function requires a previously decoded Input identity, a mapped pointer, and a region large enough for all accessed registers. Every access is aligned and bounds-checked. It performs a reset and limited status/feature-register writes; it does not read or write beyond the documented MMIO transport register window.

Not implemented yet:
- input device configuration-space queries;
- event virtqueue allocation or publication;
- interrupt discovery/routing for the input device;
- DRIVER_OK transition;
- event-buffer consumption or keyboard/pointer event delivery to the GUI.

The QEMU test attaches a virtual keyboard to the first VirtIO-MMIO bus and requires both the Input identity and successful initial driver-feature negotiation markers. This verifies transport setup only, not delivery of key events.

## Validation

Host tests cover metadata validation, bounded mapped reads, identity decoding, version 1 and version 2 status/feature negotiation, invalid identity, and short mappings. A fresh complete ARM64/QEMU CI run is required before this stage is considered validated.

## Next stages

1. Parse and validate the input device's interrupt specifier from FDT.
2. Confirm the QEMU GIC SPI number and wire the existing interrupt framework to the VirtIO interrupt.
3. Add a bounded contiguous DMA/event-queue memory allocation path.
4. Configure the event queue and only then transition to DRIVER_OK.
5. Consume VirtIO input events and translate them to `syntra_input_event_t`.
6. Route keyboard/pointer events into the desktop/window interaction loop and test actual key delivery in QEMU.

All implementation is original SyntraOS code using the project's FDT, MMU, allocator, and interrupt components plus public VirtIO transport definitions. No proprietary vendor GUI source or third-party GUI toolkit is imported.

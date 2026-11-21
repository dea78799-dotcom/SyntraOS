# SyntraOS VirtIO-MMIO Discovery and MMU Mapping v3

Status: Device Tree discovery, copied identity-snapshot decoding, and checked Device-memory mapping
Target: QEMU ARM64 `virt`

## Implemented

- Locate the first Device Tree node compatible with `virtio,mmio`.
- Read its first `reg` tuple through the existing validated FDT parser.
- Validate the resulting base address and region size.
- Define the public VirtIO-MMIO identification register offsets and decode copied register snapshots.
- Validate transport versions 1 and 2 and classify device ID 18 as VirtIO Input.
- Extend the ARM64 MMU mapping plan with an optional VirtIO region.
- Round the discovered region outward to 4-KiB page boundaries and plan it as Device memory, privileged read/write, execute-never.
- Reject zero/undersized/overflowing ranges and overlap with any existing planned mapping.
- Pass the discovered FDT region from boot discovery into the MMU plan.
- Verify every planned mapping using the existing page-table software walker before and after MMU activation.

## Explicit safety boundary

The mapping plan only adds a mapping to the checked page-table build. It does not access the VirtIO registers.

The identity decoder still accepts copied register values only. It never dereferences a physical address. This stage does not:
- read identification registers from hardware;
- reset or configure a device;
- negotiate VirtIO feature bits;
- allocate or publish a virtqueue;
- enable or acknowledge a VirtIO interrupt;
- consume keyboard or pointer events.

A future hardware reader must be added only after the MMU activation audit has passed, and must use the validated mapped device region with bounded, aligned 32-bit reads.

## Validation

Host tests cover the optional VirtIO mapping, Device/RW/XN attributes, 4-KiB rounding, invalid size, missing size, address overflow, and overlap with an existing mapping. ARM64 QEMU boot validation is required for this revision before calling the integration complete.

## Next stages

1. Confirm the final ARM64 QEMU boot and post-MMU mapping verification with the VirtIO mapping present.
2. Add a bounded read-only identification-register accessor and verify magic, version, device ID, and vendor ID on QEMU.
3. Negotiate the VirtIO Input device and configure a bounded event queue.
4. Decode input events into `syntra_input_event_t`.
5. Route events to the desktop/window interaction loop and test using QEMU's virtual input devices.

The implementation is original SyntraOS C code. It uses the project's own FDT, MMU, and page-table components plus public VirtIO transport definitions; no vendor GUI source or third-party GUI toolkit is imported.

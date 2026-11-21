# SyntraOS VirtIO-MMIO Discovery, Mapping, and Read-Only Probe v4

Status: FDT discovery, checked Device-memory mapping, bounded identity-register reads
Target: QEMU ARM64 `virt`

## Implemented

- Locate the first Device Tree node compatible with `virtio,mmio`.
- Read its first `reg` tuple through the existing validated FDT parser.
- Validate the resulting base address and region size.
- Extend the ARM64 MMU mapping plan with an optional VirtIO region.
- Round the discovered region outward to 4-KiB page boundaries and map it as Device memory, privileged read/write, execute-never.
- Reject zero, undersized, overflowing, or overlapping ranges.
- Verify every planned mapping using the existing page-table software walker before and after MMU activation.
- Only after the MMU audit succeeds, read the four identification registers (MagicValue, Version, DeviceID, VendorID) through the verified identity mapping.
- Bound every read to the discovered region size, require aligned base addresses, and copy the values into a snapshot before decoding.
- Decode supported transport versions 1 and 2 and classify device ID 18 as VirtIO Input.

## Read-only boundary

The boot probe reads only the four identification registers. It does not write any device register or change device state.

It does not:
- reset or configure a device;
- negotiate feature bits;
- allocate or publish a virtqueue;
- enable, acknowledge, or handle a VirtIO interrupt;
- consume keyboard or pointer events.

The reader is called only after the boot MMU mapping audit has verified the discovered region as Device/RW/XN. It receives an explicit mapped pointer and size, checks alignment and bounds, and uses volatile 32-bit reads. On AArch64 it executes a read barrier before returning the snapshot.

If the optional device is absent, reports an invalid magic value, or uses an unsupported transport version, the probe logs the condition and preserves the normal boot path.

## Validation

Host tests cover metadata validation, mapped identity reads, register decoding, null pointers, unaligned pointers, insufficient/oversized bounds, and supported/unsupported identity values. ARM64 QEMU CI must still verify the live read-only probe and framebuffer boot path before this stage is marked complete.

## Next stages

1. Verify the corrected QEMU boot validator and the live identification-register log in a fresh ARM64 CI run.
2. If QEMU exposes a real VirtIO Input device, confirm its DeviceID and vendor ID.
3. Implement feature negotiation and a bounded event virtqueue only after a real input device is confirmed.
4. Decode input events into `syntra_input_event_t`.
5. Route events to the desktop/window interaction loop and test with QEMU virtual input devices.

The implementation is original SyntraOS C code. It uses the project's own FDT, MMU, and page-table components plus public VirtIO transport definitions; no vendor GUI source or third-party GUI toolkit is imported.

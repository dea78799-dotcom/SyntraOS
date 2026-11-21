# SyntraOS VirtIO-MMIO Discovery, Mapping, and Input Queue Preparation v8

Status: FDT discovery, MMU mapping, identity and feature negotiation, GIC trigger setup, legacy event queue construction/publication
Target: QEMU ARM64 `virt`

## Implemented

- Discover the first VirtIO-MMIO node, validate its `reg` tuple, resolve inherited interrupt-parent phandles, and parse the GIC interrupt specifier.
- Confirm QEMU's Input device uses SPI INTID 48 with edge-rising trigger flags 1.
- Map the transport as Device/RW/XN and verify the mapping before and after MMU activation.
- Read bounded identity registers and identify DeviceID 18 as VirtIO Input.
- Reset the Input transport, advance ACKNOWLEDGE/DRIVER, and negotiate an empty feature set.
- Configure the GIC SPI trigger bits for edge-rising or level-high; reject unsupported polarity/trigger flags.
- Add contiguous physical page allocation for a requested run of complete 4-KiB RAM pages.
- Allocate two contiguous pages for the legacy VirtIO split event queue.
- Build eight writable event descriptors, the available ring, the used ring, and eight 8-byte event buffers.
- Validate QueueNumMax, configure QueueNum and QueueAlign, and publish QueuePFN last after a memory write barrier.
- Keep the device short of DRIVER_OK. The queue is configured but not started; the interrupt remains disabled.

## Runtime boundary

The queue layout is prepared and published, but the driver has not set DRIVER_OK, enabled the SPI, consumed the used ring, or delivered key/pointer events. This stage does not claim live input support.

## Validation

Host tests cover contiguous allocation, queue bounds, descriptor layout, available/used ring initialization, QueuePFN publication, and refusal to configure an undersized queue or a device already in DRIVER_OK. A fresh complete ARM64/QEMU CI run must verify the queue setup marker and all previous checks.

## Next stages

1. Register a bounded VirtIO Input IRQ handler using the parsed SPI INTID.
2. Enable the IRQ only after the queue is fully prepared.
3. Set DRIVER_OK and notify queue 0.
4. Consume used-ring entries, validate descriptor IDs/lengths, and recycle event buffers.
5. Convert VirtIO input wire events into `syntra_input_event_t`.
6. Route keyboard/pointer events into the desktop/window interaction loop and test actual key delivery in QEMU.

All implementation is original SyntraOS code using project FDT, MMU, page allocator, and interrupt components plus public VirtIO transport definitions. No proprietary vendor GUI source or third-party GUI toolkit is imported.

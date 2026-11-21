# SyntraOS VirtIO-MMIO Discovery, Mapping, and Input Queue Preparation v8

Status: FDT discovery, MMU mapping, identity and feature negotiation, GIC trigger setup, event queue publication, IRQ registration and queue start
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
- Initialize normalized input state at 640x480.
- Register the Input SPI handler in the generic interrupt framework, assign priority, and enable the SPI.
- Set DRIVER_OK and notify queue 0, starting the device queue after the handler is registered.

## Runtime boundary

The queue layout is prepared and published, but the driver has not set DRIVER_OK, enabled the SPI, consumed the used ring, or delivered key/pointer events. This stage does not claim live input support.

## Validation

Host tests cover contiguous allocation, queue bounds, descriptor layout, available/used ring initialization, QueuePFN publication, and refusal to configure an undersized queue or a device already in DRIVER_OK. CI requires markers for queue setup, IRQ registration, queue start, and all previous boot/MMU/framebuffer checks. Host tests simulate a keyboard press/release through the queue handler.

## Next stages

1. Add QEMU test that injects a keyboard event and verifies IRQ handler receives and updates key state.
2. Route keyboard/pointer state into the desktop/window interaction loop.
3. Add pointer absolute-axis range discovery and test touch/mouse events.

All implementation is original SyntraOS code using project FDT, MMU, page allocator, and interrupt components plus public VirtIO transport definitions. No proprietary vendor GUI source or third-party GUI toolkit is imported.

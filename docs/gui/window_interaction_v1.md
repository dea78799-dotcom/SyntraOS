# GUI v3 — Window interaction layer

This milestone adds pointer-driven interaction on top of the window manager.

## Implemented

- Detects a new left-button press using the current and previous button state.
- Hit-tests windows from front to back.
- Raises the selected window in z-order.
- Starts a drag when the pointer presses the title bar.
- Preserves the pointer-to-window offset while dragging.
- Clamps dragged windows to the configured virtual screen bounds.
- Closes a window when the close-control rectangle is clicked.
- Clears drag state when the left button is released.
- Rejects mismatched input and manager dimensions.
- Includes host tests for focus, drag, screen-edge clamping, close, and invalid dimensions.

## Input contract

The interaction layer consumes a normalized `syntra_input_state_t`. A device driver or test harness must first feed events through the existing input event core. This layer does not read VirtIO queues, poll QEMU devices, or connect directly to AVNC touch events.

Call `syntra_gui_window_interaction_process()` once after updating the normalized input state for a frame. The caller is responsible for rendering the desktop and windows after any state change.

## Current limitations

- Only the left pointer button is used.
- Window resizing is not implemented.
- There is no keyboard shortcut routing yet.
- No application content or application lifecycle is attached to windows.
- Live QEMU input remains a separate driver/integration task.

## Source and licensing

The interaction logic is original SyntraOS C code using the project's own window manager and normalized input API. It does not import a third-party GUI framework or vendor desktop source.


## GUI v5 persistent runtime and IRQ event FIFO

The VirtIO input IRQ handler copies accepted normalized events into a bounded 64-event single-producer/single-consumer FIFO. The consumer uses `syntra_virtio_mmio_pop_input_event()` in normal context; `available` distinguishes an empty queue from errors. Queue overflow drops newest events and increments `software_event_dropped`. The IRQ still updates the shared input snapshot for diagnostics, but the ordered FIFO preserves transient press/release events when main-context polling is delayed.

`gui_runtime` owns input state, the persistent v5 window manager, and pointer-interaction state. It processes one normalized event at a time outside IRQ context and renders desktop plus persistent windows. A host integration test exercises drag, close, and rendering. This runtime is not yet called by the ARM64 boot/main loop; live IRQ-to-runtime-to-framebuffer wiring and QEMU validation remain pending.


## ARM64 ramfb GUI runtime input pump

The ramfb initializer now creates a persistent GUI runtime (desktop, app windows,
input state, and drag state) and renders it to the framebuffer. The
`syntra_ramfb_process_input_events()` pump drains the VirtIO software FIFO
outside IRQ context, processes the ordered events, and redraws once per drained
batch. It deliberately does not run in the IRQ handler.

**Integration status:** the pump is implemented but not yet connected to the
kernel's long-running idle/main loop. Boot currently returns after its boot
sequence, so this commit does not claim live interactive GUI input on QEMU.

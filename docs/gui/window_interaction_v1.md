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

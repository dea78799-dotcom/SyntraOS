# GUI v3 — Window manager foundation

This milestone introduces a transport-independent, fixed-capacity window manager.

## Implemented

- A bounded table of up to eight windows.
- Stable non-zero window IDs.
- Open and close operations.
- Move operations that reject rectangles outside the virtual screen.
- Focus changes that move the selected window to the front of the z-order.
- Read-only lookup and active-window accessors.
- Host unit tests for ordering, geometry validation, lifecycle, and capacity.
- The source is included in both the host GUI library and the ARM64 kernel build.

## Deliberate scope limits

This is the state-management foundation only. It does not draw window frames, provide title bars or buttons, launch applications, or connect to a live QEMU input device. The existing desktop renderer remains unchanged. Window rendering and app surfaces will be separate follow-up work.

## Geometry and ordering

Window rectangles use framebuffer pixel coordinates. Width and height must be non-zero and the full rectangle must fit within the configured virtual screen. The manager stores windows from back to front; the last entry is the active/frontmost window. Focusing an existing window raises it without changing its ID or geometry.

## Resource policy

The manager uses a fixed array of eight entries and performs no dynamic allocation. Opening a ninth window returns SYNTRA_STATUS_OUT_OF_MEMORY. This makes the initial kernel-side behavior deterministic and avoids introducing an allocator dependency into the GUI foundation.

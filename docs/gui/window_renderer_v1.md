# GUI v3 — Window frame renderer

This milestone adds the first visual layer for the GUI v3 window manager.

## Implemented

- Draws each managed window onto an existing software framebuffer.
- Uses the manager's back-to-front order for deterministic overlap.
- Draws a frame border, dark content area, and title bar.
- Gives the frontmost window a distinct active title-bar color.
- Draws a visible close-control motif in the title bar.
- Validates framebuffer dimensions against the manager's virtual screen.
- Rejects malformed window records before drawing them.
- Includes host tests for z-order, active/inactive title bars, close-control appearance, untouched pixels, stride padding, and invalid arguments.

## Deliberate scope limits

The close control is visual only. It does not close a window when clicked. The renderer does not yet process pointer events, keyboard events, dragging, resizing, or application content. Those require a separate input-routing and window interaction layer.

The wallpaper supplied for SyntraOS is not embedded or modified by this renderer. It remains a planned desktop resource for a later integration step.

## Rendering contract

Call `syntra_gui_render_windows()` after rendering the desktop background. It draws the windows from back to front and leaves pixels outside their rectangles untouched. The framebuffer dimensions must match the dimensions used to initialize the window manager.

## Source and dependencies

The renderer is original SyntraOS C code and uses only the project's own framebuffer and window-manager interfaces. No third-party GUI toolkit or proprietary vendor source is included.

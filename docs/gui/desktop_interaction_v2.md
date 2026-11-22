# Desktop Interaction v2

Status: Specification; implementation not yet complete
Target: SyntraOS GUI runtime on ARM64 QEMU

## Goals

Extend the existing press/release and title-bar drag behavior into a consistent
desktop interaction model. Input events are normalized before reaching this
layer. No GUI rendering or application work may run inside an IRQ handler.

## Pointer and button state

The input layer already tracks left, right, and middle button state, pointer
coordinates, relative wheel movement, absolute axes, key state, and SYN frame
boundaries. The GUI runtime currently reacts to the left-button press edge for
launcher activation and app selection. Window interaction currently focuses,
closes, and starts a title-bar drag on left-button press, then moves the window
while the button remains down.

The next implementation must preserve these edge semantics and must not treat
every repeated held-button event as a fresh click.

## Required gesture model

### Press, hold, and release

- Press begins a pointer gesture and records target, origin, and monotonic start
  time.
- Hold is a state, not a new click. It remains active while the same pointer
  button/contact stays down.
- Long-press fires once after a configurable threshold (initial default:
  500 ms), provided movement remains below a small slop radius (initial default:
  8 logical pixels).
- Release ends the gesture and reports whether it was a tap, drag, or completed
  long-press. A long-press must not also trigger the ordinary click action on
  release.
- Pointer cancellation, device reset, focus loss, or malformed input clears
  the active gesture safely.

### Click and double-click

- A click is recognized on release only when the press target remains valid,
  movement is within the tap slop, and long-press did not fire.
- Double-click requires two valid clicks on the same target within a
  configurable interval (initial default: 400 ms).
- Double-click opens a Files folder or launches an app only when that target
  explicitly supports activation. It must not duplicate destructive actions.

### Drag and drop

- Window dragging starts only from a draggable title-bar region, not from
  close controls or app content.
- Dragging preserves the pointer-to-window offset and clamps the window to the
  screen bounds.
- Drag release ends the operation and clears capture state.
- Future icon drag/drop must use a separate draggable-object policy so window
  movement and file movement cannot conflict.

### Context menu

- Right-click and long-press request a context-menu event for the hit-tested
  target. The GUI layer owns menu presentation; applications own the available
  actions.
- If no actions are available, no empty menu should be shown.
- Menu selection and dismissal must be keyboard- and pointer-accessible.

### Scroll and hover/focus

- Wheel events scroll the content under the pointer, with bounded scroll
  offsets.
- Pointer movement updates hover target; focus remains distinct from hover.
- Keyboard focus must have a visible focus indicator and remain usable without
  a pointer.

### Touch and multi-touch

- Absolute pointer axes are supported by the input core when ranges are
  configured. This is not proof of a working touchscreen driver.
- A single touch contact should map to pointer down/move/up.
- Multi-touch requires contact IDs and per-contact state. The current normalized
  event structure has no contact ID, so pinch/rotate gestures are explicitly
  deferred until that representation is added and tested.

## Timing contract

Long-press and double-click require a monotonic clock. The current normalized
input event contains type, code, and value only; it carries no timestamp.
The implementation must add an explicit monotonic timestamp or pass a
monotonic time value from the platform timer into the GUI runtime. It must not
approximate elapsed time using event count, SYN_REPORT count, or CPU loop
iterations.

## Implementation sequence

1. Add monotonic-time plumbing and gesture state with overflow-safe elapsed-time
   calculations.
2. Add unit tests for press/hold/release, long-press exactly once, movement
   cancellation, tap slop, double-click timing, and device cancellation.
3. Add pointer capture and drag lifecycle tests for windows.
4. Add context-menu event routing and a minimal desktop menu renderer.
5. Add bounded scroll state and hover/focus feedback.
6. Add VirtIO pointer/tablet discovery separately from keyboard discovery.
7. Add QEMU integration tests that inject pointer move/down/up and verify
   window state and framebuffer changes.
8. Add touch contact IDs only with a driver/transport test that supplies them.

## Acceptance criteria

A feature is not complete merely because it compiles or a screenshot looks
correct. Each interaction must have deterministic unit tests. Device-facing
features additionally require QEMU integration evidence. The Termux screenshot
workflow remains a headless visual check and does not count as interactive
input validation.

## Current status

- Implemented: normalized left/right/middle button state, relative/absolute
  pointer coordinates, wheel deltas, keyboard state, left-button edge
  detection, title-bar window drag, window focus and close on click.
- Not yet implemented/verified: time-based long-press, double-click, context
  menu, scrollable app content, hover feedback, pointer/tablet QEMU integration,
  touch contact tracking, pinch/rotate.

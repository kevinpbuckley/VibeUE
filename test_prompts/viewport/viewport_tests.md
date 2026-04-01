# Viewport Tests

Test viewport camera controls, view types, exposure, layout switching, and all perspective menu options. Run sequentially.

---

## Viewport Info

Show me the current viewport state — view type, camera position, FOV, exposure, layout, everything.

---

## View Type Switching — Perspective

Make sure we're in perspective view.

---

Confirm the viewport type is "perspective".

---

## View Type Switching — Orthographic

Switch to top-down view.

---

What's the viewport type now? Should be "top".

---

Switch to bottom view.

---

Confirm we're in bottom view.

---

Switch to front view.

---

Confirm front.

---

Switch to back view.

---

Confirm back.

---

Switch to the left orthographic view.

---

Confirm left.

---

Switch to right.

---

Confirm right.

---

Go back to perspective view.

---

Confirm perspective.

---

## Field of View

What's the current FOV?

---

Set the field of view to 60 degrees.

---

Confirm the FOV is 60.

---

Set it to 120.

---

Confirm 120.

---

Reset it back to 90.

---

Confirm 90.

---

## Near Clip Plane

Set the near clipping plane to 1.0.

---

Get the viewport info and check what the near clip is.

---

Reset the near clip to default (-1).

---

## Far Clip Plane

Set the far clipping plane to 25000.

---

Confirm the far clip plane is 25000.

---

Set the far clip to 0 (infinity / default).

---

Confirm it's back to 0.

---

## Exposure — Fixed EV100

Set the exposure to fixed mode with EV100 = 3.0.

---

Get the viewport info. Exposure should be fixed=true, EV100=3.0.

---

Change the EV100 to 0.5 (still fixed).

---

Confirm EV100 is 0.5.

---

## Exposure — Game Settings (Auto)

Switch exposure back to game settings / auto.

---

Confirm exposure is no longer fixed.

---

## Game View

Enable Game View.

---

Get the viewport info. Game view should be true.

---

Disable Game View.

---

Confirm game view is off.

---

## Cinematic Control

Disable cinematic control on the viewport.

---

Confirm cinematic control is disabled.

---

Re-enable cinematic control.

---

Confirm it's enabled again.

---

## Realtime Rendering

What's the current realtime rendering state?

---

Disable realtime rendering.

---

Confirm realtime is off.

---

Turn realtime back on.

---

Confirm realtime is on.

---

## Camera Position

Move the viewport camera to position 1000, 2000, 500.

---

Get the viewport info. Camera location should be approximately (1000, 2000, 500).

---

## Camera Rotation

Set the camera rotation to pitch -30, yaw 45, roll 0.

---

Get the viewport info. Camera rotation should be approximately (-30, 45, 0).

---

## Camera Speed

Set the camera movement speed to 1 (slowest).

---

Get the viewport info. Camera speed should be 1.

---

Set camera speed to 8 (fastest).

---

Confirm speed is 8.

---

Set it back to 4 (default).

---

## Layout — Quad View

Switch the viewport to quad view (2x2).

---

What layout are we in? Should be FourPanes2x2.

---

## Layout — Single Pane

Switch back to single pane view.

---

Confirm we're in OnePane.

---

## Layout — Two Panes

Switch to two panes horizontal.

---

Confirm TwoPanesHoriz.

---

Switch back to single pane.

---

## Layout — Other Layouts

Switch to three panes left.

---

Confirm ThreePanesLeft.

---

Switch to four panes bottom.

---

Confirm FourPanesBottom.

---

Switch back to single pane (OnePane).

---

## Combined Workflow

Set up a top-down orthographic view with the following:
1. View type: top
2. Camera location: 0, 0, 5000
3. Realtime on
4. Game view off

---

Show me the full viewport state to verify all settings.

---

Now switch back to perspective, FOV 90, and single pane.

---

Confirm everything is back to defaults.

---

## Cleanup

Reset viewport to default state:
- Perspective view
- FOV 90
- Near clip default (-1)
- Far clip infinity (0)
- Exposure game settings (auto)
- Game view off
- Cinematic control on
- Realtime on
- Camera speed 4
- OnePane layout

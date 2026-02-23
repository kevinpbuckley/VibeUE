# Landscape Spline Tests

Isolated spline tests extracted from the full landscape service test suite.
Requires an empty open world level. Includes minimal setup to create a landscape with layers before testing splines.

---

## Setup — Create Landscape and Layers

Create a new landscape at the origin with default settings. Call it "TestTerrain".

---

Create a landscape material called M_TestTerrain in /Game/TestLandscape/Materials. Add a layer blend node with "Grass" (WeightBlend) and "Rock" (WeightBlend) layers. Connect the blend node to BaseColor. Compile and save.

---

Create weight-blended layer info objects for "Grass" and "Rock" in /Game/TestLandscape.

---

Assign M_TestTerrain to TestTerrain with the Grass and Rock layer info objects.

---

Confirm TestTerrain has Grass and Rock layers using `list_layers`.

---

## Splines — Initial State

How many spline points does TestTerrain currently have? Use `get_spline_info`. Report `num_control_points` and `num_segments`. Both should be 0.

---

## Splines — Create Points

Create a spline point on TestTerrain at world location (0, 0, 500) using `create_spline_point`. Width 400, side_falloff 300, end_falloff 300. Paint under it using the "Grass" layer. Report the returned point_index.

---

Create a second spline point at world location (5000, 0, 600). Same settings. Report the point_index.

---

Create a third spline point at world location (10000, 3000, 500). Report the point_index.

---

Use `get_spline_info` to confirm there are now 3 control points and 0 segments.

---

## Splines — Connect Points

Connect point 0 to point 1 using `connect_spline_points`. Use auto tangent (0). Paint under it with "Grass". Report success.

---

Connect point 1 to point 2. Report success.

---

Use `get_spline_info` again — should now have 3 points and 2 segments. Report the segment details (start index, end index, tangent lengths).

---

## Splines — Modify and Apply

Move point 1 to world location (5000, 500, 700) using `modify_spline_point`. Leave width unchanged (pass -1). Report success.

---

Apply the splines to deform terrain using `apply_splines_to_landscape`. Report success.

---

Check the height at world (5000, 0) — it should have been raised or lowered to approximate the spline height. Report the value.

---

## Splines — Delete and Rebuild

Delete spline point 0 using `delete_spline_point`. Report success.

---

Use `get_spline_info` — point 0 is gone, the segment connecting it should also be removed. How many points and segments remain?

---

Delete all remaining splines using `delete_all_splines`.

---

`get_spline_info` should now return 0 points and 0 segments. Confirm.

---

## Splines — create_spline_from_points

Create a 5-point road across the landscape using `create_spline_from_points`. Use these world locations:
- (-15000, -5000, 400)
- (-7500, 0, 500)
- (0, -2000, 600)
- (7500, 0, 500)
- (15000, 5000, 400)

Width 500, side_falloff 400, end_falloff 400. Paint layer "Grass". Raise terrain. Do NOT close the loop.

---

Report the result: how many points and segments were created?

---

Apply the splines to terrain using `apply_splines_to_landscape`.

---

Sample the height at (0, 0). Did the spline deform the terrain? Report the height.

---

Delete all splines to clean up.

---


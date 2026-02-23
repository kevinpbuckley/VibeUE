# Fix Broken Splines on Landscape4

Tests the fixed `connect_spline_points` (negative tangent preservation) and
`modify_spline_point` (rotation control) by diagnosing and recreating
Landscape4's broken splines from a reference landscape.

---

## Step 1 — Inventory All Landscapes

Use `get_all_level_actors` to list all actors of type `Landscape`. Report
every actor's name and world location.

---

## Step 2 — Inspect Landscape4's Current Splines

Use `get_spline_info` on Landscape4. Report:
- `num_control_points`
- `num_segments`
- For EVERY control point: `point_index`, `location`, `rotation`
- For EVERY segment: `segment_index`, `start_point_index`, `end_point_index`,
  `start_tangent_length`, `end_tangent_length`

Note any segments with **negative** tangent lengths — that is the key data we
need to preserve.

---

## Step 3 — Find the Reference Landscape

Look at the other landscapes in the level. Use `get_spline_info` on each one
(Landscape1, Landscape2, Landscape3 — whichever have splines). Identify which
landscape has the "correct" splines that Landscape4 should match.

Report its name and its full spline info (control points + segments with all
tangent values).

---

## Step 4 — Delete Landscape4's Broken Splines

Use `delete_all_splines` on Landscape4.

Confirm with `get_spline_info` — should return 0 points and 0 segments.

---

## Step 5 — Recreate Splines from Reference Data

Using the reference spline data collected in Step 3, recreate each control
point on Landscape4 using `create_spline_point`. Use matching world locations,
widths, side_falloff, and end_falloff from the reference.

After creating all points, call `get_spline_info` to confirm the correct number
of control points were created.

---

## Step 6 — Connect Points, Preserving Tangents

For EACH segment from the reference, call `connect_spline_points` with:
- `start_point_index` and `end_point_index` matching the reference
- `tangent_length` = the EXACT `start_tangent_length` from the reference
  segment (pass the raw value — **do not zero out negative values**)

This exercises the fixed negative-tangent path:
- Positive tangent → mesh flows start→end
- Negative tangent → mesh flows end→start (reversed)

Report each connection result.

---

## Step 7 — Apply Exact Rotations

For EACH control point from the reference where the rotation is non-zero, call:

```python
modify_spline_point(
    landscape_name="Landscape4",
    point_index=<index>,
    location=<original_location>,  # unchanged
    auto_calc_rotation=False,
    rotation=<rotation_from_reference>
)
```

This exercises the new `auto_calc_rotation=False` path added to
`modify_spline_point`.

Report each modify result.

---

## Step 8 — Verify Spline Fidelity

Use `get_spline_info` on Landscape4 one final time.

Compare EVERY field against the reference:
- Number of points → must match
- Number of segments → must match
- Each segment's `start_tangent_length` → must match the reference value
  (sign and magnitude), not auto-calculated
- Each point's `rotation` → must match the reference, not auto-calculated

Report PASS or FAIL for each field. If any segment tangent is positive and the
reference was negative (or vice versa), that is a **FAIL** — the fix did not
work.

---

## Step 9 — Apply Splines to Landscape

Call `apply_splines_to_landscape` on Landscape4 to deform terrain.

Report success.

---

## Step 10 — Summary

Report the overall test result:
- Were negative tangent values preserved end-to-end? (tangent fix test)
- Were explicit rotations applied correctly without AutoCalcRotation
  overwriting them? (rotation fix test)
- Did `apply_splines_to_landscape` succeed?

Verdict: PASS / FAIL with details.

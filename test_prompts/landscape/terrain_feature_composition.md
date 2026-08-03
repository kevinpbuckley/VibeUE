never run more than one tool at a time

# Terrain Feature Composition Tests (edit-layer write path)

Regression coverage for PR #522: `create_valley`/`create_mountain`/`create_plateau`/`create_crater`
were writing heights via a raw `FLandscapeEditDataInterface::SetHeightData` call with no edit-layer
scope, so a later layer-content resolve could silently discard the edit — and, because earlier calls
were written the same way, discard *their* edits too. The observable symptom was a second
`create_valley` call resetting the whole landscape back to flat.

The point of every check below is **composition and persistence**: an edit must still be there after
another edit elsewhere, after a forced layer resolve, and after a reload from disk.

Do all of this in a throwaway level — never on a project map that has a real landscape.

---

## Setup

Create a new empty level at /Game/Dev/TerrainCompositionTest and make it the current level. In it,
create a landscape labelled CompTest at the origin with scale 100,100,100 and 4x4 components of 63
quads. Confirm it has at least one edit layer.

---

Sample the height at a few widely separated world XY points and confirm the landscape starts flat
(height 0).

---

## Two valleys must compose, not overwrite

Carve a valley at world (5000, 5000) with radius 3000 and depth 2000, noise off.

---

Read the height back at (5000, 5000). It must be about -2000.

---

Now carve a second valley at (20000, 20000), same radius and depth, noise off.

---

Read the height at BOTH centers. The first valley must still be about -2000 — if it has returned to 0,
the edit-layer write path has regressed and this is the exact bug #522 fixed. The second must also be
about -2000.

---

## Overlapping valleys accumulate

Carve a third valley whose radius overlaps the first, centered a short distance away. Read the height
at the overlap point and confirm it is deeper than either valley alone would make it — overlapping
carves blend additively rather than replacing each other.

---

## The other radial features behave the same way

Repeat the "two calls, check the first survives" pattern for each of create_mountain, create_plateau
and create_crater, each at its own well-separated location. All four share one helper, so a regression
in that helper breaks all of them.

---

After all of those, re-read every earlier feature's center point. Every one must still hold its value.
Report the full list of positions and heights.

---

## Non-radial features must not wipe the radial ones

These take a different write path (create_ridge, apply_erosion, create_terraces,
blend_terrain_features, apply_noise, project_mesh_to_landscape). Create a ridge somewhere clear of the
existing features.

---

Re-read the centers of the valleys, mountain, plateau and crater. They must all be unchanged. Then read
the ridge's own midpoint and confirm it was raised.

---

## Forced layer resolve

Force a full layer update on the landscape (`force_layers_full_update`), then re-read every feature
center. Nothing may change — a full resolve rebuilds the merged heightmap from the real per-layer data,
so anything that disappears here never actually reached layer storage.

---

## Paint layers survive height edits

Add a paint layer to the landscape and paint it at full strength across a large rectangle. Confirm the
weight reads back as 1.0 at several points.

---

Now carve another valley and create another ridge. Re-read those same weight samples: they must still
be 1.0. A height write that triggers an unscoped full layer resolve can zero out weightmap data, so a
weight that has dropped to 0 is a real failure.

---

## Persistence across a reload

Save the level, then load a different level and load this one back again. Re-read every feature center
and every weight sample. All values must match what they were before the save — in-memory correctness
is not enough, the edits have to be in the saved layer data.

---

## Cleanup

Delete the /Game/Dev/TerrainCompositionTest level and any layer info asset created for it, and switch
back to whatever level was open before.

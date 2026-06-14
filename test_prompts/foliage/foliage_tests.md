# Foliage Service Tests

Tests for foliage placement, removal, and management. Requires a landscape in the level (create one first if needed). Run sequentially.

---

## Prerequisites

Create a landscape at the origin called "FoliageTestTerrain" with default settings if one doesn't exist.

---

## Empty State Checks

List all foliage types in the level. Should be empty (or note what's already there).

---

Check instance count for "/Engine/BasicShapes/Cube" — should return -1 (not in level).

---

Does any foliage exist for "/Engine/BasicShapes/Cube"? Should be false.

---

## Basic Scatter

Scatter 50 cubes ("/Engine/BasicShapes/Cube") in a 3000-unit radius around (0, 0) on FoliageTestTerrain with seed 42.

---

How many instances were added? How many rejected?

---

List foliage types now. Should show the cube with ~50 instances.

---

Get instance count for "/Engine/BasicShapes/Cube". Should be ~50.

---

## Scatter Reproducibility

Scatter 50 more cubes with the SAME seed (42) at the same location. The instance count should double but the pattern should be consistent.

---

## Scatter Rectangle

Scatter 30 spheres ("/Engine/BasicShapes/Sphere") in a rectangle from (-5000, -5000) to (5000, 5000) with seed 7.

---

List foliage types. Should now show both cubes and spheres.

---

## Add Specific Instances

Place 5 cylinder instances ("/Engine/BasicShapes/Cylinder") at these exact locations:
- (0, 0, 0)
- (1000, 0, 0)
- (0, 1000, 0)
- (-1000, 0, 0)
- (0, -1000, 0)

---

Query cylinders in a 1500-unit radius around (0, 0). Should find all 5.

---

## Query Foliage

Get cubes in a 1000-unit radius around (0, 0). Note how many are within that radius vs total.

---

## Remove in Radius

Remove all cubes within 1000 units of (0, 0).

---

How many cubes remain? Should be fewer than before.

---

## Remove All of Type

Remove all remaining cubes from the level.

---

Get instance count for cubes. Should return -1 or 0.

---

Spheres should still exist. Verify.

---

## Clear All Foliage

Clear all foliage from the level.

---

List foliage types. Should be empty.

---

## Foliage Type Asset Creation

Create a foliage type called "FT_TestTree" from "/Engine/BasicShapes/Cone" saved to "/Game/FoliageTest" with:
- min scale 0.5
- max scale 2.0
- align to normal true
- ground slope max angle 30

---

Does the foliage type asset exist at "/Game/FoliageTest/FT_TestTree"?

---

Get the AlignMaxAngle property from the foliage type. Should be 30.

---

Set the CullDistance.Max property to 30000.

---

## Scatter with Foliage Type Asset

Scatter 100 instances using the foliage type asset "/Game/FoliageTest/FT_TestTree" in a 4000-unit radius around (0, 0) with seed 123.

---

List foliage types. Should show FT_TestTree with ~100 instances.

---

## Layer-Aware Scatter (requires painted landscape)

If FoliageTestTerrain has paint layers, scatter 200 instances of "/Engine/BasicShapes/Sphere" only where the first paint layer weight > 0.5.

---

Check instances_rejected — should be > 0 if only part of the landscape is painted.

---

## Cleanup

Clear all foliage.

---

Delete all assets under /Game/FoliageTest.

---

Delete FoliageTestTerrain.

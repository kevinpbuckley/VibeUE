# Landscape Material Tests

Tests for landscape materials, layer blending, and full terrain setup. Start from an empty open world level. Run sequentially.

---

## Empty State Checks

Does a landscape material called M_LandscapeTest exist anywhere?

---

Does a layer info object exist at /Game/LandscapeTest/LI_Grass?

---

## Create Landscape Material

Create a new landscape material called M_LandscapeTest in /Game/LandscapeTest/Materials.

---

Verify it exists.

---

## Build the Layer Blend

Add a layer blend node to M_LandscapeTest at position (-400, 0).

---

Add a "Grass" layer with WeightBlend.

---

Add a "Rock" layer with WeightBlend.

---

Add a "Sand" layer with HeightBlend.

---

What layers are on the blend node now? Should be three.

---

## Connect Blend to Output

Connect the blend node to BaseColor.

---

## Add Tiling Coordinates

Create a landscape layer coords node with mapping scale 0.01 at (-800, 0).

---

## Compile and Save

Compile M_LandscapeTest.

---

Save it.

---

## Create All Layer Info Objects

Create a weight-blended layer info for "Grass" in /Game/LandscapeTest.

---

Create a weight-blended layer info for "Rock" in /Game/LandscapeTest.

---

Create a non-weight-blended layer info for "Sand" in /Game/LandscapeTest.

---

## Verify Layer Info

Does /Game/LandscapeTest/LI_Grass exist?

---

Get details on the Grass layer info. Confirm it's weight-blended.

---

Get details on the Sand layer info. Confirm it's NOT weight-blended.

---

## Layer Weight Node (Alternative Approach)

Create a layer weight node for "Snow" with preview weight 0.5 at (-400, 400) in M_LandscapeTest.

---

## Remove a Layer

Remove "Sand" from the blend node.

---

Check the blend node. Should only have Grass and Rock now.

---

## End-to-End: Create Landscape and Assign Material

Create a landscape at the origin called "MatTestTerrain" with default settings.

---

Assign M_LandscapeTest to MatTestTerrain, mapping Grass and Rock to their layer info objects.

---

What layers does MatTestTerrain have?

---

## Paint to Verify Material

Paint Grass at (0, 0) with radius 2000 and full strength.

---

Paint Rock at (4000, 4000) with radius 1500 and full strength.

---

Check layer weights at (0, 0).

---

Check layer weights at (4000, 4000).

---

## Cleanup

Delete MatTestTerrain.

---

Delete all assets under /Game/LandscapeTest - material, layer infos, everything. Force delete.

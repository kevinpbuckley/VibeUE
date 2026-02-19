# Landscape Service Tests

Tests for creating and managing landscapes. Start from an empty open world level. Run sequentially.

---

## Discovery - Empty Level

Are there any landscapes in the current level?

---

Does a landscape called "TestTerrain" exist?

---

## Create First Landscape

Create a new landscape at the origin with default settings. Call it "TestTerrain".

---

List all landscapes now. TestTerrain should show up.

---

Get detailed info about the TestTerrain landscape - show me everything.

---

## Properties

What's the current scale on TestTerrain?

---

How many components does it have?

---

## Heightmap Basics

Get the height at location (0, 0) on TestTerrain.

---

What about at (500, 500)?

---

## Sculpting - Raise Terrain

Sculpt a hill at (0, 0) on TestTerrain with radius 2000, strength 0.5, and smooth falloff.

---

Check the height at (0, 0) again. It should be higher now.

---

## Sculpting - Smooth

Smooth the area around (0, 0) with radius 2000 and strength 0.3.

---

Smooth (0, 0) again with radius 5000, strength 1.0, and Linear falloff. Should produce a much stronger effect.

---

## Sculpting - Flatten

Flatten a plateau at (3000, 3000) with radius 1500, target height 500, and strength 1.0.

---

Check the height at (3000, 3000). Should be close to 500.

---

Flatten (5000, 0) with radius 2000, target 200, strength 0.8, and Spherical falloff type.

---

## Raise/Lower Region

Raise a region centered at (8000, 0) that's 4000x4000 by 500 world units with a falloff width of 1500.

---

Check the height at (8000, 0). Should be raised by about 500.

---

Check the height at (10500, 0) — should be in the falloff zone, partially raised.

---

## Apply Noise with Stats

Apply noise at (0, 0) with radius=8000, amplitude=300, frequency=0.005, seed=42, and 6 octaves. Check the result — it should return min/max delta, vertices modified, and saturation count.

---

## Set Height Region

Set a 4x4 region of heights starting at (5000, 5000). Make it ramp up from left to right.

---

## Material Setup (Full Pipeline)

Before we can paint layers, we need a material. Create a landscape material called M_TestTerrain in /Game/TestLandscape/Materials.

---

Add a layer blend node to M_TestTerrain.

---

Add a "Grass" layer to the blend node with WeightBlend type.

---

Add a "Rock" layer to the blend node with WeightBlend type.

---

Connect the blend node to the BaseColor output.

---

Compile and save M_TestTerrain.

---

## Layer Info Objects

Create a weight-blended layer info object for "Grass" in /Game/TestLandscape.

---

Create a weight-blended layer info object for "Rock" in /Game/TestLandscape.

---

## Assign Material to Landscape

Assign M_TestTerrain to TestTerrain with the Grass and Rock layer info objects we just created.

---

## Layer Verification

What layers does TestTerrain have now?

---

Does a layer called "Grass" exist on TestTerrain?

---

Does a layer called "Rock" exist on TestTerrain?

---

Does a layer called "Dirt" exist? It shouldn't.

---

## Paint Layers

Paint Grass at location (0, 0) with radius 2000 and full strength.

---

Paint Rock at location (3000, 3000) with radius 1500 and full strength.

---

Get the layer weights at (0, 0). Grass should dominate.

---

Get the layer weights at (3000, 3000). Rock should dominate.

---

## Visibility

Hide TestTerrain.

---

Show it again.

---

## Collision

Disable collision on TestTerrain.

---

Re-enable collision.

---

## Second Landscape

Create another landscape at (100000, 0, 0) called "TestTerrain2" with 4 components, 63 quads per section, and 2 sections per component.

---

List all landscapes. Both should be there.

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

---

## Heightmap Region (Bulk Read/Write)

Read the heights for the full TestTerrain landscape using `get_height_in_region`. Start at vertex (0, 0) and read the full resolution. Report how many height samples you got.

---

Take those heights, add 200 world units to every sample, then write them back with `set_height_in_region`. After writing, read the height at (0, 0) — it should be ~200 units higher than before.

---

## Batch Painting — paint_layer_in_region

Read the full landscape info for TestTerrain. Using `paint_layer_in_region`, paint the entire landscape with full Grass weight (strength 1.0) starting at vertex (0, 0).

---

Now paint a 50×50 vertex square in the top-left corner (starting at vertex 0, 0) with full Rock weight using `paint_layer_in_region`. Use strength 1.0.

---

Read the layer weights at world location (0, 0) using `get_layer_weights_at_location`. Rock should now dominate in that corner.

---

## Batch Painting — paint_layer_in_world_rect

Use `paint_layer_in_world_rect` to paint Rock in the world-space rectangle from (-5000, -5000) to (5000, 5000) at 80% strength.

---

Check the layer weights at world location (0, 0). Rock should still dominate.

---

Use `paint_layer_in_world_rect` to paint Grass over the same rectangle at 100% strength to reset it.

---

## Weight Map Bulk Read/Write

Use `get_weights_in_region` to read all Grass weights for the full TestTerrain resolution (vertex 0,0 to full size). Report the total number of samples and the min/max weight values found.

---

Take the weights you just read, invert them (new_weight = 1.0 - old_weight for each), and write them back using `set_weights_in_region`. Report success.

---

Read the layer weights at world (0, 0) again. Grass should now be close to 0 (since it was previously 1.0 and we inverted). Report the values.

---

Reset by painting the full landscape with Grass at 100% using `paint_layer_in_region` again so future tests have a clean base.

---

## Weight Map Export / Import

Export the Grass weight map to a temporary file (e.g. `C:/Temp/TestTerrain_Grass.png`) using `export_weight_map`. Report the dimensions and confirm the file was written.

---

Export the Rock weight map to `C:/Temp/TestTerrain_Rock.png`. Report dimensions.

---

Now paint Rock at strength 1.0 across the entire landscape using `paint_layer_in_region` — this changes the weight map so we can verify the import restores it.

---

Import the original Grass weight map back from `C:/Temp/TestTerrain_Grass.png` using `import_weight_map`. Report how many vertices were modified.

---

Check the Grass weights at world (0, 0) and (5000, 5000) — they should be restored to their pre-overwrite values. Report the weights.

---

## Landscape Holes

Check whether the vertex at world location (0, 0) is a hole using `get_hole_at_location`. It should be false (solid terrain).

---

Punch a circular hole at world location (0, 0) with radius 1000 using `set_hole_at_location`. Report success.

---

Check again whether (0, 0) is a hole. Should now be true.

---

Check (5000, 5000) — should still be false (solid, outside the hole radius).

---

Fill the hole back at (0, 0) with radius 1000 using `set_hole_at_location` with `create_hole=False`. Report success.

---

Verify (0, 0) is solid again using `get_hole_at_location`.

---

Now punch a rectangular hole covering a 30×30 vertex region starting at vertex (20, 20) using `set_hole_in_region`.

---

Fill the entire landscape (all vertices) back to solid using `set_hole_in_region` with `create_hole=False`, starting at vertex (0, 0) over the full resolution. This ensures no stray holes remain for future tests.

---

## Landscape Splines — Basic

How many spline points does TestTerrain currently have? Use `get_spline_info`. Report `num_control_points` and `num_segments`.

---

Create a spline point on TestTerrain at world location (0, 0, 500) using `create_spline_point`. Width 400, side_falloff 300, end_falloff 300. Paint under it using the "Grass" layer. Report the returned point_index.

---

Create a second spline point at world location (5000, 0, 600). Same settings. Report the point_index.

---

Create a third spline point at world location (10000, 3000, 500). Report the point_index.

---

Use `get_spline_info` to confirm there are now 3 control points and 0 segments.

---

Connect point 0 to point 1 using `connect_spline_points`. Use auto tangent (0). Paint under it with "Grass". Report success.

---

Connect point 1 to point 2. Report success.

---

Use `get_spline_info` again — should now have 3 points and 2 segments. Report the segment details (start index, end index, tangent lengths).

---

## Landscape Splines — Modify and Apply

Move point 1 to world location (5000, 500, 700) using `modify_spline_point`. Leave width unchanged (pass -1). Report success.

---

Apply the splines to deform terrain using `apply_splines_to_landscape`. Report success.

---

Check the height at world (5000, 0) — it should have been raised or lowered to approximate the spline height. Report the value.

---

## Landscape Splines — Delete and Rebuild

Delete spline point 0 using `delete_spline_point`. Report success.

---

Use `get_spline_info` — point 0 is gone, the segment connecting it should also be removed. How many points and segments remain?

---

Delete all remaining splines using `delete_all_splines`.

---

`get_spline_info` should now return 0 points and 0 segments. Confirm.

---

## Landscape Splines — create_spline_from_points

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

Delete all splines to clean up before the resize test.

---

## Landscape Resize

Get the full info for TestTerrain — report its current resolution (resolution_x, resolution_y) and component counts.

---

Resize TestTerrain from its current configuration to a 4×4 component grid using `resize_landscape`. Keep the same quads_per_section and sections_per_component (pass -1 for both).

---

Report the new landscape's actor label and get its full info. What is the new resolution?

---

Verify the landscape still exists and has the same world location as the original. Check that the Grass layer still exists after the resize.

---

Get the height at world (0, 0) on the resized landscape. The terrain shape should be approximately preserved (bilinearly resampled). Report the height.

---

Get the Grass layer weight at (0, 0) on the resized landscape. It should be approximately 1.0 (since we last painted Grass at full strength). Report the weight.

---


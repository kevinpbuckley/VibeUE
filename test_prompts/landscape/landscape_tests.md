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


---

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


---

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


---

# M_Landscape Full Recreation Test

Recreate `/Game/Stylized_Spruce_Forest/Materials/Master_Materials/M_Landscape` as `M_Landscape2` using only VibeUE landscape material tools — zero asset duplication. Every expression, connection, property, parameter, function call, HLSL node, and material output must be reproduced exactly.

**Source:** `/Game/Stylized_Spruce_Forest/Materials/Master_Materials/M_Landscape`
**Target:** `/Game/Stylized_Spruce_Forest/Materials/Master_Materials/M_Landscape2`

Run sequentially. Each step depends on the previous. Do NOT skip steps.

---

## Phase 0: Pre-Flight Checks

### 0.1 Verify Source Exists

Does `/Game/Stylized_Spruce_Forest/Materials/Master_Materials/M_Landscape` exist? Get its summary.

---

### 0.2 Verify Target Does Not Exist

Does `/Game/Stylized_Spruce_Forest/Materials/Master_Materials/M_Landscape2` exist? It should NOT.

---

### 0.3 Source Material Summary

Summarize M_Landscape. Report: expression count, parameter count, blend mode, shading model, two-sided flag, material domain.

---

## Phase 1: Export Source Material Graph

### 1.1 Full Graph Export

Export M_Landscape's complete graph as JSON using `export_graph`. Save the JSON output — we'll reference it throughout.

---

### 1.2 Analyze Export — Expression Census

Parse the exported JSON. Report:
- Total expression count
- Count by expression class (sorted descending)
- How many are parameters (is_parameter=true)
- How many have function_path set (MaterialFunctionCall nodes)
- How many have hlsl_code set (Custom HLSL nodes)
- How many have collection_path set (CollectionParameter nodes)

---

### 1.3 Analyze Export — Connection Census

From the export JSON, report:
- Total connection count
- How many material output connections are set and which outputs (BaseColor, Normal, Roughness, etc.)
- Top 10 most-connected target expressions (by number of incoming connections)

---

### 1.4 Analyze Export — Parameter Census

From the export JSON, list every parameter:
- Parameter name
- Expression class (ScalarParameter, VectorParameter, StaticSwitchParameter, TextureObjectParameter, etc.)
- Group name
- Default value (from properties)

There should be ~42 parameters. List them all.

---

### 1.5 Analyze Export — Landscape-Specific Nodes

From the export JSON, identify all landscape-specific expression types:
- LandscapeLayerBlend (count, layer names configured)
- LandscapeLayerCoords (count, mapping scale values)
- LandscapeLayerSample (count, layer names)
- LandscapeLayerWeight (count, layer names)
- LandscapeGrassOutput (count, grass type mappings)

---

### 1.6 Analyze Export — Material Function Calls

List every MaterialFunctionCall node with:
- Node ID
- Function path
- Input names
- Output names
- Position

There should be ~32 of these.

---

### 1.7 Analyze Export — Custom HLSL Nodes

List every Custom HLSL expression with:
- Node ID
- Description
- HLSL code (full text)
- Output type
- Input names
- Position

There should be ~8 of these.

---

### 1.8 Analyze Export — Collection Parameters

List every CollectionParameter node with:
- Node ID
- Collection path
- Parameter name within collection
- Position

There should be ~3 of these.

---

### 1.9 Analyze Export — Texture References

From the export JSON, find all expressions that reference textures:
- TextureSample nodes (count, texture asset paths from properties)
- TextureObject nodes (count, texture paths)
- TextureObjectParameter nodes (count, parameter names, default texture paths)
- TextureSampleParameter2D nodes (count, parameter names, default texture paths)

---

### 1.10 Analyze Export — Reroute Nodes

Count all Reroute expression nodes. Report their IDs and positions. These are simple pass-through nodes but critical for connection routing.

---

## Phase 2: Create Target Material

### 2.1 Create M_Landscape2

Create a new material called `M_Landscape2` in `/Game/Stylized_Spruce_Forest/Materials/Master_Materials/`.

---

### 2.2 Open M_Landscape2 in Editor

Open M_Landscape2 in the material editor so we can watch it populate in real time as expressions and connections are added.

---

### 2.3 Set Material Properties

From the export, set the exact same material properties on M_Landscape2:
- Blend mode (should be Masked)
- Shading model
- Two-sided flag
- Any other material-level properties that differ from defaults

List each property you set and its value.

---

### 2.4 Verify Material Properties

Get M_Landscape2's properties. Compare against M_Landscape's properties. Report any differences.

---

## Phase 3: Recreate All Expressions

This is the core of the test. Every expression from M_Landscape must be created in M_Landscape2, maintaining the old_id → new_id mapping for connections later.

### 3.1 Batch Create — Simple Expression Nodes

From the export, collect all expressions that are NOT:
- MaterialFunctionCall (needs `create_function_call`)
- Custom (needs `create_custom_expression`)
- CollectionParameter (needs `create_collection_parameter`)

Use `batch_create` to create them all at their original positions. Report:
- How many expressions sent to batch_create
- How many successfully created
- Any failures and their class names

Map every old ID to its new ID.

---

### 3.2 Create MaterialFunctionCall Nodes

For each MaterialFunctionCall in the export, use `create_function_call` with:
- The exact function_path from the export
- The exact position (pos_x, pos_y)

Report each function call created:
- Old ID → New ID
- Function path
- Resulting input/output pin names

If any function_path fails to load, report it as a critical error.

---

### 3.3 Create Custom HLSL Nodes

For each Custom expression in the export, use `create_custom_expression` with:
- The exact hlsl_code
- The exact output_type
- The description
- Input names (from the export's inputs array)
- The exact position

Report each created:
- Old ID → New ID
- Description
- Input/output count

---

### 3.4 Create CollectionParameter Nodes

For each CollectionParameter in the export, use `create_collection_parameter` with:
- The exact collection_path
- The exact parameter_name
- The exact position

Report each created:
- Old ID → New ID
- Collection path
- Parameter name

---

### 3.5 Verify Expression Count

List all expressions in M_Landscape2. Count them. Compare to M_Landscape's count. They MUST match exactly.

If they don't match, identify which expressions are missing and create them.

---

## Phase 4: Set All Expression Properties

### 4.1 Batch Set Properties — Parameters

For every parameter expression (ScalarParameter, VectorParameter, StaticSwitchParameter, TextureObjectParameter, etc.), set:
- ParameterName
- Group
- DefaultValue / Default constant
- SortPriority (if present)
- Any other parameter-specific properties from the export

Use `batch_set_properties` for efficiency. Report how many properties were set successfully.

---

### 4.2 Batch Set Properties — ComponentMask Nodes

For every ComponentMask expression, set:
- R (True/False)
- G (True/False)
- B (True/False)
- A (True/False)

Use `batch_set_properties`. Report count.

---

### 4.3 Batch Set Properties — Constant Nodes

For every Constant, Constant3Vector, Constant4Vector expression, set their value properties from the export:
- Constant: `R` property (float value)
- Constant3Vector: `Constant` property (`(R=x,G=y,B=z,A=1.0)` format)
- Constant4Vector: `Constant` property

Use `batch_set_properties`. Report count.

---

### 4.4 Batch Set Properties — Texture References

For every TextureSample and TextureObject expression, set:
- `Texture` property to the asset path from the export
- `SamplerType` if present

Use `batch_set_properties`. Report how many textures were set and any that failed to resolve.

---

### 4.5 Batch Set Properties — Transform Nodes

For every Transform expression, set:
- `TransformSourceType` (enum value from export)
- `TransformType` (enum value from export)

---

### 4.6 Batch Set Properties — ConstantBiasScale Nodes

For every ConstantBiasScale expression, set:
- `Bias`
- `Scale`

---

### 4.7 Batch Set Properties — TextureCoordinate Nodes

For every TextureCoordinate expression, set:
- `UTiling`
- `VTiling`
- `CoordinateIndex`

---

### 4.8 Batch Set Properties — StaticBool Nodes

For every StaticBool expression, set:
- `Value` (True/False)

---

### 4.9 Batch Set Properties — LandscapeLayerCoords Nodes

For every LandscapeLayerCoords expression, set:
- `MappingScale`
- `MappingRotation` (if present)
- `MappingPanU` / `MappingPanV` (if present)
- `MappingType` (if present)

---

### 4.10 Batch Set Properties — LandscapeLayerSample Nodes

For every LandscapeLayerSample expression, set:
- `ParameterName` (the layer name)
- `PreviewWeight` (if present)

---

### 4.11 Batch Set Properties — Clamp Nodes

For every Clamp expression, set:
- `ClampMode` (if customized)
- `MinDefault` / `MaxDefault` (if present)

---

### 4.12 Batch Set Properties — Remaining Properties

For any expressions with exported properties not yet covered, set them now using `batch_set_properties`. This includes:
- Reroute node properties (usually none)
- Any other per-class properties the export captured

Report total properties set across all batch operations.

---

### 4.13 Verify Properties — LandscapeLayerBlend

The LandscapeLayerBlend node has layer configuration that might not be captured by generic property export. Check:
- Were layers created correctly? (use `get_layer_blend_info`)
- If no layers exist, use `add_layer_to_blend_node` for each layer from the export
- Set the correct blend type for each layer (WeightBlend, HeightBlend, AlphaBlend)

Report the final layer blend configuration.

---

### 4.14 Verify Properties — LandscapeGrassOutput

If a LandscapeGrassOutput node exists, verify its grass type mappings are set. If not, note this as a limitation.

---

## Phase 5: Recreate All Connections

### 5.1 Map Connection IDs

From the export's connections array, translate every old source_id and target_id to the new IDs using the mapping built in Phase 3. Report:
- Total connections to recreate
- How many have both source and target mapped
- Any unmappable connections (missing IDs)

---

### 5.2 Batch Connect — All Wires

Use `batch_connect` to create all connections at once. For each connection:
- source_id → mapped new source ID
- source_output_index → convert to output name using the export's outputs array
- target_id → mapped new target ID
- target_input → use the target_input name from the export

Report:
- How many connections attempted
- How many succeeded
- Any failures (list source class → target class and input name)

---

### 5.3 Verify Connection Count

List all connections in M_Landscape2. Compare count to M_Landscape's connection count. Report any difference.

---

### 5.4 Connect Layer Inputs on LandscapeLayerBlend

If the layer blend node's layer inputs were not connected via the generic batch_connect (because layer inputs use a special format), use `connect_to_layer_input` for each layer:
- Connect the correct source expression to each layer's "Layer" input
- Connect height sources to "Height" inputs if using height blend

Report each layer connection made.

---

## Phase 6: Connect Material Outputs

### 6.1 Connect All Material Outputs

From the export's output_connections, connect each using `connect_to_output`:
- BaseColor
- Metallic
- Specular
- Roughness
- EmissiveColor
- Opacity
- OpacityMask
- Normal
- WorldPositionOffset
- AmbientOcclusion
- SubsurfaceColor
- PixelDepthOffset
- Any other connected outputs

Report each output connected and which expression it's wired to.

---

### 6.2 Verify Material Output Connections

Get M_Landscape2's output connections. Compare to M_Landscape's. Every connected output must match.

---

## Phase 7: Compile and Save

### 7.1 Compile M_Landscape2

Compile the material. Report success/failure. If compilation fails, report the errors.

---

### 7.2 Save M_Landscape2

Save the material to disk.

---

## Phase 8: Full Verification

### 8.1 Export M_Landscape2

Export M_Landscape2's graph as JSON.

---

### 8.2 Compare Expression Counts

Compare M_Landscape vs M_Landscape2:
- Total expression count
- Count by class

Report any differences.

---

### 8.3 Compare Connection Counts

Compare total connections and output connections.

---

### 8.4 Compare Parameters

List all parameters in both materials. Compare:
- Same parameter names
- Same parameter types
- Same default values
- Same groups

Report any differences.

---

### 8.5 Compare Material Properties

Compare blend mode, shading model, two-sided, domain between both materials.

---

### 8.6 Compare Function Calls

Verify every MaterialFunctionCall in M_Landscape2 references the same function path as M_Landscape.

---

### 8.7 Compare HLSL Code

Verify every Custom expression in M_Landscape2 has identical HLSL code to M_Landscape.

---

### 8.8 Compare Texture References

Verify all TextureSample/TextureObject nodes reference the same textures.

---

### 8.9 Final Summary Report

Print a comprehensive comparison report:
```
=== M_Landscape Recreation Report ===
Source: M_Landscape
Target: M_Landscape2

Expressions:  [source_count] → [target_count] [MATCH/MISMATCH]
Connections:  [source_count] → [target_count] [MATCH/MISMATCH]
Parameters:   [source_count] → [target_count] [MATCH/MISMATCH]
FunctionCalls: [source_count] → [target_count] [MATCH/MISMATCH]
CustomHLSL:   [source_count] → [target_count] [MATCH/MISMATCH]
CollectionParams: [source_count] → [target_count] [MATCH/MISMATCH]
Output Connections: [source_count] → [target_count] [MATCH/MISMATCH]
Blend Mode:    [value] [MATCH/MISMATCH]
Shading Model: [value] [MATCH/MISMATCH]
Compiled:      [YES/NO]

Overall: [PASS/FAIL]
Differences: [list any]
```

---

## Phase 9: Visual Verification (Optional — Requires Landscape)

### 9.1 Create Test Landscape

If a landscape exists in the level, use it. Otherwise create a small test landscape.

---

### 9.2 Assign M_Landscape to Half

Assign M_Landscape to the test landscape with its layer info objects. Paint a test pattern.

---

### 9.3 Screenshot M_Landscape

Take a screenshot for visual reference.

---

### 9.4 Assign M_Landscape2

Assign M_Landscape2 to the same landscape with the same layer info objects.

---

### 9.5 Screenshot M_Landscape2

Take a screenshot for comparison.

---

### 9.6 Visual Comparison

Report whether the two screenshots look visually identical. Note any differences.

---


---

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


---

# Landscape Recreation Demo

recreate landscape1 as landscape2. paint it the same, foliange the same. water the same, have it to the right so I can clearly compare them.  don't use export import we want to test our tools ability to create things.

---

# Auto-Material Tool Tests

Tests for the new landscape auto-material workflow tools. Run sequentially.

---

## Skill + Preflight

Load the `landscape-auto-material` skill.

---

Does `/Game/Real_Landscape` exist?

---

Does a landscape named `AutoMatTerrain` already exist?

---

If `AutoMatTerrain` does not exist, create a landscape at origin named `AutoMatTerrain` with default settings.

---

## Texture Discovery

Run `FindLandscapeTextures` on `/Game/Real_Landscape` with no filter. Report how many texture sets were found.

---

Run `FindLandscapeTextures` on `/Game/Real_Landscape` with filter `Grass`. Report matches.

---

Run `FindLandscapeTextures` on `/Game/Real_Landscape` with filter `Rock`. Report matches.

---

Run `FindLandscapeTextures` on `/Game/DoesNotExist` with no filter. Confirm graceful failure or empty results (no crash).

---

## Build Auto Material From Discovered Textures

Create an auto-material named `M_AutoLandscape_Test` in `/Game/LandscapeAutoTest/Materials` using 3 discovered layers (Grass, Rock, Dirt or closest available names).

---

Assign the new material to `AutoMatTerrain` during creation if supported.

---

Create layer info assets in `/Game/LandscapeAutoTest/LayerInfos` for each generated layer.

---

Verify `/Game/LandscapeAutoTest/Materials/M_AutoLandscape_Test` exists.

---

Get a summary of `M_AutoLandscape_Test` and report expression count and landscape-layer related nodes.

---

Compile and save `M_AutoLandscape_Test`.

---

## Validation + Failure Cases

Attempt to create `M_AutoLandscape_Test` again in the same path. Confirm duplicate-name handling returns a clear error.

---

Get landscape layer info for `AutoMatTerrain` and confirm generated layers are present.

---

Paint one generated layer at `(0, 0)` with full strength and radius `1500`, then read back layer weights at `(0, 0)`.

---

Paint a second generated layer at `(3000, 0)` with full strength and radius `1500`, then read back layer weights at `(3000, 0)`.

---

Return a final pass/fail checklist for:
- texture discovery
- auto-material creation
- layer info creation
- landscape assignment
- duplicate protection


---

# Runtime Virtual Texture Pipeline Tests

Tests for the new Runtime Virtual Texture service tools. Run sequentially.

---

## Skill + Preflight

Load the `landscape-auto-material` skill.

---

Verify a landscape named `AutoMatTerrain` exists. If not, create it at origin with default settings.

---

Verify `/Game/LandscapeAutoTest/Materials/M_AutoLandscape_Test` exists. If not, create a basic landscape material and assign it to `AutoMatTerrain`.

---

## RVT Asset Creation

Create an RVT asset named `RVT_AutoTest` in `/Game/LandscapeAutoTest/RVT` with:
- material type: `BaseColor_Normal_Roughness`
- tile count: `256`
- tile size: `256`
- tile border size: `4`
- continuous update: `true`
- single physical space: `false`

---

Verify `/Game/LandscapeAutoTest/RVT/RVT_AutoTest` exists.

---

Get runtime virtual texture info for `RVT_AutoTest` and report all fields.

---

## RVT Volume + Landscape Assignment

Create an RVT volume for `AutoMatTerrain` using `RVT_AutoTest` with actor label `RVT_Vol_AutoTest`.

---

Verify the RVT volume actor exists in the level and report its label/name.

---

Assign `RVT_AutoTest` to `AutoMatTerrain` at slot `0`.

---

Read back landscape RVT slots and confirm slot `0` references `RVT_AutoTest`.

---

## Additional RVT Type

Create a second RVT asset named `RVT_AutoTest_WorldHeight` in `/Game/LandscapeAutoTest/RVT` with material type `WorldHeight`.

---

Get runtime virtual texture info for `RVT_AutoTest_WorldHeight` and confirm material type is `WorldHeight`.

---

Assign `RVT_AutoTest_WorldHeight` to `AutoMatTerrain` at slot `1`.

---

Read back landscape RVT slots and confirm slot `1` references `RVT_AutoTest_WorldHeight`.

---

## Failure Cases

Attempt to create `RVT_AutoTest` again in `/Game/LandscapeAutoTest/RVT`. Confirm duplicate-name handling returns a clear error.

---

Attempt to create an RVT volume for non-existent landscape `NoSuchLandscape`. Confirm it fails gracefully.

---

Attempt to assign RVT to non-existent landscape `NoSuchLandscape` at slot `0`. Confirm it returns false/error without crashing.

---

Attempt to get RVT info for invalid path `/Game/Nope/NoRVT`. Confirm clear error text.

---

Return a final pass/fail checklist for:
- RVT asset creation
- RVT info introspection
- RVT volume creation
- landscape slot assignment
- invalid input handling


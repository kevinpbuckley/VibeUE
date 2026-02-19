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

### 2.2 Set Material Properties

From the export, set the exact same material properties on M_Landscape2:
- Blend mode (should be Masked)
- Shading model
- Two-sided flag
- Any other material-level properties that differ from defaults

List each property you set and its value.

---

### 2.3 Verify Material Properties

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


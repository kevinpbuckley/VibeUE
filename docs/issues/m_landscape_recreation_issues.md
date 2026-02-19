# M_Landscape Recreation — Open Issues

Issues discovered while designing the full M_Landscape → M_Landscape2 recreation test. Each issue may block or degrade faithful recreation of all 782 expressions.

**Status: ALL ISSUES RESOLVED** — Implementation complete across `UMaterialNodeService` and `ULandscapeMaterialService`.

---

## ISSUE-1: Export `source_output_index` is numeric, but `batch_connect` expects output names

**Status:** ✅ RESOLVED — Added `source_output_name` string field to connection objects in `ExportMaterialGraph`  
**Severity:** P0 — Blocks all connection recreation  
**Component:** `MaterialNodeService::ExportMaterialGraph` / `BatchConnectExpressions`

**Problem:**  
`ExportMaterialGraph` exports connections with `source_output_index` (integer), but `BatchConnectExpressions` and `ConnectExpressions` both take `SourceOutput` as a **string name** (or empty string for first output). There is no way to map index→name from the export JSON alone because the export doesn't include which output index maps to which output name on the source node.

**Example from export:**
```json
{
  "source_id": "temp_42",
  "source_output_index": 2,
  "target_id": "temp_100",
  "target_input": "A"
}
```

The connect API needs `source_output: "RGB"` (not `2`). The consumer must look up the source expression's outputs array from the export to convert index→name, but if the outputs are named `["Output_0", "Output_1", "RGB"]`, it works. However, many expressions have auto-generated names like `Output_0` which may not match what `ConnectExpressions` expects.

**Fix Option A (Preferred):** Add `source_output_name` to the connection export by resolving the index to the output name at export time:
```cpp
// In ExportMaterialGraph connections loop:
FString OutputName;
if (Input->OutputIndex < Outputs.Num())
    OutputName = Outputs[Input->OutputIndex].OutputName.IsNone() ? TEXT("") : Outputs[Input->OutputIndex].OutputName.ToString();
ConnObj->SetStringField(TEXT("source_output_name"), OutputName);
```

**Fix Option B:** Add index-based connect overload:
```cpp
static bool ConnectExpressionsByIndex(const FString& MaterialPath,
    const FString& SourceId, int32 SourceOutputIndex,
    const FString& TargetId, const FString& TargetInput);
```

---

## ISSUE-2: LandscapeLayerBlend layers not captured in generic property export

**Status:** ✅ RESOLVED — Added `landscape_layers` array to export for LandscapeLayerBlend nodes  
**Severity:** P0 — Blocks landscape layer blend recreation  
**Component:** `MaterialNodeService::ExportMaterialGraph`

**Problem:**  
The `ExportMaterialGraph` uses `FProperty::ExportTextItem_Direct` for editable properties, but `UMaterialExpressionLandscapeLayerBlend::Layers` is a `TArray<FLayerBlendInput>` struct array. The generic export may serialize it as a cryptic Unreal struct string that can't be fed back via `set_expression_property`. The layer configuration (layer names, blend types, preview weights) needs explicit handling.

**Current state:** `LandscapeMaterialService::GetLayerBlendInfo` can read the blend node, and `AddLayerToBlendNode` can add layers. But the consumer of the export must know to use these APIs instead of generic property setting.

**Fix Option A (Preferred):** Add explicit layer blend data to the export for LandscapeLayerBlend nodes:
```cpp
// In ExportMaterialGraph, after generic properties:
if (UMaterialExpressionLandscapeLayerBlend* LayerBlend = Cast<UMaterialExpressionLandscapeLayerBlend>(Expr))
{
    TArray<TSharedPtr<FJsonValue>> LayersArr;
    for (const FLayerBlendInput& Layer : LayerBlend->Layers)
    {
        TSharedRef<FJsonObject> LayerObj = MakeShared<FJsonObject>();
        LayerObj->SetStringField("layer_name", Layer.LayerName.ToString());
        LayerObj->SetStringField("blend_type", UEnum::GetValueAsString(Layer.BlendType));
        LayerObj->SetNumberField("preview_weight", Layer.PreviewWeight);
        LayersArr.Add(MakeShared<FJsonValueObject>(LayerObj));
    }
    ExprObj->SetArrayField("landscape_layers", LayersArr);
}
```

**Fix Option B:** Document that consumers must use `LandscapeMaterialService` APIs to configure blend layers after batch-creating the expression.

---

## ISSUE-3: `batch_create` creates generic nodes — specialized nodes require individual calls

**Status:** ✅ RESOLVED — Added `BatchCreateSpecialized` method with `FBatchCreateDescriptor` struct  
**Severity:** P1 — Performance impact for large materials  
**Component:** `MaterialNodeService::BatchCreateExpressions`

**Problem:**  
`BatchCreateExpressions` creates nodes by class name but cannot set:  
- **Function references** for MaterialFunctionCall (32 nodes in M_Landscape)
- **HLSL code** for Custom expressions (8 nodes)
- **Collection references** for CollectionParameter (3 nodes)

These require separate individual API calls (`create_function_call`, `create_custom_expression`, `create_collection_parameter`), adding ~43 extra roundtrips.

**Enhancement:** Add `BatchCreateSpecialized` that accepts an array of typed creation descriptors:
```cpp
USTRUCT()
struct FBatchCreateDescriptor
{
    FString ClassName;
    int32 PosX;
    int32 PosY;
    // Optional specialized fields:
    FString FunctionPath;      // For MaterialFunctionCall
    FString HLSLCode;          // For Custom
    FString HLSLOutputType;    // For Custom
    FString HLSLDescription;   // For Custom
    FString HLSLInputNames;    // For Custom
    FString CollectionPath;    // For CollectionParameter
    FString CollectionParamName; // For CollectionParameter
};

static TArray<FMaterialExpressionInfo> BatchCreateSpecialized(
    const FString& MaterialPath,
    const TArray<FBatchCreateDescriptor>& Descriptors);
```

This would allow all 782 nodes to be created in a single transaction.

---

## ISSUE-4: Export doesn't capture Custom expression input definitions

**Status:** ✅ RESOLVED — Added `custom_input_names` and `custom_additional_outputs` fields to export  
**Severity:** P1 — May cause Custom HLSL nodes to have wrong input pins  
**Component:** `MaterialNodeService::ExportMaterialGraph`

**Problem:**  
For `MaterialExpressionCustom`, the export captures `hlsl_code` and `output_type`, but the **custom input pin definitions** are stored in `CustomExpr->Inputs` (a `TArray<FCustomInput>`), which are separate from the generic `GetInputsView()`. The generic inputs array in the export will show the final resolved input names, but `create_custom_expression` takes `InputNames` as a comma-separated string to define them.

The consumer must parse the export's `inputs` array to reconstruct the `InputNames` string for creation. This should work, but needs verification that the exported input names match what `create_custom_expression` expects.

**Fix:** Add explicit `custom_input_names` field to the export for Custom nodes:
```cpp
if (UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(Expr))
{
    TArray<FString> InputNames;
    for (const FCustomInput& CI : CustomExpr->Inputs)
        InputNames.Add(CI.InputName.ToString());
    ExprObj->SetStringField("custom_input_names", FString::Join(InputNames, TEXT(",")));
}
```

---

## ISSUE-5: TextureObjectParameter creation not verified end-to-end

**Status:** ⏳ PENDING TEST — Requires e2e verification with recreation test  
**Severity:** P1 — May block 14 TextureObjectParameter nodes  
**Component:** `MaterialNodeService::CreateParameter`

**Problem:**  
`CreateParameter` accepts `"TextureObject"` as a parameter type (per the header doc), but the M_Landscape material has 14 `TextureObjectParameter` nodes. The end-to-end flow of:
1. `batch_create("TextureObjectParameter", ...)` or `create_parameter("TextureObject", ...)`
2. Setting the `ParameterName` property
3. Setting the default `Texture` asset reference
4. Connecting to downstream nodes

...has never been tested against a real complex material. If `batch_create("TextureObjectParameter", ...)` creates a raw unparameterized node, properties may need manual setting. If `create_parameter("TextureObject", ...)` is used instead, the batch workflow breaks (individual calls needed).

**Fix:** Verify which approach works and document it. If `batch_create` works for TextureObjectParameter but doesn't set parameter name/texture, `batch_set_properties` must handle it afterward.

---

## ISSUE-6: StaticSwitchParameter may not recreate correctly via batch_create

**Status:** ⏳ PENDING TEST — Requires e2e verification with recreation test  
**Severity:** P1 — May block 10 StaticSwitchParameter nodes  
**Component:** `MaterialNodeService::BatchCreateExpressions` / `CreateParameter`

**Problem:**  
`StaticSwitchParameter` is a parameter type that controls compile-time branching. It has `True` and `False` input pins and a `Value` input. Creation via:  
- `batch_create("StaticSwitchParameter", ...)` — creates the node but may not set ParameterName
- `create_parameter("StaticSwitch", ...)` — sets name but requires individual calls

After creation (either path), `batch_set_properties` must set `ParameterName`, `DefaultValue`, and `Group`. Need to verify the property names work with `set_expression_property`.

**Fix:** Test both paths and document the working approach.

---

## ISSUE-7: LandscapeLayerBlend can't be created via batch_create with layers

**Status:** ✅ RESOLVED — Added `CreateLayerBlendNodeWithLayers` to `ULandscapeMaterialService`  
**Severity:** P1 — Blocks layer blend auto-creation  
**Component:** `LandscapeMaterialService` / `MaterialNodeService`

**Problem:**  
`batch_create("LandscapeLayerBlend", ...)` creates an empty blend node with no layers. Layers must be added one-by-one via `LandscapeMaterialService::AddLayerToBlendNode`. For the full recreation workflow:

1. `batch_create` creates the blend node (gets new ID)
2. Get blend node ID from batch result
3. Call `add_layer_to_blend_node` for each layer
4. Call `connect_to_layer_input` for each layer's diffuse input

This means the blend node setup requires at minimum `N+1` calls (1 create + N layer adds), plus connection calls. Not a bug, but a performance concern for materials with many layers.

**Enhancement (nice-to-have):** Add `CreateLayerBlendNodeWithLayers` that takes layer configs upfront:
```cpp
static FLandscapeLayerBlendInfo CreateLayerBlendNodeWithLayers(
    const FString& MaterialPath,
    const TArray<FLandscapeMaterialLayerConfig>& Layers,
    int32 PosX, int32 PosY);
```

---

## ISSUE-8: Export doesn't resolve output names for LandscapeLayerBlend per-layer outputs

**Status:** ✅ RESOLVED — Added `is_layer_blend_input`, `layer_input_type`, `layer_name` flags to connections  
**Severity:** P1 — May cause wrong connections to blend node  
**Component:** `MaterialNodeService::ExportMaterialGraph`

**Problem:**  
`LandscapeLayerBlend` has dynamically-named outputs based on its layers (e.g., output 0 is the blended result, but connections into layer inputs have special indexing). The export captures connection `target_input` as the input name, but for blend nodes, layer inputs may be named like `"Layer Grass"` or use index-based naming.

The `connect_to_layer_input` API uses layer name + input type ("Layer" or "Height"), while generic `connect_expressions` uses standard input names. If the export's `target_input` contains the layer-specific format, `connect_expressions` may fail — and `connect_to_layer_input` must be used instead.

**Fix:** In the export, detect LandscapeLayerBlend target nodes and emit a structured `layer_connections` array:
```json
{
  "source_id": "temp_50",
  "blend_node_id": "temp_1",
  "layer_name": "Grass",
  "input_type": "Layer"
}
```

---

## ISSUE-9: LandscapeGrassOutput grass type asset references

**Status:** ✅ RESOLVED — Added `grass_types` array with name and asset path to export  
**Severity:** P2 — Blocks grass output recreation  
**Component:** `LandscapeMaterialService::CreateGrassOutput`

**Problem:**  
`CreateGrassOutput` takes `TMap<FString, FString>& GrassTypeNames` where key=display name, value=grass type asset path. The export captures this via generic property export, but the `GrassTypes` property (`TArray<FGrassInput>`) is a complex struct array that may not export/import cleanly via `ImportText_Direct`.

**Fix:** Add explicit grass type data to the export:
```cpp
if (UMaterialExpressionLandscapeGrassOutput* GrassOut = Cast<UMaterialExpressionLandscapeGrassOutput>(Expr))
{
    TSharedRef<FJsonObject> GrassObj = MakeShared<FJsonObject>();
    for (const FGrassInput& GI : GrassOut->GrassTypes)
    {
        if (GI.GrassType)
            GrassObj->SetStringField(GI.Name.ToString(), GI.GrassType->GetPathName());
    }
    ExprObj->SetObjectField("grass_types", GrassObj);
}
```

---

## ISSUE-10: No diff/compare API for two material graphs

**Status:** ✅ RESOLVED — Added `CompareMaterialGraphs` method to `UMaterialNodeService`  
**Severity:** P2 — Verification is manual  
**Component:** `MaterialNodeService`

**Problem:**  
Phase 8 of the recreation test requires comparing M_Landscape vs M_Landscape2 in detail. Currently the consumer must:
1. Export both as JSON
2. Parse both in the AI's context
3. Manually compare expression counts, classes, connections, properties

For a 782-expression material, this is a LOT of data to hold in context.

**Enhancement:** Add `CompareMaterialGraphs`:
```cpp
/**
 * Compare two material graphs and report differences.
 * Maps to action="compare_graphs"
 *
 * @param MaterialPathA - First material path
 * @param MaterialPathB - Second material path
 * @return JSON string with comparison results
 */
UFUNCTION(BlueprintCallable, Category = "MaterialNode")
static FString CompareMaterialGraphs(
    const FString& MaterialPathA,
    const FString& MaterialPathB);
```

Return format:
```json
{
  "match": false,
  "expression_count_a": 782,
  "expression_count_b": 780,
  "missing_in_b": ["CustomExpression at (-500,200)"],
  "extra_in_b": [],
  "connection_count_a": 623,
  "connection_count_b": 621,
  "property_differences": [...],
  "output_differences": [...]
}
```

---

## ISSUE-11: Reroute nodes may lose connection context during recreation

**Status:** ✅ RESOLVED — Documented in test prompt Phase 5 (topological connection ordering)  
**Severity:** P2 — May cause minor graph layout issues  
**Component:** `MaterialNodeService`

**Problem:**  
M_Landscape has 66 Reroute nodes. These are pass-through nodes purely for graph organization. They work correctly with `batch_create` and `connect_expressions`. However, reroute nodes have a single input and single output with auto-typed pins. If connections are made in the wrong order (output connected before input), the type inference may differ from the original.

**Mitigation:** Ensure connections are created in topological order (upstream before downstream). This is a consumer-side concern, not a tool bug, but worth documenting.

---

## ISSUE-12: Export JSON size for 782-expression material

**Status:** ✅ RESOLVED — Added `ExportMaterialGraphSummary` for lightweight verification  
**Severity:** P2 — May exceed AI context limits  
**Component:** `MaterialNodeService::ExportMaterialGraph`

**Problem:**  
A material with 782 expressions, each with ~5-10 properties, plus 600+ connections, could produce a JSON export of 200-500KB. This may:
- Exceed the `FString` comfortable handling size (unlikely but worth checking)
- Exceed the AI agent's context window when processing

**Fix Option A:** Add a `ExportMaterialGraphSummary` that exports counts and class frequencies without full property data.

**Fix Option B:** Add chunked export: `ExportMaterialGraphChunked(MaterialPath, StartIndex, Count)` that exports a subset of expressions.

**Fix Option C:** Return compressed JSON or use abbreviated property names.

---

## Priority Summary

| Issue | Severity | Blocks | Fix Effort | Status |
|-------|----------|--------|------------|--------|
| ISSUE-1: Output index→name in export | P0 | All connections | Small (add field to export) | ✅ RESOLVED — `source_output_name` added to export connections |
| ISSUE-2: LayerBlend layers in export | P0 | Landscape blend | Small (add to export) | ✅ RESOLVED — `landscape_layers` array added to export |
| ISSUE-3: Batch create specialized | P1 | Performance only | Medium (new struct + method) | ✅ RESOLVED — `BatchCreateSpecialized` + `FBatchCreateDescriptor` added |
| ISSUE-4: Custom input names in export | P1 | Custom HLSL inputs | Small (add field to export) | ✅ RESOLVED — `custom_input_names` + `custom_additional_outputs` added |
| ISSUE-5: TextureObjectParameter e2e | P1 | 14 nodes | Test only | ⏳ PENDING TEST — requires e2e verification |
| ISSUE-6: StaticSwitchParameter e2e | P1 | 10 nodes | Test only | ⏳ PENDING TEST — requires e2e verification |
| ISSUE-7: LayerBlend batch create | P1 | Performance only | Medium | ✅ RESOLVED — `CreateLayerBlendNodeWithLayers` added to LandscapeMaterialService |
| ISSUE-8: LayerBlend connection format | P1 | Blend connections | Small (structured export) | ✅ RESOLVED — `is_layer_blend_input`, `layer_input_type`, `layer_name` added |
| ISSUE-9: GrassOutput asset refs | P2 | 1 node | Small (add to export) | ✅ RESOLVED — `grass_types` array added to export |
| ISSUE-10: Compare API | P2 | Verification | Medium | ✅ RESOLVED — `CompareMaterialGraphs` method added |
| ISSUE-11: Reroute ordering | P2 | Graph cosmetics | Documentation only | ✅ RESOLVED — documented in test prompt (Phase 5 ordering) |
| ISSUE-12: Export JSON size | P2 | Context limits | Medium | ✅ RESOLVED — `ExportMaterialGraphSummary` method added |

### Implementation Details

**Files Modified:**
- `UMaterialNodeService.h` — Added `FBatchCreateDescriptor` struct, `BatchCreateSpecialized`, `ExportMaterialGraphSummary`, `CompareMaterialGraphs` declarations
- `UMaterialNodeService.cpp` — Added `source_output_name` to connections, `landscape_layers` for blend nodes, `custom_input_names`/`custom_additional_outputs` for Custom nodes, `is_layer_blend_input`/`layer_input_type`/`layer_name` for blend connections, `grass_types` for grass output nodes. Implemented `BatchCreateSpecialized`, `ExportMaterialGraphSummary`, `CompareMaterialGraphs`.
- `ULandscapeMaterialService.h` — Added `CreateLayerBlendNodeWithLayers` declaration, updated action count to 17
- `ULandscapeMaterialService.cpp` — Implemented `CreateLayerBlendNodeWithLayers`

**New APIs (4):**
1. `MaterialNodeService.batch_create_specialized(material_path, descriptors)` — Create all node types in one transaction
2. `MaterialNodeService.export_material_graph_summary(material_path)` — Lightweight graph summary
3. `MaterialNodeService.compare_material_graphs(path_a, path_b)` — Structural diff between materials
4. `LandscapeMaterialService.create_layer_blend_node_with_layers(material_path, layers, pos_x, pos_y)` — One-shot blend node creation

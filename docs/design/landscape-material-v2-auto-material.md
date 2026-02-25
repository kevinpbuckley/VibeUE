# Landscape Material v2 — Auto-Material & Intelligent Painting

## Motivation

### The Problem

An AI agent was asked to paint a landscape that had been sculpted from mesh projection. After multiple attempts, the landscape remained visually flat — a single green color across the entire terrain. The root causes reveal systemic gaps in both the API surface and the AI skill/workflow guidance.

#### Failure Timeline

1. **Attempt 1 — Manual `MaterialNodeService` Graph Building:** The AI manually created `LandscapeLayerBlend`, `TextureSample`, and `LandscapeLayerCoords` nodes using the generic `MaterialNodeService`. It wired textures for two layers but accidentally used the **same texture** (`T_Background_Landscape_01_Albedo`) for both "Background" and "Base" layers. Result: both layers rendered identically — monochrome green.

2. **Attempt 2 — Per-Vertex Weight Painting:** The AI computed 4M slope/height values, derived per-vertex weights, and called `set_weights_in_region`. The weights were correct but invisible — both layers had the same texture, so painting had no visual effect.

3. **Attempt 3 — Material Rebuild:** The AI deleted and recreated the material with different textures (`T_Grass_01_Albedo` for Base, `T_Slope_01_Albedo` for Background). This required full manual graph construction (~15 API calls), material compilation, layer re-creation, and full weight repainting. Result: finally worked, but required 6+ round trips and the agent reinventing functionality that already existed.

### Root Causes

| Gap | Severity | Description |
|-----|----------|-------------|
| **AI didn't discover `LandscapeMaterialService`** | Critical | The `landscape` skill doesn't mention `LandscapeMaterialService`. The AI loaded the `landscape` skill (sculpting/painting) but never loaded `landscape-materials` (material graph construction). The skill system didn't cross-reference. |
| **No `setup_height_slope_blend` usage** | Critical | This API already exists and does exactly what was needed — auto-blend layers by height and slope in the material shader itself, eliminating per-vertex weight painting entirely. But the AI never discovered it because the wrong skill was loaded. |
| **No texture discovery/recommendation** | High | The AI had to manually search for textures with `list_assets` and pick by name. No API suggests appropriate textures for terrain types (grass, rock, snow, mud). |
| **No one-call auto-material** | High | Creating a landscape material with proper layers, textures, and height/slope blending requires 10+ API calls across 3 services. There should be a single "create a terrain material with these layers" call. |
| **`setup_layer_textures` not wired to height/slope** | Medium | `setup_layer_textures` creates texture nodes and connects them to blend inputs, but doesn't set up auto-blending (height/slope masks). Two separate workflows that should be combined. |
| **No landscape texture search** | Medium | No way to search for textures tagged/suitable for landscape use. The AI resorted to path-based guessing. |
| **Skill cross-referencing missing** | Medium | The `landscape` skill (sculpting) should reference `landscape-materials` skill when material work is needed. Currently they're isolated. |
| **Layer re-creation after material change** | Low | When a material is deleted and recreated, layers are lost. The assignment workflow doesn't detect or warn about this. |

### User Expectation

> *"I see the mountains but they are not painted"*
> *"it still is just all green the material may be wrong"*
> *"do we need to update our api and skills to better handle the landscape material? we want to be able to paint it similar to the mesh with an auto material?"*

The user expects: **one operation** that creates a terrain that looks like terrain — grass on flat areas, rock on slopes, optionally snow on peaks — without manually building material graphs or painting 4M vertices.

---

## Current State

### Existing APIs

#### LandscapeMaterialService (20 actions) — Already Implemented

| Category | Actions | What It Does |
|----------|---------|------------|
| Material Creation | 1 | `create_landscape_material` — creates a blank landscape material |
| Layer Blend Node | 6 | Create/manage `LandscapeLayerBlend` nodes with layers |
| Coordinates | 1 | `create_layer_coords_node` — UV tiling |
| Layer Sample | 1 | `create_layer_sample_node` — sample layer weight |
| Grass Output | 1 | `create_grass_output` — procedural foliage |
| Layer Info | 2 | Create/inspect `ULandscapeLayerInfoObject` assets |
| Assignment | 1 | `assign_material_to_landscape` — assign material + layer mapping |
| Convenience | 1 | `setup_layer_textures` — complete layer texture setup |
| Weight Node | 1 | `create_layer_weight_node` — alternative blending |
| Height/Slope | 3 | `create_height_mask`, `create_slope_mask`, `setup_height_slope_blend` |
| Existence | 2 | Check material/layer-info existence |

#### LandscapeService Painting (from 64 actions)

| Category | Actions | What It Does |
|----------|---------|------------|
| Layer Management | 5 | `list_layers`, `add_layer`, `remove_layer`, `get_layer_weights_at_location`, `paint_layer_at_location` |
| Region Painting | 2 | `paint_layer_in_region`, `paint_layer_in_world_rect` |
| Weight Maps | 4 | `export/import_weight_map`, `get/set_weights_in_region` |
| Terrain Analysis | 5 | `analyze_terrain`, `get_slope_map`, `get_slope_at_location`, etc. |

### What Already Works (But Wasn't Used)

The `setup_height_slope_blend` action already does exactly what was needed:

```python
# This ALREADY EXISTS but the AI never found it
unreal.LandscapeMaterialService.setup_height_slope_blend(
    mat_path, blend_node_id,
    base_layer_name="Grass",
    height_layer_name="Snow", height_threshold=8000.0, height_blend=2000.0,
    slope_layer_name="Rock", slope_threshold=35.0, slope_blend=10.0
)
```

This creates material-shader-based blending that requires **zero weight painting** — the GPU handles it automatically based on world position and surface normal. But the AI never discovered it because:
1. It loaded the `landscape` skill, not `landscape-materials`
2. The `landscape` skill doesn't cross-reference `LandscapeMaterialService`
3. The skill suggestion system ranked `landscape-materials` at 44 relevance, but the AI had already loaded `landscape` and proceeded

---

## Proposed Enhancements

### Feature 1: `create_auto_material` — One-Call Landscape Material

**Priority: Critical**
**Estimated effort: 2–3 days**
**Location: `ULandscapeMaterialService`**

A single API call that creates a complete landscape material with layers, textures, height/slope-driven blending, layer info objects, and assigns it to a landscape. This is the "auto-material" equivalent for landscapes.

#### API Signature

```cpp
/**
 * Create a complete auto-material for a landscape in one call.
 * Maps to action="create_auto_material"
 *
 * Creates a landscape material with:
 * - LandscapeLayerBlend node with all specified layers
 * - Texture samplers for each layer (diffuse + optional normal)
 * - LandscapeLayerCoords for UV tiling
 * - Optional height/slope-driven alpha blending (auto-paint)
 * - Layer info objects for each layer
 * - Assigns material to landscape with layer mapping
 * - Compiles and saves everything
 *
 * When auto-blend is enabled, no weight-map painting is required — the
 * material shader handles blending based on world height and slope angle.
 *
 * @param LandscapeNameOrLabel - Landscape to apply the material to
 * @param MaterialName - Name for the new material asset
 * @param MaterialPath - Directory to create the material in
 * @param LayerConfigs - Array of layer configurations
 * @param bAutoBlend - If true, wire height/slope masks for automatic blending
 * @param HeightThreshold - World Z where height layer begins (if auto-blend)
 * @param HeightBlend - Transition width for height blending
 * @param SlopeThreshold - Slope angle where slope layer begins (degrees)
 * @param SlopeBlend - Transition width for slope blending (degrees)
 * @return Result with material path and any errors
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
static FLandscapeAutoMaterialResult CreateAutoMaterial(
    const FString& LandscapeNameOrLabel,
    const FString& MaterialName,
    const FString& MaterialPath,
    const TArray<FLandscapeAutoLayerConfig>& LayerConfigs,
    bool bAutoBlend = true,
    float HeightThreshold = 5000.0f,
    float HeightBlend = 1000.0f,
    float SlopeThreshold = 35.0f,
    float SlopeBlend = 10.0f);
```

#### New Structs

```cpp
USTRUCT(BlueprintType)
struct FLandscapeAutoLayerConfig
{
    GENERATED_BODY()

    /** Layer name (e.g., "Grass", "Rock", "Snow") */
    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    FString LayerName;

    /** Path to diffuse/albedo texture asset */
    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    FString DiffuseTexturePath;

    /** Optional: Path to normal map texture */
    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    FString NormalTexturePath;

    /** Optional: Path to roughness texture */
    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    FString RoughnessTexturePath;

    /** UV tiling scale (default 0.01 = tile every 100 units) */
    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    float TilingScale = 0.01f;

    /** Role determines auto-blend behavior:
     *  "base"   — Default layer, shown on low/flat terrain
     *  "slope"  — Shown on steep terrain (driven by slope mask)
     *  "height" — Shown at high elevation (driven by height mask)
     *  "paint"  — Manual paint only (no auto-blending)
     */
    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    FString Role = TEXT("paint");
};

USTRUCT(BlueprintType)
struct FLandscapeAutoMaterialResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    FString MaterialAssetPath;

    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    TArray<FString> LayerInfoPaths;

    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    FString BlendNodeId;

    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    FString ErrorMessage;
};
```

#### Usage Example

```python
import unreal

# One call — creates material, layers, textures, auto-blend, assigns to landscape
result = unreal.LandscapeMaterialService.create_auto_material(
    "TerrainBase",                     # landscape name
    "M_TerrainAuto",                   # material name
    "/Game/Materials",                 # material directory
    [
        # Base layer (flat ground) — grass
        unreal.LandscapeAutoLayerConfig(
            layer_name="Grass",
            diffuse_texture_path="/Game/Stylized_Spruce_Forest/Textures/Landscape/Grass_01/T_Grass_01_Albedo",
            normal_texture_path="/Game/Stylized_Spruce_Forest/Textures/Landscape/Grass_01/T_Grass_01_Normal",
            tiling_scale=0.01,
            role="base"
        ),
        # Slope layer (steep faces) — rock/slope texture
        unreal.LandscapeAutoLayerConfig(
            layer_name="Rock",
            diffuse_texture_path="/Game/Stylized_Spruce_Forest/Textures/Landscape/Slope/T_Slope_01_Albedo",
            normal_texture_path="/Game/Stylized_Spruce_Forest/Textures/Landscape/Slope/T_Slope_01_Normal",
            tiling_scale=0.01,
            role="slope"
        ),
        # Height layer (peaks) — snow
        unreal.LandscapeAutoLayerConfig(
            layer_name="Snow",
            diffuse_texture_path="/Game/Stylized_Spruce_Forest/Textures/Landscape/Snow/T_Snow_Albedo",
            normal_texture_path="/Game/Stylized_Spruce_Forest/Textures/Landscape/Snow/T_Snow_Normal",
            tiling_scale=0.01,
            role="height"
        ),
    ],
    True,           # auto-blend enabled
    8000.0, 2000.0, # height threshold + blend
    30.0, 15.0      # slope threshold + blend (degrees)
)

print(f"Material: {result.material_asset_path}")
# Done! Landscape now renders with auto-blended grass/rock/snow - zero painting needed
```

#### Implementation Notes

Internally, `CreateAutoMaterial` would:
1. Call `CreateLandscapeMaterial` to create the base material
2. Call `CreateLayerBlendNodeWithLayers` to add all layers in one transaction
3. For each layer, call `SetupLayerTextures` to wire diffuse/normal/roughness
4. Call `MaterialNodeService::ConnectToOutput` for BaseColor (and Normal if any layer has normals)
5. If `bAutoBlend`, call `SetupHeightSlopeBlend` to wire height/slope masks for the appropriate layers
6. Call `MaterialService::CompileMaterial`
7. For each layer, call `CreateLayerInfoObject`
8. Call `AssignMaterialToLandscape` with all layer info paths
9. Save all created assets

This replaces the current 10-15 manual API calls with one.

---

### Feature 2: `find_landscape_textures` — Texture Discovery

**Priority: High**
**Estimated effort: 1–2 days**
**Location: `ULandscapeMaterialService`**

Search the project for textures suitable for landscape layers, optionally filtered by terrain type.

#### API Signature

```cpp
/**
 * Search the project for textures suitable for landscape layers.
 * Maps to action="find_landscape_textures"
 *
 * Searches content directories for textures matching common landscape naming
 * conventions (Albedo/Diffuse/BaseColor/Normal, in Landscape/ or Terrain/ dirs).
 * Can filter by terrain type keyword.
 *
 * @param SearchPath - Root content path to search (empty = "/Game/")
 * @param TerrainType - Optional filter: "grass", "rock", "snow", "mud", "sand", etc
 * @param bIncludeNormals - Also return matching normal maps
 * @return Array of texture set results
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|LandscapeMaterial")
static TArray<FLandscapeTextureSet> FindLandscapeTextures(
    const FString& SearchPath = TEXT(""),
    const FString& TerrainType = TEXT(""),
    bool bIncludeNormals = true);
```

#### New Struct

```cpp
USTRUCT(BlueprintType)
struct FLandscapeTextureSet
{
    GENERATED_BODY()

    /** Terrain type inferred from path/name (e.g., "Grass", "Rock", "Snow") */
    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    FString TerrainType;

    /** Path to albedo/diffuse texture */
    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    FString AlbedoPath;

    /** Path to normal map (empty if not found) */
    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    FString NormalPath;

    /** Path to roughness map (empty if not found) */
    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    FString RoughnessPath;

    /** Resolution of the albedo texture */
    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    int32 TextureWidth = 0;

    UPROPERTY(BlueprintReadWrite, Category = "LandscapeMaterial")
    int32 TextureHeight = 0;
};
```

#### Usage Example

```python
import unreal

# Find all grass textures in the project
grass = unreal.LandscapeMaterialService.find_landscape_textures("", "grass")
for t in grass:
    print(f"{t.terrain_type}: {t.albedo_path} (Normal: {t.normal_path})")

# Find all landscape textures
all_tex = unreal.LandscapeMaterialService.find_landscape_textures()
# Returns: [{terrain_type: "Grass", albedo_path: "/.../T_Grass_01_Albedo", ...}, ...]
```

#### Implementation Notes

Texture discovery heuristics:
1. Search directories containing "Landscape", "Terrain", "Ground", "Environment"
2. Match albedo by suffix: `_Albedo`, `_Diffuse`, `_BaseColor`, `_D`, `_Color`
3. Match normal by suffix: `_Normal`, `_N`, `_NormalMap`
4. Match roughness by suffix: `_Roughness`, `_R`, `_Rough`
5. Infer terrain type from path components: `Grass`, `Rock`, `Snow`, `Mud`, `Sand`, `Slope`, `Ice`, `Needles`, `Pebbles`
6. Group albedo+normal+roughness into `FLandscapeTextureSet` entries

---

### Feature 3: `auto_paint_from_terrain` — Analyze-and-Paint

**Priority: Medium**
**Estimated effort: 1–2 days**
**Location: `ULandscapeService`**

For cases where material-shader auto-blending isn't desired (e.g., more artistic control, or the material uses `LB_WeightBlend`), provide a C++ action that reads the heightmap and slope map, computes per-vertex layer weights based on configurable rules, and writes them — all in one C++ call.

Currently, this requires:
1. Python call: `get_slope_map` → 4M floats returned to Python
2. Python call: `get_height_in_region` → 4M floats returned to Python
3. Python loop: compute 4M weights (slow in Python)
4. Python call: `set_weights_in_region("Background", ...)` → 4M floats sent to C++
5. Python call: `set_weights_in_region("Base", ...)` → 4M floats sent to C++

This transfers ~80MB of data across the Python↔C++ boundary and runs a 4M-iteration loop in Python.

#### API Signature

```cpp
/**
 * Auto-paint landscape layers based on terrain height and slope analysis.
 * Maps to action="auto_paint_from_terrain"
 *
 * Analyzes the landscape heightmap and slope data entirely in C++, then
 * computes and writes per-vertex layer weights based on configurable
 * height/slope thresholds. No data crosses the Python↔C++ boundary.
 *
 * @param LandscapeNameOrLabel - Landscape to paint
 * @param PaintRules - Array of painting rules (one per layer)
 * @return Result with stats about what was painted
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
static FAutoPaintResult AutoPaintFromTerrain(
    const FString& LandscapeNameOrLabel,
    const TArray<FAutoPaintRule>& PaintRules);
```

#### New Structs

```cpp
USTRUCT(BlueprintType)
struct FAutoPaintRule
{
    GENERATED_BODY()

    /** Layer name to paint */
    UPROPERTY(BlueprintReadWrite, Category = "Landscape")
    FString LayerName;

    /** Minimum slope (degrees) where this layer starts appearing (0 = flat) */
    UPROPERTY(BlueprintReadWrite, Category = "Landscape")
    float MinSlopeDegrees = 0.0f;

    /** Maximum slope (degrees) where this layer is fully visible */
    UPROPERTY(BlueprintReadWrite, Category = "Landscape")
    float MaxSlopeDegrees = 90.0f;

    /** Minimum height (world Z) where this layer starts appearing */
    UPROPERTY(BlueprintReadWrite, Category = "Landscape")
    float MinHeight = -1000000.0f;

    /** Maximum height (world Z) where this layer is fully visible */
    UPROPERTY(BlueprintReadWrite, Category = "Landscape")
    float MaxHeight = 1000000.0f;

    /** Blending mode: "slope", "height", "both", "base"
     *  - "base": fills everywhere, lowest priority
     *  - "slope": driven by slope thresholds
     *  - "height": driven by height thresholds
     *  - "both": must satisfy both slope AND height thresholds
     */
    UPROPERTY(BlueprintReadWrite, Category = "Landscape")
    FString Mode = TEXT("base");

    /** Priority (higher = painted on top when overlapping) */
    UPROPERTY(BlueprintReadWrite, Category = "Landscape")
    int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct FAutoPaintResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Landscape")
    bool bSuccess = false;

    /** Per-layer vertex counts */
    UPROPERTY(BlueprintReadWrite, Category = "Landscape")
    TMap<FString, int32> VerticesPainted;

    /** Total vertices processed */
    UPROPERTY(BlueprintReadWrite, Category = "Landscape")
    int32 TotalVertices = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Landscape")
    FString ErrorMessage;
};
```

#### Usage Example

```python
import unreal

# Auto-paint based on terrain analysis — all in C++
result = unreal.LandscapeService.auto_paint_from_terrain(
    "TerrainBase",
    [
        unreal.AutoPaintRule(
            layer_name="Grass",
            mode="base",
            priority=0
        ),
        unreal.AutoPaintRule(
            layer_name="Rock",
            min_slope_degrees=25.0,
            max_slope_degrees=45.0,
            mode="slope",
            priority=1
        ),
        unreal.AutoPaintRule(
            layer_name="Snow",
            min_height=8000.0,
            max_height=12000.0,
            mode="height",
            priority=2
        ),
    ]
)

print(f"Painted {result.total_vertices} vertices")
for layer, count in result.vertices_painted.items():
    print(f"  {layer}: {count}")
```

---

### Feature 4: Skill System Cross-Referencing

**Priority: Critical**
**Estimated effort: 0.5 days (content changes only)**

#### Problem

The `landscape` skill (sculpting) mentions `LandscapeService` but not `LandscapeMaterialService`. When an agent loads the `landscape` skill for terrain work, it never discovers the material-specific APIs that handle texture setup, layer blending, and auto-material creation.

#### Changes Required

##### 4a. Update `landscape` skill to reference `landscape-materials`

Add to `skills/landscape/skill.md`:

```markdown
### Related Skills
- **landscape-materials**: For material creation, texture setup, layer blending, and auto-material.
  Load this skill when you need to create or modify the material applied to a landscape.
  
  Example: `manage_skills(action="load", skill_name="landscape-materials")`

### When to Load Both Skills
- Creating a new landscape with textures → load BOTH `landscape` + `landscape-materials`
- Sculpting existing terrain → `landscape` only
- Changing texture appearance → `landscape-materials` only
- Full workflow (create + sculpt + texture) → load BOTH
```

##### 4b. Update `landscape-materials` skill with auto-material workflow

Once Feature 1 is implemented, add the `create_auto_material` workflow as the **primary recommended approach**:

```markdown
### ⚠️ Preferred Approach: Auto-Material

For most landscape material tasks, use `create_auto_material` instead of building
the graph manually. It handles material creation, texture wiring, auto-blending,
layer info objects, and landscape assignment in one call.

Only use the individual APIs when you need fine-grained control over the material 
graph (e.g., custom HLSL, non-standard blending, procedural patterns).
```

##### 4c. Add auto-load suggestion to `LandscapeService` docstring

When `LandscapeService` is discovered and material-related operations are needed, the system should hint at loading the `landscape-materials` skill. Add to the `LandscapeService` docstring:

```
NOTE: For landscape material, texture, and layer blending operations,
use LandscapeMaterialService (load the "landscape-materials" skill).
```

---

### Feature 5: `create_landscape_with_auto_material` — Full End-to-End

**Priority: Medium**
**Estimated effort: 1 day**
**Location: `ULandscapeService`**

The ultimate convenience API: create a landscape, optionally project meshes onto it, and apply an auto-material with terrain-aware blending — all in one call.

#### API Signature

```cpp
/**
 * Create a landscape from mesh actors with auto-material.
 * Maps to action="create_landscape_from_meshes"
 *
 * Complete workflow in one call:
 * 1. Creates a landscape covering the bounding box of specified mesh actors
 * 2. Projects each mesh actor's geometry onto the landscape
 * 3. Creates an auto-material with terrain-aware layer blending
 * 4. Assigns the material to the landscape
 * 5. Optionally hides the source mesh actors
 *
 * @param MeshActorLabels - Names/labels of mesh actors to project
 * @param LandscapeName - Label for the new landscape
 * @param LayerConfigs - Auto-material layer configurations
 * @param bAutoBlend - Enable height/slope-driven blending
 * @param bHideSourceMeshes - Hide the source mesh actors after projection
 * @param PaddingPercent - Extra padding around mesh bounding box (0.1 = 10%)
 * @param HeightThreshold - For auto-blend
 * @param HeightBlend - For auto-blend
 * @param SlopeThreshold - For auto-blend (degrees)
 * @param SlopeBlend - For auto-blend (degrees)
 * @return Comprehensive result
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
static FLandscapeFromMeshesResult CreateLandscapeFromMeshes(
    const TArray<FString>& MeshActorLabels,
    const FString& LandscapeName,
    const TArray<FLandscapeAutoLayerConfig>& LayerConfigs,
    bool bAutoBlend = true,
    bool bHideSourceMeshes = false,
    float PaddingPercent = 0.15f,
    float HeightThreshold = 5000.0f,
    float HeightBlend = 1000.0f,
    float SlopeThreshold = 35.0f,
    float SlopeBlend = 10.0f);
```

#### Usage

```python
import unreal

# Find landscape textures automatically
textures = unreal.LandscapeMaterialService.find_landscape_textures()
grass = next(t for t in textures if t.terrain_type == "Grass")
rock = next(t for t in textures if t.terrain_type == "Slope")
snow = next(t for t in textures if t.terrain_type == "Snow")

# One call: create landscape + project meshes + auto-material
result = unreal.LandscapeService.create_landscape_from_meshes(
    ["SM_STZD_Background_Landscape_40", "SM_STZD_Background_Landscape_35"],
    "TerrainBase",
    [
        unreal.LandscapeAutoLayerConfig(
            layer_name="Grass", role="base",
            diffuse_texture_path=grass.albedo_path,
            normal_texture_path=grass.normal_path),
        unreal.LandscapeAutoLayerConfig(
            layer_name="Rock", role="slope",
            diffuse_texture_path=rock.albedo_path,
            normal_texture_path=rock.normal_path),
        unreal.LandscapeAutoLayerConfig(
            layer_name="Snow", role="height",
            diffuse_texture_path=snow.albedo_path,
            normal_texture_path=snow.normal_path),
    ],
    b_auto_blend=True,
    b_hide_source_meshes=True,
    slope_threshold=30.0,
    height_threshold=8000.0
)

# Done! Landscape created, sculpted to match meshes, materials applied automatically
```

---

## Implementation Priority

| # | Feature | Priority | Effort | Impact |
|---|---------|----------|--------|--------|
| 4 | Skill cross-referencing | Critical | 0.5 day | Fixes the root cause — AI discovers existing APIs |
| 1 | `create_auto_material` | Critical | 2–3 days | Eliminates 10+ manual API calls |
| 2 | `find_landscape_textures` | High | 1–2 days | Eliminates texture guessing |
| 3 | `auto_paint_from_terrain` | Medium | 1–2 days | Faster C++-native weight painting |
| 5 | `create_landscape_from_meshes` | Medium | 1 day | Ultimate convenience (depends on 1+2) |

**Recommended implementation order: 4 → 1 → 2 → 3 → 5**

Feature 4 (skill cross-referencing) should be done **immediately** — it's a markdown-only change that makes the existing `setup_height_slope_blend` discoverable. This alone would have prevented the original failure.

---

## Comparison: Current vs. Proposed Workflow

### Current (10+ calls, 3 services, manual graph construction)

```
1. LandscapeMaterialService.create_landscape_material(...)
2. LandscapeMaterialService.create_layer_blend_node(...)
3. LandscapeMaterialService.add_layer_to_blend_node(...) × N layers
4. LandscapeMaterialService.setup_layer_textures(...) × N layers
5. MaterialNodeService.connect_to_output(..., "BaseColor")
6. MaterialNodeService.connect_to_output(..., "Normal")  [if normals]
7. MaterialService.compile_material(...)                  [SLOW]
8. LandscapeMaterialService.create_layer_info_object(...) × N layers
9. LandscapeMaterialService.assign_material_to_landscape(...)
10. LandscapeService.get_slope_map(...)                   [4M floats → Python]
11. LandscapeService.get_height_in_region(...)            [4M floats → Python]
12. [Python loop over 4M vertices to compute weights]
13. LandscapeService.set_weights_in_region(...) × N layers [4M floats → C++]
```

**Total: 10-15 API calls, ~80MB data transfer, Python loop over 4M elements**

### Proposed Auto-Material (2 calls with auto-blend, 3 calls with weight painting)

#### Option A: Shader-based auto-blend (zero painting)
```
1. LandscapeMaterialService.find_landscape_textures(...)   [optional]
2. LandscapeMaterialService.create_auto_material(...)      [everything in one call]
```

#### Option B: Weight-map painting (artistic control)
```
1. LandscapeMaterialService.find_landscape_textures(...)   [optional]
2. LandscapeMaterialService.create_auto_material(..., bAutoBlend=False)
3. LandscapeService.auto_paint_from_terrain(...)           [all in C++]
```

#### Option C: Full end-to-end (landscape + meshes + material)
```
1. LandscapeMaterialService.find_landscape_textures(...)
2. LandscapeService.create_landscape_from_meshes(...)      [everything]
```

---

## Appendix: Texture Search Heuristics

For `find_landscape_textures`, the following naming patterns should be searched:

### Directory Patterns (case-insensitive)
- `*/Landscape/*`
- `*/Terrain/*`
- `*/Ground/*`
- `*/Environment/Terrain/*`
- `*/Background/*` (common in asset packs)

### Albedo Suffixes
- `_Albedo`, `_Diffuse`, `_BaseColor`, `_Color`, `_D`, `_BC`

### Normal Suffixes  
- `_Normal`, `_N`, `_NormalMap`, `_Norm`

### Roughness Suffixes
- `_Roughness`, `_R`, `_Rough`, `_ORM` (packed), `_ARM`

### Terrain Type Inference (from path components)
| Path Keyword | Terrain Type |
|-------------|-------------|
| `Grass`, `Lawn`, `Meadow` | Grass |
| `Rock`, `Stone`, `Cliff`, `Slope` | Rock |
| `Snow`, `Ice`, `Frost` | Snow |
| `Mud`, `Dirt`, `Soil`, `Earth` | Dirt |
| `Sand`, `Desert`, `Beach` | Sand |
| `Gravel`, `Pebbles`, `Peebles` | Gravel |
| `Road`, `Path`, `Trail` | Road |
| `Water`, `River`, `Lake` | Water |
| `Needle`, `Forest`, `Pine` | Forest Floor |

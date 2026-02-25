# Landscape Auto-Material Skill — Design Plan

## Executive Summary

The Real_Landscape content pack uses a **production-quality material architecture** fundamentally different from VibeUE's current landscape-materials approach. Instead of manually wiring `LandscapeLayerBlend` nodes + texture samplers, Real_Landscape uses:

1. **A Master Material** (`M_Landscape_Master_01`) with exposed parameters
2. **18 Material Functions** that encapsulate blending logic (auto-layer, altitude, slope, RVT, distance fades, etc.)
3. **10 Layer Functions** (`MF_Layer_Grass`, `MF_Layer_Rock`, etc.) that encapsulate per-layer texture sampling
4. **Material Instances** (`MI_Landscape_Default_01`, etc.) that configure the master via parameters — no graph editing needed
5. **Runtime Virtual Textures** (`RVT_Default_Demo_01`) for scalable rendering
6. **Layer Info Objects** including an `Auto_Layer_LayerInfo` for automatic layering

This is a **parametric, instance-driven** approach vs. our current **manual graph-building** approach. To fully support it, we need:
- New tools for RVT management and material function introspection
- A new `landscape-auto-material` skill teaching the AI this paradigm
- Export-driven reverse engineering of the master material to document its parameter surface

---

## Phase 0: Reverse-Engineer Real_Landscape (Research — Day 1)

### Goal
Use existing `export_graph` and `export_graph_summary` tools to dump the complete structure of M_Landscape_Master_01, all 18 material functions, and the material instances. Build a complete parameter map.

### Tasks

#### 0.1 Export Master Material Graph
```python
# Use existing tool — NO code changes needed
graph_json = unreal.MaterialNodeService.export_graph(
    "/Game/Real_Landscape/Core/Materials/Masters/M_Landscape_Master_01"
)
```
This gives us:
- Every expression node (class, position, properties)
- Every function call (with `function_path` references to MF_*)  
- All connections (how functions chain together)
- Material output connections (BaseColor, Normal, Roughness, WorldPositionOffset, etc.)
- All parameters exposed (Scalar, Vector, Texture, StaticSwitch)

#### 0.2 Export Each Material Function
```python
# Material functions are also materials — export_graph works on them
for mf_name in ["MF_AutoLayer_01", "MF_Altitude_Blend_01", "MF_Slope_Blend_01", 
                 "MF_RVT_01", "MF_Distance_Blends_01", "MF_BumpOffset_WorldAlignedNormal_01",
                 "MF_Color_Correction_01", "MF_Displacement_01", ...]:
    graph = unreal.MaterialNodeService.export_graph(
        f"/Game/Real_Landscape/Core/Materials/Functions/{mf_name}"
    )
```

**Note:** `export_graph` works on `UMaterial` assets. Material Functions (`UMaterialFunction`) may need a separate export path. This is **Tool Gap #1** — see Phase 1.

#### 0.3 Export Material Instance Parameters
```python
# Use existing MaterialService to inspect instances
info = unreal.MaterialService.get_instance_info(
    "/Game/Real_Landscape/Default/Materials/Landscape/MI_Landscape_Default_01"
)
params = unreal.MaterialService.list_instance_parameters(
    "/Game/Real_Landscape/Default/Materials/Landscape/MI_Landscape_Default_01"
)
```
This tells us exactly which parameters the master material exposes and what the Default biome sets them to.

#### 0.4 Compare Biome Instances
Export MI_Landscape_Default_01, MI_Landscape_Default_02, MI_Landscape_Meadow_Island_01, and MI_Landscape_Meadow_Mountain_01 to understand how different biomes configure the same master material.

### Deliverable
A `real-landscape-architecture.md` reference document capturing:
- Complete parameter surface of M_Landscape_Master_01
- Material function dependency graph
- Per-biome instance parameter differences
- RVT configuration requirements
- Layer function input/output interfaces

---

## Phase 1: Tool Gaps & New Actions (Implementation — Days 2-5)

### Gap Analysis

| # | Gap | Severity | Current State | Needed |
|---|-----|----------|---------------|--------|
| 1 | **Material Function export** | Critical | `export_graph` works on `UMaterial` only | Support `UMaterialFunction` in `export_graph` |
| 2 | **Material Function introspection** | Critical | Can create function calls but can't list a function's inputs/outputs | New: `get_function_info(function_path)` → inputs, outputs, descriptions |
| 3 | **Material Function creation** | High | No API to create new `UMaterialFunction` assets | New: `create_material_function()` |
| 4 | **Material Function input/output management** | High | No API to add/configure function inputs/outputs | New: `add_function_input()`, `add_function_output()`, `set_function_input_type()` |
| 5 | **Runtime Virtual Texture asset creation** | High | No RVT-specific API | New: `create_rvt()`, `get_rvt_info()` |
| 6 | **RVT assignment to landscapes/volumes** | High | No API to create `RuntimeVirtualTextureVolume` actors or assign RVTs | New: `assign_rvt_to_landscape()`, `create_rvt_volume()` |
| 7 | **Material instance bulk parameter set** | Medium | `set_instance_scalar_parameter` is one-at-a-time | New: `set_instance_parameters_bulk()` for setting 20+ params at once |
| 8 | **Material property: bUsedWithVirtualTexturing** | Medium | Can set via `set_property` but AI doesn't know which properties to set | Document in skill |
| 9 | **Landscape material slot (tessellation/RVT output)** | Low | Only BaseColor/Normal/Roughness documented | Document RVT output pins |
| 10 | **Material function library browsing** | Medium | No way to list available MFs in a directory | Use `AssetManagement.list_assets` with class filter |

### 1.1 Material Function Export (Critical)

**File:** `UMaterialNodeService.h/.cpp`
**Action:** `export_function_graph`

```cpp
/**
 * Export a Material Function's node graph as JSON.
 * Maps to action="export_function_graph"
 *
 * Works like export_graph but for UMaterialFunction assets.
 * Returns all expressions, connections, function inputs/outputs,
 * and properties.
 *
 * @param FunctionPath - Asset path to the UMaterialFunction
 * @return JSON string of the complete function graph
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|MaterialNode")
static FString ExportFunctionGraph(const FString& FunctionPath);
```

**Implementation notes:**
- Load `UMaterialFunction` via `StaticLoadObject`
- Iterate `Function->FunctionExpressions` (same as `Material->Expressions`)
- Handle `UMaterialExpressionFunctionInput` and `UMaterialExpressionFunctionOutput` specially
- Reuse existing expression serialization from `ExportMaterialGraph`

### 1.2 Material Function Introspection (Critical)

**File:** `UMaterialNodeService.h/.cpp`
**Action:** `get_function_info`

```cpp
USTRUCT(BlueprintType)
struct FMaterialFunctionInfo
{
    GENERATED_BODY()
    
    FString FunctionPath;
    FString Description;
    TArray<FMaterialFunctionPinInfo> Inputs;
    TArray<FMaterialFunctionPinInfo> Outputs;
    int32 ExpressionCount;
    bool bExposeToLibrary;
};

USTRUCT(BlueprintType)
struct FMaterialFunctionPinInfo
{
    GENERATED_BODY()
    
    FString Name;
    FString Description;
    int32 InputType; // EFunctionInputType (0=Scalar, 1=Vector2, 2=Vector3, 3=Vector4, 4=Texture2D, etc.)
    FString InputTypeName; // Human-readable
    FLinearColor PreviewValue;
    bool bUsePreviewValueAsDefault;
    int32 SortPriority;
};

/**
 * Get information about a Material Function's interface (inputs/outputs).
 * Maps to action="get_function_info"
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|MaterialNode")
static FMaterialFunctionInfo GetFunctionInfo(const FString& FunctionPath);
```

### 1.3 Material Function Creation (High)

**File:** `UMaterialNodeService.h/.cpp` or new `UMaterialFunctionService.h/.cpp`
**Actions:** `create_function`, `add_function_input`, `add_function_output`

```cpp
/**
 * Create a new Material Function asset.
 * Maps to action="create_function"
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|MaterialNode")
static FMaterialCreateResult CreateMaterialFunction(
    const FString& FunctionName,
    const FString& DirectoryPath,
    const FString& Description = TEXT(""),
    bool bExposeToLibrary = false,
    const TArray<FString>& LibraryCategories = {});

/**
 * Add an input to a Material Function.
 * Maps to action="add_function_input"
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|MaterialNode")
static FString AddFunctionInput(
    const FString& FunctionPath,
    const FString& InputName,
    const FString& InputType = TEXT("Vector3"), // Scalar, Vector2, Vector3, Vector4, Texture2D, etc.
    int32 SortPriority = 0,
    const FString& Description = TEXT(""));

/**
 * Add an output to a Material Function.
 * Maps to action="add_function_output"
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|MaterialNode")
static FString AddFunctionOutput(
    const FString& FunctionPath,
    const FString& OutputName,
    int32 SortPriority = 0,
    const FString& Description = TEXT(""));
```

### 1.4 Runtime Virtual Texture Management (High)

**File:** New `URuntimeVirtualTextureService.h/.cpp` or extend `UMaterialService`

```cpp
/**
 * Create a Runtime Virtual Texture asset.
 * Maps to action="create_rvt"
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|RVT")
static FAssetCreateResult CreateRuntimeVirtualTexture(
    const FString& AssetName,
    const FString& DirectoryPath,
    const FString& MaterialType = TEXT("BaseColor_Normal_Roughness"), 
        // BaseColor, BaseColor_Normal_Roughness, WorldHeight, etc.
    int32 TileCount = 256,
    int32 TileSize = 256,
    int32 TileBorderSize = 4,
    bool bContinuousUpdate = false,
    bool bSinglePhysicalSpace = false);

/**
 * Get info about an existing RVT asset.
 * Maps to action="get_rvt_info"
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|RVT")
static FRVTInfo GetRuntimeVirtualTextureInfo(const FString& AssetPath);

/**
 * Create a RuntimeVirtualTextureVolume actor covering a landscape.
 * Maps to action="create_rvt_volume"
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|RVT")
static FActorCreateResult CreateRVTVolume(
    const FString& LandscapeNameOrLabel,
    const FString& RVTAssetPath,
    const FString& VolumeName = TEXT(""));

/**
 * Assign an RVT to a landscape component's virtual texture slots.
 * Maps to action="assign_rvt_to_landscape"
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|RVT")
static bool AssignRVTToLandscape(
    const FString& LandscapeNameOrLabel,
    const FString& RVTAssetPath,
    int32 SlotIndex = 0);
```

### 1.5 Material Instance Bulk Parameters (Medium)

**File:** `UMaterialService.h/.cpp`
**Action:** `set_instance_parameters_bulk`

```cpp
USTRUCT(BlueprintType)
struct FInstanceParameterValue
{
    GENERATED_BODY()
    
    FString Name;
    FString Type;  // "Scalar", "Vector", "Texture", "StaticSwitch"
    FString Value; // "0.5", "(R=1,G=0,B=0,A=1)", "/Game/T_Tex", "true"
};

/**
 * Set multiple parameters on a material instance in one call.
 * Maps to action="set_instance_parameters_bulk"
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Material")
static bool SetInstanceParametersBulk(
    const FString& InstancePath,
    const TArray<FInstanceParameterValue>& Parameters);
```

### Implementation Priority

| Priority | Action | Effort | Dependency |
|----------|--------|--------|------------|
| 1 | `export_function_graph` | 1 day | None (reuse export_graph internals) |
| 2 | `get_function_info` | 0.5 day | None |
| 3 | `create_function` + inputs/outputs | 1.5 days | None |
| 4 | RVT create/assign/volume | 1.5 days | None |
| 5 | `set_instance_parameters_bulk` | 0.5 day | None |
| 6 | (From v2 doc) `create_auto_material` | 2 days | None but benefits from 1-4 |
| 7 | (From v2 doc) `find_landscape_textures` | 1 day | None |

**Total: ~8 days of implementation work**

---

## Phase 2: The `landscape-auto-material` Skill (Content — Days 3-4)

### Skill Scope

This skill teaches the AI the **Real_Landscape paradigm**: master materials → material functions → material instances. It's complementary to `landscape-materials` (which covers the simpler LayerBlend approach).

### Skill File: `Content/Skills/landscape-auto-material/skill.md`

```yaml
---
name: landscape-auto-material
display_name: Landscape Auto-Material System
description: >
  Create production-quality landscape materials using the master material + 
  material function + material instance paradigm. Covers Runtime Virtual 
  Textures, auto-layering (slope/altitude/distance blending), layer functions,
  and configuring biomes through material instances.
vibeue_classes:
  - MaterialService
  - MaterialNodeService
  - LandscapeMaterialService
unreal_classes:
  - MaterialExpressionMaterialFunctionCall
  - MaterialExpressionRuntimeVirtualTextureOutput
  - MaterialExpressionRuntimeVirtualTextureSample
  - RuntimeVirtualTexture
  - RuntimeVirtualTextureVolume
  - MaterialFunction
  - MaterialInstanceConstant
  - LandscapeLayerInfoObject
keywords:
  - auto material
  - auto layer
  - master material
  - material function
  - material instance
  - runtime virtual texture
  - RVT
  - altitude blend
  - slope blend
  - distance blend
  - biome
  - landscape material
  - layer function
  - Real_Landscape
  - parametric material
---
```

### Skill Content Structure

The skill will cover these sections:

#### 1. Architecture Overview
- Master Material → Material Functions → Material Instances pattern
- Why this is better than manual graph building (reusability, artist-friendly, performance)
- When to use this skill vs. `landscape-materials` (complex/production vs. simple/prototype)

#### 2. Real_Landscape Reference Architecture
Document the specific material function chain from M_Landscape_Master_01:

```
M_Landscape_Master_01 (Master Material)
├── MF_AutoLayer_01 ──── Drives automatic layer selection
│   ├── MF_Altitude_Blend_01 ── Height-based layer transitions
│   └── MF_Slope_Blend_01 ──── Slope-based layer transitions
├── MF_Layer_Grass ──────── Per-layer texture sampling
├── MF_Layer_Rock ───────── Per-layer texture sampling
├── MF_Layer_Snow ───────── Per-layer texture sampling
├── ... (10 layer functions)
├── MF_RVT_01 ──────────── Runtime Virtual Texture output
├── MF_Distance_Blends_01 ─ LOD transitions
├── MF_Distance_Fades_01 ── Far-distance fading
├── MF_BumpOffset_WorldAlignedNormal_01 ── Parallax
├── MF_BumpOffset_WorldAlignedTexture_01 ── Parallax textures 
├── MF_Color_Correction_01 ── Color grading
├── MF_Color_Variations_01 ── Per-instance color variation
├── MF_Normal_Correction_01 ── Normal map fixes
├── MF_Normal_Variations_01 ── Normal variation
├── MF_PBR_Conversion_01 ──── PBR parameter conversion
├── MF_Texture_Scale_01 ───── UV tiling control
├── MF_Displacement_01 ────── World position offset / tessellation
└── MF_Wind_System_01 ────── Foliage wind animation
```

#### 3. Workflows

**Workflow A: Create a new biome using Real_Landscape's master material**
1. Duplicate MI_Landscape_Default_01 → MI_Landscape_MyBiome_01
2. Set texture parameters for each layer
3. Adjust altitude/slope thresholds
4. Configure RVT
5. Assign to landscape

**Workflow B: Create a new layer function**
1. Duplicate MF_Layer_ToDuplicateToCreateNew
2. Modify texture inputs
3. Wire into master material
4. Create layer info object

**Workflow C: Build a master material from scratch**
1. Create the material
2. Create core material functions (auto-layer, slope, altitude)
3. Create per-layer functions
4. Wire function calls into master material
5. Add RVT output
6. Set material properties (two-sided, virtual texturing enabled)
7. Compile
8. Create material instance
9. Set default parameters
10. Create layer infos
11. Assign to landscape + RVT volume

**Workflow D: Add Runtime Virtual Textures**
1. Create RVT asset
2. Create RVT Volume covering landscape
3. Add RVT Output node to material
4. Connect BaseColor/Normal/Roughness to RVT outputs
5. Enable virtual texturing on material

**Workflow E: Configure auto-blending parameters**
- Height thresholds (where snow/alpine starts)
- Slope thresholds (where rock appears)
- Blend distances (transition sharpness)
- Color correction per-biome

#### 4. Material Function Patterns

Document the internal structure of key material functions:

**MF_AutoLayer_01** — The core auto-layering function:
- Inputs: World Position, World Normal, layer texture arrays
- Logic: Combines altitude + slope masks to weight each layer
- Output: Blended BaseColor, Normal, Roughness

**MF_Slope_Blend_01** — Slope detection:
- Input: World Normal (from vertex)
- Parameters: Slope threshold (degrees), blend width
- Output: 0-1 mask (0=flat, 1=steep)

**MF_Altitude_Blend_01** — Height detection:
- Input: World Position Z
- Parameters: Height threshold, blend width  
- Output: 0-1 mask (0=below, 1=above)

**MF_RVT_01** — Runtime Virtual Texture output:
- Input: BaseColor, Normal, Roughness from layer blending
- Wraps in RuntimeVirtualTextureOutput node
- Enables landscape streaming/paging

#### 5. Common Mistakes
- Forgetting to enable `bUsedWithVirtualTexturing` on the material
- Not creating an RVT Volume actor (material has RVT output but nothing reads it)
- Wrong RVT material type (must match what the material outputs)
- Forgetting to compile after function call changes
- Not setting `bExposeToLibrary` on material functions meant for reuse

#### 6. Parameter Reference Table
A complete table of all parameters exposed by M_Landscape_Master_01 (populated by Phase 0 research).

### Related Skill Files

Also create supplementary `.md` files in the skill directory:
- `auto-layer-architecture.md` — Deep dive on the auto-layer function chain
- `rvt-setup.md` — Complete RVT setup guide
- `layer-function-template.md` — Template for creating new layer functions
- `biome-configuration.md` — How to configure different biomes via instances
- `parameter-reference.md` — Complete master material parameter table

---

## Phase 3: Skill Cross-Referencing Updates (Content — Day 2)

### Update `landscape` skill
Add to bottom of `skills/landscape/skill.md`:
```markdown
### Related Skills
- **landscape-materials**: Simple landscape materials with LandscapeLayerBlend nodes
- **landscape-auto-material**: Production-quality auto-materials with material functions, RVT, and material instances (Real_Landscape paradigm)
  
**When to load which:**
| Task | Skills to Load |
|------|---------------|
| Sculpt terrain only | `landscape` |
| Simple painted material | `landscape` + `landscape-materials` |
| Auto-blending material (production) | `landscape` + `landscape-auto-material` |
| Material instances/biome configuration | `landscape-auto-material` |
```

### Update `landscape-materials` skill
Add cross-reference:
```markdown
### When to Use This Skill vs. landscape-auto-material
- **This skill** (`landscape-materials`): Simple materials with 2-5 layers using LandscapeLayerBlend. Good for prototyping.
- **`landscape-auto-material`**: Production materials using material functions, RVT, and instances. Good for shipping quality.
```

### Update `materials` skill
Add note about material functions:
```markdown
### Material Functions
For creating/editing Material Functions (reusable node subgraphs):
- Use `MaterialNodeService.get_function_info()` to inspect a function's inputs/outputs
- Use `MaterialNodeService.create_function()` to create new functions
- Use `MaterialNodeService.create_function_call()` to reference functions in material graphs
- Load the `landscape-auto-material` skill for landscape-specific material function patterns
```

---

## Phase 4: Validation Workflow (Day 6-7)

### End-to-End Test Plan

#### Test 1: Export Real_Landscape Master Material
- Export `M_Landscape_Master_01` graph JSON
- Verify all 18 function calls are captured
- Verify all parameters are captured
- Verify material output connections

#### Test 2: Recreate from Export
- Use batch_create_specialized + batch_connect to rebuild M_Landscape_Master_01 from the exported JSON
- Compare with export_graph_summary
- Visually verify the material renders identically

#### Test 3: Create New Biome
- Create a new material instance from the master
- Set texture parameters for a custom biome (e.g., tropical)
- Assign to landscape
- Verify visual result

#### Test 4: Build Material Function from Scratch
- Create a custom MF_Layer_Tropical function
- Add inputs (base color texture, normal texture, tiling)
- Add outputs (blended color, blended normal)
- Wire into the master material
- Compile and verify

#### Test 5: Full RVT Pipeline
- Create RVT asset
- Create RVT Volume
- Add RVT output to material
- Verify streaming works

---

## Architecture: Real_Landscape Material Hierarchy

```
Content/Real_Landscape/
│
├── Core/Materials/
│   ├── Masters/
│   │   └── M_Landscape_Master_01          ← Master material (complex graph)
│   │       ├── Uses 18 material functions
│   │       ├── Exposes 50+ parameters
│   │       └── Outputs: BaseColor, Normal, Roughness, WorldPosOffset, RVT
│   │
│   ├── Functions/                          ← Reusable material function library
│   │   ├── MF_AutoLayer_01                ← Core: auto-selects layers by altitude/slope
│   │   ├── MF_Altitude_Blend_01           ← Mask by world Z height
│   │   ├── MF_Slope_Blend_01             ← Mask by surface normal angle
│   │   ├── MF_RVT_01                     ← Runtime Virtual Texture output wrapper
│   │   ├── MF_Distance_Blends_01         ← Near/far texture LOD transitions
│   │   ├── MF_Distance_Fades_01          ← Far-distance alpha fading
│   │   ├── MF_BumpOffset_WorldAligned*    ← Parallax mapping (2 variants)
│   │   ├── MF_Color_Correction_01         ← Per-biome color grading
│   │   ├── MF_Color_Variations_01         ← Instance-level color variety
│   │   ├── MF_Normal_Correction_01        ← Normal map orientation fixes
│   │   ├── MF_Normal_Variations_01        ← Normal variation/blending
│   │   ├── MF_PBR_Conversion_01           ← Roughness/metallic conversion
│   │   ├── MF_Texture_Scale_01            ← Dynamic UV tiling
│   │   ├── MF_Displacement_01             ← World position offset
│   │   ├── MF_Wind_System_01             ← Vegetation wind animation
│   │   ├── MF_TextureObject_To_TextureSample_01
│   │   └── MF_NormalObject_To_NormalSample_01
│   │
│   ├── Layers/                             ← Per-terrain-type layer functions
│   │   ├── MF_Layer_Grass                 ← Grass texture sampling + blending
│   │   ├── MF_Layer_Rock                  ← Rock texture sampling
│   │   ├── MF_Layer_Snow                  ← Snow texture sampling
│   │   ├── MF_Layer_Dirt                  ← Dirt texture sampling
│   │   ├── MF_Layer_Forest                ← Forest floor sampling
│   │   ├── MF_Layer_Beach                 ← Beach/sand sampling
│   │   ├── MF_Layer_Desert                ← Desert sampling
│   │   ├── MF_Layer_Grass_Dry             ← Dry grass variant
│   │   ├── MF_Layer_Rock_Desert           ← Desert rock variant
│   │   └── MF_Layer_ToDuplicateToCreateNew ← Template for new layers
│   │
│   ├── Layer_Info/                         ← LayerInfoObject assets
│   │   ├── Auto_Layer_LayerInfo            ← Special auto-layer blending
│   │   ├── Grass_LayerInfo
│   │   ├── Rock_LayerInfo
│   │   ├── Snow_LayerInfo
│   │   ├── Dirt_LayerInfo
│   │   ├── Forest_LayerInfo
│   │   ├── Beach_LayerInfo
│   │   ├── Desert_LayerInfo
│   │   ├── Grass_Dry_LayerInfo
│   │   ├── FlowMap_LayerInfo               ← For water/flow effects
│   │   └── NoFoliage_LayerInfo             ← Suppress procedural foliage
│   │
│   └── Textures/                           ← Core shared textures
│       ├── T_Ground_Default_BC/H/N
│       ├── T_Nature_Default_BC/N/RMA_01
│       └── T_Noise_01, T_Noise_02
│
├── Default/                                ← "Default" biome configuration
│   ├── Materials/Landscape/
│   │   ├── MI_Landscape_Default_01         ← Material instance for Default biome
│   │   └── MI_Landscape_Default_02         ← Variant
│   ├── Textures/Landscape/                 ← 36 biome-specific textures (BC/H/N per terrain)
│   ├── Textures/RVT/
│   │   └── RVT_Default_Demo_01             ← Runtime Virtual Texture asset
│   └── GrassTypes/                         ← 8 procedural grass type configs
│
└── Meadow/                                 ← "Meadow" biome configuration
    ├── Materials/Landscape/
    │   ├── MI_Landscape_Meadow_Island_01   ← Island variant instance
    │   └── MI_Landscape_Meadow_Mountain_01 ← Mountain variant instance
    └── Maps/                               ← Demo levels
```

---

## Summary: What Gets Built

| Deliverable | Type | Effort | Phase |
|-------------|------|--------|-------|
| Real_Landscape architecture reference doc | Research doc | 1 day | 0 |
| `export_function_graph` action | C++ tool | 1 day | 1 |
| `get_function_info` action | C++ tool | 0.5 day | 1 |
| `create_function` + input/output management | C++ tool | 1.5 days | 1 |
| RVT create/assign/volume actions | C++ tool | 1.5 days | 1 |
| `set_instance_parameters_bulk` action | C++ tool | 0.5 day | 1 |
| `create_auto_material` (from v2 doc) | C++ tool | 2 days | 1 |
| `find_landscape_textures` (from v2 doc) | C++ tool | 1 day | 1 |
| `landscape-auto-material` skill.md | Skill content | 1 day | 2 |
| Supplementary skill docs (5 files) | Skill content | 1 day | 2 |
| Cross-reference updates (3 skills) | Skill content | 0.5 day | 3 |
| Validation tests (5 scenarios) | Testing | 1.5 days | 4 |
| **Total** | | **~13 days** | |

### Recommended Build Order

```
Week 1:
  Day 1: Phase 0 — Export & document Real_Landscape architecture
  Day 2: Phase 3 — Skill cross-referencing (immediate impact, content-only)
  Day 3: Phase 1.1 — export_function_graph  
  Day 4: Phase 1.2 — get_function_info
  Day 5: Phase 1.3 — create_function + inputs/outputs

Week 2:
  Day 6: Phase 1.4 — RVT tools
  Day 7: Phase 1.5 — set_instance_parameters_bulk
  Day 8-9: Phase 1.6 — create_auto_material
  Day 10: Phase 1.7 — find_landscape_textures

Week 3:
  Day 11: Phase 2 — Write landscape-auto-material skill + supplementary docs
  Day 12-13: Phase 4 — Validation tests
```

### Quick Wins (Can Do Today)
1. **Skill cross-referencing** — Update landscape, landscape-materials, and materials skills (Phase 3, markdown-only)
2. **Phase 0 research** — Run export_graph on M_Landscape_Master_01 to start building the architecture reference

---

## Appendix A: Key Unreal Classes for RVT

| Class | Purpose |
|-------|---------|
| `URuntimeVirtualTexture` | The RVT asset itself — defines tile count, size, material type |
| `ARuntimeVirtualTextureVolume` | World actor that bounds the RVT to a region |
| `UMaterialExpressionRuntimeVirtualTextureOutput` | Material node that writes to RVT |
| `UMaterialExpressionRuntimeVirtualTextureSample` | Material node that reads from RVT |
| `UMaterialExpressionRuntimeVirtualTextureSampleParameter` | Parameterized RVT sample |
| `UVirtualTexture2D` | Underlying virtual texture resource |

## Appendix B: Material Function Input Types

| EFunctionInputType | Name | Description |
|---------------------|------|-------------|
| 0 | Scalar | Single float |
| 1 | Vector2 | 2D vector |
| 2 | Vector3 | 3D vector (BaseColor, Normal) |
| 3 | Vector4 | RGBA / 4D vector |
| 4 | Texture2D | 2D texture reference |
| 5 | TextureCube | Cubemap |
| 6 | TextureExternal | External texture |
| 7 | VolumeTexture | 3D texture |
| 8 | StaticBool | Static switch |
| 9 | MaterialAttributes | Full material attributes struct |

## Appendix C: Existing Real_Landscape Texture Naming Convention

| Suffix | Meaning | Example |
|--------|---------|---------|
| `_BC` | Base Color / Albedo | `T_Ground_Clovers_01_BC` |
| `_N` | Normal Map | `T_Ground_Clovers_01_N` |
| `_H` | Height Map | `T_Ground_Clovers_01_H` |
| `_RMA` | Roughness-Metallic-AO packed | `T_Nature_Default_RMA_01` |

Biome texture sets follow: `T_Ground_<TerrainType>_##_<Suffix>`
- `T_Ground_Clovers_01_BC`, `T_Ground_Desert_01_BC`, `T_Ground_Dirt_01_BC`
- `T_Ground_Forest_01_BC`, `T_Ground_Grass_01_BC`, `T_Ground_Rock_01_BC`
- `T_Ground_Sand_01_BC`, `T_Ground_Snow_01_BC`

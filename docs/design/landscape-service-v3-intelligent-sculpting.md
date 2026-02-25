# ULandscapeService v3 — Intelligent Sculpting & Mesh Projection

## Motivation

### The Problem

An AI agent was asked to create a landscape that matches the geometry of two existing static mesh mountains in the scene. After **three failed approaches**, we identified fundamental gaps in the tooling:

1. **Attempt 1 — Raise/Lower Region:** Used `raise_lower_region` with negative delta to "carve" depressions. Result: the landscape was sunken. The agent misunderstood the direction — it needed to raise terrain INTO mountain shapes, not dig holes.

2. **Attempt 2 — Cosine-Falloff Shapes:** Used `set_height_in_region` to write procedural cosine-falloff mountain shapes centered at mesh locations. Result: generic cone-shaped mountains that looked nothing like the actual mesh geometry. The agent was guessing at shapes rather than reading the real data.

3. **Attempt 3 — Python Line Traces:** Used `unreal.SystemLibrary.line_trace_single()` from Python to sample the mesh surface geometry vertex-by-vertex. Individual traces worked (~0ms each, returning accurate Z heights). But the landscape had ~1.4M vertices requiring ~1.4M traces. The Python-C++ marshalling overhead per call made full-resolution sampling impractical. The user cancelled after several minutes of no visible progress.

### Root Causes

| Gap | Description |
|-----|-------------|
| **No mesh sampling** | No C++ native tool to batch-sample a mesh's surface geometry at arbitrary XY positions |
| **No mesh-to-landscape projection** | No tool to project mesh geometry onto a landscape heightmap in a single operation |
| **No semantic terrain tools** | Every sculpting action operates at the raw vertex level. There's no "create a mountain" or "carve a valley" — only "set these heights to these values" |
| **No terrain analysis** | No slope/curvature/elevation queries to make intelligent decisions about where to paint or how to blend |
| **No height-based painting** | Manual brush-stroke painting for slope/height zones; no automatic "paint rock above 5000 units" |
| **Python ↔ C++ overhead** | Each Python-to-C++ function call has marshalling overhead. 1M individual calls is prohibitively slow even when each call takes <1ms |

### User Expectations

> *"I don't want the mountain placed on the landscape... I want the landscape sculpted like the two mountains."*
> *"We want the tool to be better at sculpting, not just monkey see monkey do."*

Two distinct capabilities are needed:

1. **Mesh Projection** (the literal task): Sample a mesh's surface and write it to the heightmap. This is precise, deterministic, and geometry-driven.

2. **Intelligent Sculpting** (the broader need): Semantic terrain operations that understand geological features — mountains, valleys, ridges, erosion, plateaus — not just raw height arrays. The agent should be able to say "create a mountain range here" with believable results, not compute every vertex manually.

---

## Current State: 46 Actions, 10 Categories

After v2 implementation, the service covers:

| Category | Actions | Capability |
|----------|---------|------------|
| CRUD | 4 | Create, delete, list, get info, landscape exists |
| Heightmap I/O | 2 | Import/export 16-bit PNG/RAW |
| Height Read/Write | 3 | Get/set height at location or in region |
| Sculpting | 5 | Sculpt, flatten, smooth (circular brush), raise/lower region, apply noise |
| Paint Layers | 5 | List, add, remove layers; get weights at location; paint at location |
| Batch Painting | 4 | Paint in region/world rect; get/set weights in region |
| Weight Map I/O | 2 | Import/export weight maps as grayscale PNG |
| Holes | 3 | Set/get hole at location or in region |
| Splines | 10 | Full spline CRUD with mesh support and terrain deformation |
| Properties | 5 | Material, properties, visibility, collision, resize |
| Existence | 2 | Check landscape/layer exists |

**What's missing for intelligent sculpting:**
- Zero mesh interaction tools
- Zero procedural terrain feature tools (only Perlin noise)
- Zero analysis/query tools (slope, curvature, elevation zones)
- Zero auto-painting tools (everything requires explicit coordinates)
- No batch geometry operations at C++ speed

---

## Proposed Feature Areas

### Feature 1: Mesh-to-Landscape Projection

**Priority: Critical**
**Estimated effort: 3–4 days**

The single highest-value addition. Enables the complete "make the landscape match this mesh" workflow.

#### Concept

Given a static mesh actor in the scene, project its surface geometry downward (or upward) onto the landscape heightmap. For each landscape vertex that falls within the mesh's XY bounding box, cast a ray from above (or below) the mesh and record where it hits the mesh surface. Write those Z values to the landscape.

This replaces 1M+ individual Python line traces with a single C++ call that:
1. Enables collision on the mesh temporarily if needed
2. Iterates landscape vertices in the overlap region
3. Casts rays against the mesh at native C++ speed
4. Writes the resulting height array directly to the landscape
5. Optionally applies blending at the edges

#### New USTRUCTs

```cpp
/**
 * Configuration for mesh-to-landscape projection.
 */
USTRUCT(BlueprintType)
struct FMeshProjectionConfig
{
    GENERATED_BODY()

    /** Actor name, label, or path to the static mesh actor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MeshActorIdentifier;

    /** Height offset applied to all projected values (world units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HeightOffset = 0.0f;

    /** Width of the blending zone at the mesh boundary (world units).
     *  0 = hard edge, >0 = smooth linear blend from mesh edge to existing terrain */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BlendRadius = 0.0f;

    /** Blend curve type: "Linear", "Smooth" (hermite), "Ease" (cubic) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BlendFalloff = TEXT("Smooth");

    /** Operation mode:
     *  "Replace" - overwrite landscape heights with mesh surface
     *  "Max" - take the higher of mesh surface or existing terrain
     *  "Min" - take the lower of mesh surface or existing terrain
     *  "Add" - add mesh surface height (relative to mesh origin Z) to terrain
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BlendMode = TEXT("Replace");

    /** If true, the mesh actor is hidden after projection (moved below landscape) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHideMeshAfterProjection = false;

    /** Maximum ray cast distance above the mesh bounds. Default 100000 units. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RaycastHeight = 100000.0f;

    /** If true, only project where the mesh surface is ABOVE the current landscape.
     *  Useful for additive mountain stamping without cutting into existing terrain. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bOnlyRaiseTerrrain = false;
};

/**
 * Result of a mesh-to-landscape projection operation.
 */
USTRUCT(BlueprintType)
struct FMeshProjectionResult
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSuccess = false;

    /** Number of landscape vertices modified */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 VerticesModified = 0;

    /** Number of rays that hit the mesh surface */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RayHits = 0;

    /** Number of rays that missed (vertex is outside mesh projection) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RayMisses = 0;

    /** Min/max Z values written to the landscape */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinHeightWritten = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHeightWritten = 0.0f;

    /** World-space bounding box of the affected landscape region */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector RegionMin = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector RegionMax = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ErrorMessage;
};
```

#### New Actions (3 actions)

##### `project_mesh_to_landscape`
The primary workhorse. Projects one mesh actor's geometry onto the landscape heightmap.

```cpp
/**
 * Project a static mesh actor's surface geometry onto the landscape heightmap.
 * Maps to action="project_mesh_to_landscape"
 *
 * For each landscape vertex within the mesh's XY bounding box, casts a ray
 * downward through the mesh to determine the surface Z height, then writes
 * that height to the landscape. Runs entirely in C++ for performance.
 *
 * The mesh does NOT need pre-existing collision — complex collision is
 * enabled temporarily for the duration of the trace, then restored.
 *
 * @param LandscapeNameOrLabel - Name or label of the landscape
 * @param Config - Projection configuration (mesh actor, blend mode, etc.)
 * @return Projection result with statistics
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|MeshProjection")
static FMeshProjectionResult ProjectMeshToLandscape(
    const FString& LandscapeNameOrLabel,
    const FMeshProjectionConfig& Config);
```

**Implementation sketch:**
1. Find the landscape and mesh actor
2. Get static mesh component, save current collision settings
3. Enable `UseComplexAsSimple` collision for accurate surface tracing
4. Get the mesh actor's world bounding box, expand by `BlendRadius`
5. Convert bounding box to landscape vertex index range
6. Read existing heights via `GetHeightInRegion()`
7. For each vertex (X, Y) in the range:
   a. Compute world XY from vertex index
   b. Cast `LineTraceSingleByChannel` from `(WorldX, WorldY, MeshBoundsTop + RaycastHeight)` downward to `(WorldX, WorldY, MeshBoundsBottom - 1000)`
   c. If hit: record `ImpactPoint.Z`
   d. If miss: mark as "no mesh here"
8. For vertices with hits, apply `BlendMode` (Replace/Max/Min/Add)
9. For vertices in the `BlendRadius` zone, interpolate between projected height and existing terrain using `BlendFalloff`
10. Write the modified height array via `SetHeightInRegion()`
11. Restore original collision settings on the mesh
12. Return statistics

**Performance estimate:** A 2017×2017 landscape covers ~4M vertices, but typically only 5-20% overlap with a mesh. For 200K-800K rays, C++ `LineTraceSingle` at ~1μs each = 0.2-0.8 seconds. Acceptable.

##### `project_multiple_meshes_to_landscape`
Batch version for projecting multiple meshes in one call.

```cpp
/**
 * Project multiple static mesh actors onto the landscape in a single operation.
 * Maps to action="project_meshes_to_landscape"
 *
 * Processes meshes in order. Later meshes can overwrite earlier ones based
 * on their individual BlendMode settings. Uses a single heightmap read/write.
 *
 * @param LandscapeNameOrLabel - Name or label of the landscape
 * @param Configs - Array of projection configs, one per mesh
 * @return Array of results, one per mesh
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|MeshProjection")
static TArray<FMeshProjectionResult> ProjectMultipleMeshesToLandscape(
    const FString& LandscapeNameOrLabel,
    const TArray<FMeshProjectionConfig>& Configs);
```

**Optimization:** Reads the full heightmap once, processes all meshes against the same array, writes once at the end. Avoids N read-write cycles.

##### `sample_mesh_heights`
Lower-level utility: sample a mesh's surface heights at specific world XY positions without writing to any landscape. Useful for inspection, preview, or custom processing.

```cpp
/**
 * Sample surface heights from a static mesh actor at given world XY positions.
 * Maps to action="sample_mesh_heights"
 *
 * Casts rays downward through the mesh at each XY position. Returns the Z
 * height of the first hit, or NaN for misses.
 *
 * @param MeshActorIdentifier - Name or label of the mesh actor
 * @param WorldXYPositions - Array of (X,Y) positions to sample (Z is ignored)
 * @param RaycastHeight - Max distance above mesh bounds to start ray
 * @return Array of Z heights (NaN = miss), same order as input positions
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|MeshProjection")
static TArray<float> SampleMeshHeights(
    const FString& MeshActorIdentifier,
    const TArray<FVector2D>& WorldXYPositions,
    float RaycastHeight = 100000.0f);
```

#### Python Usage

```python
import unreal

svc = unreal.LandscapeService

# Project a single mountain mesh onto the landscape
config = unreal.MeshProjectionConfig()
config.mesh_actor_identifier = "SM_STZD_Background_Landscape_8"
config.blend_radius = 5000.0        # 50m smooth blend at edges
config.blend_falloff = "Smooth"
config.blend_mode = "Max"           # Only raise terrain, don't cut into it
config.only_raise_terrain = True

result = svc.project_mesh_to_landscape("TerrainBase", config)
print(f"Modified {result.vertices_modified} vertices, "
      f"{result.ray_hits} hits, {result.ray_misses} misses")

# Project multiple meshes in one batch
configs = []
for mesh_name in ["SM_STZD_Background_Landscape_8", "SM_STZD_Background_Landscape_5"]:
    cfg = unreal.MeshProjectionConfig()
    cfg.mesh_actor_identifier = mesh_name
    cfg.blend_radius = 3000.0
    cfg.blend_mode = "Max"
    configs.append(cfg)

results = svc.project_meshes_to_landscape("TerrainBase", configs)
```

---

### Feature 2: Semantic Terrain Features

**Priority: Critical**
**Estimated effort: 5–7 days**

This is the "not monkey see monkey do" capability. Instead of requiring the AI agent to compute every vertex height manually, provide high-level terrain feature generators that produce geologically plausible results.

#### Philosophy

Current sculpting tools are **imperative** — "set this height to this value." The agent must compute every vertex's height, which requires domain knowledge of terrain geometry, falloff curves, erosion patterns, etc.

Semantic tools are **declarative** — "create a mountain here with these parameters." The C++ implementation handles all the geometry math, noise layering, and blending.

This transforms the agent's job from:
- ❌ *"Calculate 50,000 height values using cosine falloff with noise octaves"*
- ✅ *"Create a mountain at (X, Y) with radius 80,000 and peak height 40,000"*

#### New USTRUCTs

```cpp
/**
 * Configuration for a terrain feature.
 * Different feature types use different subsets of these parameters.
 */
USTRUCT(BlueprintType)
struct FTerrainFeatureConfig
{
    GENERATED_BODY()

    /** World XY center of the feature */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector2D Center = FVector2D::ZeroVector;

    /** Primary radius of the feature in world units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Radius = 10000.0f;

    /** Secondary radius for asymmetric features (0 = use Radius for both) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RadiusY = 0.0f;

    /** Peak height (above base) in world units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Height = 5000.0f;

    /** Rotation of the feature in degrees (around Z axis) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Rotation = 0.0f;

    /** Blending width at the edge of the feature */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EdgeBlend = 2000.0f;

    /** Amount of noise displacement (0 = perfectly smooth, 1 = very rough) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Roughness = 0.3f;

    /** Noise frequency multiplier (higher = more detailed surface texture) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DetailFrequency = 1.0f;

    /** Random seed for noise generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Seed = 0;

    /** Blend mode: "Add", "Max", "Replace", "Subtract" */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BlendMode = TEXT("Add");
};

/**
 * Result of a terrain feature generation operation.
 */
USTRUCT(BlueprintType)
struct FTerrainFeatureResult
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSuccess = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 VerticesModified = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinHeightDelta = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHeightDelta = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ErrorMessage;
};
```

#### New Actions (8 actions)

##### `create_mountain`
Generate a mountain or hill feature with natural-looking profile and noise.

```cpp
/**
 * Generate a mountain/hill feature on the landscape.
 * Maps to action="create_mountain"
 *
 * Creates a peaked feature using layered noise over a base profile curve.
 * The profile uses a modified Gaussian with ridge displacement for natural
 * appearance. Roughness controls the amplitude of multi-octave noise
 * applied to the surface.
 *
 * Profile types:
 * - "Peaked" - Sharp summit with concave flanks (alpine)
 * - "Rounded" - Dome-like summit (weathered hills)
 * - "Plateau" - Flat top with steep sides (mesa/butte)
 * - "Ridged" - Elongated peak along rotation axis (mountain ridge)
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param Config - Feature configuration
 * @param ProfileType - Shape profile (default "Peaked")
 * @return Feature result with statistics
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|TerrainFeatures")
static FTerrainFeatureResult CreateMountain(
    const FString& LandscapeNameOrLabel,
    const FTerrainFeatureConfig& Config,
    const FString& ProfileType = TEXT("Peaked"));
```

**Implementation sketch:**
1. Get landscape info, compute affected vertex region from Config.Center ± (Radius + EdgeBlend)
2. Read existing heights
3. For each vertex:
   a. Compute normalized distance from center (accounting for RadiusY, Rotation)
   b. Apply base profile curve (Gaussian, plateau, ridge, etc.)
   c. Multiply by Config.Height
   d. Add multi-octave Perlin noise scaled by Roughness × profile_value
   e. Apply edge blend using hermite interpolation
   f. Combine with existing terrain using BlendMode
4. Write heights back

##### `create_valley`
Carve a valley or canyon through the terrain.

```cpp
/**
 * Carve a valley through the landscape along a path.
 * Maps to action="create_valley"
 *
 * The valley follows a line from StartPoint to EndPoint with configurable
 * width, depth, and cross-section profile. Noise adds natural irregularity
 * to the valley walls.
 *
 * Profile types:
 * - "VShape" - Sharp V-shaped canyon
 * - "UShape" - Rounded U-shaped glacial valley
 * - "Flat" - Flat-bottomed river valley
 * - "Gorge" - Narrow with near-vertical walls
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param StartPoint - Valley start (world XY)
 * @param EndPoint - Valley end (world XY)
 * @param Width - Valley width in world units
 * @param Depth - Maximum depth below existing terrain in world units
 * @param ProfileType - Cross-section profile
 * @param Roughness - Noise displacement (0-1)
 * @param EdgeBlend - Blend at valley rim (world units)
 * @param Seed - Random seed
 * @return Feature result
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|TerrainFeatures")
static FTerrainFeatureResult CreateValley(
    const FString& LandscapeNameOrLabel,
    FVector2D StartPoint,
    FVector2D EndPoint,
    float Width = 5000.0f,
    float Depth = 3000.0f,
    const FString& ProfileType = TEXT("UShape"),
    float Roughness = 0.2f,
    float EdgeBlend = 2000.0f,
    int32 Seed = 0);
```

##### `create_ridge`
Create an elongated ridge or mountain range.

```cpp
/**
 * Create a ridge or mountain range along a polyline path.
 * Maps to action="create_ridge"
 *
 * Unlike create_mountain (radial), a ridge follows an arbitrary path with
 * consistent cross-section. The backbone can be displaced by noise for
 * natural meandering. Individual peaks along the ridge are auto-generated.
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param PathPoints - World XY waypoints along the ridge backbone
 * @param Width - Width of the ridge base (world units)
 * @param PeakHeight - Maximum height of ridge peaks (world units)
 * @param MinHeight - Minimum saddle height between peaks (world units)
 * @param Roughness - Surface noise (0-1)
 * @param PeakFrequency - How often peaks occur along the ridge (0.0001-0.01)
 * @param EdgeBlend - Blend at ridge base (world units)
 * @param Seed - Random seed
 * @return Feature result
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|TerrainFeatures")
static FTerrainFeatureResult CreateRidge(
    const FString& LandscapeNameOrLabel,
    const TArray<FVector2D>& PathPoints,
    float Width = 8000.0f,
    float PeakHeight = 8000.0f,
    float MinHeight = 3000.0f,
    float Roughness = 0.4f,
    float PeakFrequency = 0.001f,
    float EdgeBlend = 3000.0f,
    int32 Seed = 0);
```

##### `create_plateau`
Create a flat-topped elevated area.

```cpp
/**
 * Create a plateau (flat elevated area with steep edges).
 * Maps to action="create_plateau"
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param Config - Feature configuration (Height = plateau elevation)
 * @param CliffSteepness - How steep the edges are (0.1 = gentle, 1.0 = cliff)
 * @return Feature result
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|TerrainFeatures")
static FTerrainFeatureResult CreatePlateau(
    const FString& LandscapeNameOrLabel,
    const FTerrainFeatureConfig& Config,
    float CliffSteepness = 0.7f);
```

##### `apply_erosion`
Apply hydraulic erosion simulation to make terrain look naturally weathered.

```cpp
/**
 * Apply hydraulic erosion simulation to the landscape.
 * Maps to action="apply_erosion"
 *
 * Simulates water droplets flowing downhill, carving channels and depositing
 * sediment. Dramatically improves the realism of procedurally generated terrain.
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param WorldCenterX - Center X of erosion region
 * @param WorldCenterY - Center Y of erosion region
 * @param WorldRadius - Radius of the affected region
 * @param Iterations - Number of water droplet simulations (more = deeper erosion)
 * @param ErosionStrength - How aggressively water erodes terrain (0.01-1.0)
 * @param DepositionStrength - How readily sediment is deposited (0.01-1.0)
 * @param EvaporationRate - How quickly droplets evaporate (0.01-0.1)
 * @param Seed - Random seed for droplet starting positions
 * @return Feature result
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|TerrainFeatures")
static FTerrainFeatureResult ApplyErosion(
    const FString& LandscapeNameOrLabel,
    float WorldCenterX, float WorldCenterY,
    float WorldRadius,
    int32 Iterations = 50000,
    float ErosionStrength = 0.3f,
    float DepositionStrength = 0.3f,
    float EvaporationRate = 0.02f,
    int32 Seed = 0);
```

**Implementation:** Particle-based hydraulic erosion. Each iteration:
1. Place a water droplet at a random position within the region
2. Calculate terrain gradient at the droplet position
3. Move the droplet downhill following the gradient
4. Erode the terrain based on speed and slope
5. Deposit sediment when the droplet slows down
6. Evaporate a fraction of the droplet's water each step
7. Repeat until the droplet evaporates or leaves the region

This is a well-studied algorithm (see: Hans Theobald Beyer, "Implementation of a method for hydraulic erosion", 2015).

##### `create_crater`
Create an impact crater or caldera.

```cpp
/**
 * Create an impact crater on the landscape.
 * Maps to action="create_crater"
 *
 * Generates a circular depression with raised rim and optional central peak.
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param Center - World XY center
 * @param Radius - Outer radius including rim
 * @param Depth - Depth of crater floor below existing terrain
 * @param RimHeight - Height of rim above existing terrain
 * @param bCentralPeak - Whether to add a central peak (large craters)
 * @param Roughness - Surface noise (0-1)
 * @param Seed - Random seed
 * @return Feature result
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|TerrainFeatures")
static FTerrainFeatureResult CreateCrater(
    const FString& LandscapeNameOrLabel,
    FVector2D Center,
    float Radius = 5000.0f,
    float Depth = 2000.0f,
    float RimHeight = 500.0f,
    bool bCentralPeak = false,
    float Roughness = 0.2f,
    int32 Seed = 0);
```

##### `create_terraces`
Create stepped/terraced terrain (rice paddies, geological strata, etc.).

```cpp
/**
 * Create terraced terrain in a region.
 * Maps to action="create_terraces"
 *
 * Quantizes the existing terrain height into discrete steps, creating flat
 * levels with steep transitions between them.
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param WorldCenterX - Center X of the terraced region
 * @param WorldCenterY - Center Y of the terraced region
 * @param WorldRadius - Radius of the affected region
 * @param NumTerraces - Number of height levels
 * @param Strength - Blend between original terrain and terraced (0-1, where 1 = fully terraced)
 * @param TransitionSharpness - How sharp the steps are (0.1 = smooth transitions, 1.0 = cliff steps)
 * @return Feature result
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|TerrainFeatures")
static FTerrainFeatureResult CreateTerraces(
    const FString& LandscapeNameOrLabel,
    float WorldCenterX, float WorldCenterY,
    float WorldRadius,
    int32 NumTerraces = 5,
    float Strength = 0.8f,
    float TransitionSharpness = 0.5f);
```

##### `blend_terrain_features`
Combine multiple feature descriptions and generate them as a single coherent operation.

```cpp
/**
 * Generate multiple terrain features in a single operation with correct interaction.
 * Maps to action="blend_terrain_features"
 *
 * Unlike calling individual feature functions sequentially (which may create
 * visible seams), this generates all features into a shared buffer with
 * intelligent blending at intersection zones.
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param Mountains - Array of mountain configs
 * @param Valleys - Array of valley configs (start/end encoded in PathPoints)
 * @param GlobalRoughness - Additional noise applied to the entire result
 * @param Seed - Master random seed
 * @return Feature result for the combined operation
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|TerrainFeatures")
static FTerrainFeatureResult BlendTerrainFeatures(
    const FString& LandscapeNameOrLabel,
    const TArray<FTerrainFeatureConfig>& Mountains,
    float GlobalRoughness = 0.1f,
    int32 Seed = 0);
```

#### Python Usage

```python
import unreal
svc = unreal.LandscapeService

# Create a mountain range with a single call
config = unreal.TerrainFeatureConfig()
config.center = unreal.Vector2D(-30000, -110000)
config.radius = 100000
config.height = 45000
config.roughness = 0.4
config.seed = 42
config.blend_mode = "Add"
result = svc.create_mountain("TerrainBase", config, "Peaked")

# Carve a valley through the mountains
result = svc.create_valley("TerrainBase",
    start_point=unreal.Vector2D(-80000, -60000),
    end_point=unreal.Vector2D(30000, -160000),
    width=8000, depth=5000,
    profile_type="UShape", roughness=0.3)

# Apply erosion for realism
result = svc.apply_erosion("TerrainBase",
    0.0, -110000.0, 120000.0,
    iterations=100000, seed=42)
```

---

### Feature 3: Terrain Analysis & Queries

**Priority: High**
**Estimated effort: 2–3 days**

The AI agent needs to understand the current terrain state before making intelligent decisions. Currently there's no way to ask "what's the slope here?" or "where are the steep areas?"

#### New USTRUCTs

```cpp
/**
 * Analysis data for a terrain region.
 */
USTRUCT(BlueprintType)
struct FTerrainAnalysis
{
    GENERATED_BODY()

    /** Height statistics */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinHeight = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHeight = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MeanHeight = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MedianHeight = 0.0f;

    /** Slope statistics (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinSlope = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxSlope = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MeanSlope = 0.0f;

    /** Terrain roughness (standard deviation of local height differences) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Roughness = 0.0f;

    /** Vertices in the analyzed region */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 VertexCount = 0;
};
```

#### New Actions (5 actions)

##### `analyze_terrain`
Get statistical summary of terrain in a world-space region.

```cpp
/**
 * Analyze terrain characteristics in a circular region.
 * Maps to action="analyze_terrain"
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param WorldCenterX - Center X
 * @param WorldCenterY - Center Y
 * @param WorldRadius - Analysis radius
 * @return Terrain analysis with height/slope/roughness statistics
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Analysis")
static FTerrainAnalysis AnalyzeTerrain(
    const FString& LandscapeNameOrLabel,
    float WorldCenterX, float WorldCenterY,
    float WorldRadius);
```

##### `get_slope_at_location`
Get the terrain slope in degrees at a world position.

```cpp
/**
 * Get terrain slope at a world location.
 * Maps to action="get_slope_at_location"
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param WorldX - World X
 * @param WorldY - World Y
 * @return Slope in degrees (0 = flat, 90 = vertical cliff)
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Analysis")
static float GetSlopeAtLocation(
    const FString& LandscapeNameOrLabel,
    float WorldX, float WorldY);
```

##### `get_normal_at_location`
Get the terrain surface normal vector.

```cpp
/**
 * Get terrain surface normal at a world location.
 * Maps to action="get_normal_at_location"
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param WorldX - World X
 * @param WorldY - World Y
 * @return World-space normal vector (Z-up)
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Analysis")
static FVector GetNormalAtLocation(
    const FString& LandscapeNameOrLabel,
    float WorldX, float WorldY);
```

##### `get_slope_map`
Get a slope data array for a region, usable for auto-painting decisions.

```cpp
/**
 * Calculate slope values for every vertex in a region.
 * Maps to action="get_slope_map"
 *
 * Returns slope in degrees for each vertex, row-major.
 * Useful as input for height/slope-based auto-painting.
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param StartX - Start vertex X
 * @param StartY - Start vertex Y
 * @param SizeX - Width in vertices
 * @param SizeY - Height in vertices
 * @return Row-major float array of slopes in degrees
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Analysis")
static TArray<float> GetSlopeMap(
    const FString& LandscapeNameOrLabel,
    int32 StartX, int32 StartY,
    int32 SizeX, int32 SizeY);
```

##### `find_flat_areas`
Find regions of terrain that are flat enough for placement (landing zones, building sites, etc.).

```cpp
/**
 * Find flat areas within a search region.
 * Maps to action="find_flat_areas"
 *
 * Returns the world XY positions of areas where terrain slope is below
 * the specified threshold for at least the given minimum radius.
 *
 * @param LandscapeNameOrLabel - Target landscape
 * @param WorldCenterX - Search region center X
 * @param WorldCenterY - Search region center Y
 * @param SearchRadius - Search region radius (world units)
 * @param MaxSlope - Maximum slope in degrees to consider "flat"
 * @param MinFlatRadius - Minimum radius of flat area (world units)
 * @return Array of flat area center positions
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Analysis")
static TArray<FVector> FindFlatAreas(
    const FString& LandscapeNameOrLabel,
    float WorldCenterX, float WorldCenterY,
    float SearchRadius,
    float MaxSlope = 10.0f,
    float MinFlatRadius = 1000.0f);
```

---

### Feature 4: Batch Geometry Sampling

**Priority: Medium**
**Estimated effort: 1–2 days**

General-purpose C++ batch line tracing for use cases beyond landscape- specific projection. Useful for any scenario where Python needs to query the 3D scene geometry at scale.

#### New Actions (2 actions)

##### `batch_line_trace`
Perform many line traces in a single C++ call.

```cpp
/**
 * Perform batch line traces in C++ for high-performance scene queries.
 * Maps to action="batch_line_trace"
 *
 * Runs all traces on the game thread at C++ speed, avoiding per-call
 * Python↔C++ marshalling overhead.
 *
 * @param StartPoints - Array of trace start positions
 * @param EndPoints - Array of trace end positions (same length as StartPoints)
 * @param TraceChannel - Collision channel ("Visibility", "Camera", "WorldStatic")
 * @param ActorsToIgnore - Actor names/labels to exclude from traces
 * @return Array of hit Z values (NaN = no hit), same order as input
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Geometry")
static TArray<float> BatchLineTrace(
    const TArray<FVector>& StartPoints,
    const TArray<FVector>& EndPoints,
    const FString& TraceChannel = TEXT("Visibility"),
    const TArray<FString>& ActorsToIgnore = TArray<FString>());
```

**Note:** This is a utility on the LandscapeService for convenience, but it's not landscape-specific. It could alternatively live on a general GeometryService. For now, keeping it here since the primary use case is landscape-related mesh sampling.

##### `batch_line_trace_grid`
Trace a regular grid of points — the most common pattern for landscape work.

```cpp
/**
 * Trace a regular XY grid of vertical rays downward.
 * Maps to action="batch_line_trace_grid"
 *
 * Generates a grid of (SizeX × SizeY) rays from top to bottom and returns
 * the hit Z for each. This is the optimal pattern for sampling mesh geometry
 * for landscape projection.
 *
 * @param WorldMinX - Minimum X of the grid
 * @param WorldMinY - Minimum Y of the grid
 * @param WorldMaxX - Maximum X of the grid
 * @param WorldMaxY - Maximum Y of the grid
 * @param SizeX - Number of samples in X direction
 * @param SizeY - Number of samples in Y direction
 * @param RayTopZ - Z position to start rays from (above scene)
 * @param RayBottomZ - Z position to end rays at (below scene)
 * @param TraceChannel - Collision channel
 * @param ActorsToIgnore - Actors to exclude
 * @return Row-major float array of hit Z values (NaN = miss)
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Geometry")
static TArray<float> BatchLineTraceGrid(
    float WorldMinX, float WorldMinY,
    float WorldMaxX, float WorldMaxY,
    int32 SizeX, int32 SizeY,
    float RayTopZ = 100000.0f,
    float RayBottomZ = -100000.0f,
    const FString& TraceChannel = TEXT("Visibility"),
    const TArray<FString>& ActorsToIgnore = TArray<FString>());
```

---

## Complete Feature Summary

> **Note on height/slope-based painting:** UE landscape materials natively blend layers by height and slope via `LandscapeLayerBlend` nodes and world-normal material logic — no runtime weight-map painting needed. Use `ULandscapeMaterialService`:
> - `create_height_mask` — AbsoluteWorldPosition Z → SmoothStep mask (0 below threshold, 1 above)
> - `create_slope_mask` — VertexNormalWS Z → OneMinus → SmoothStep mask (0=flat, 1=steep)
> - `setup_height_slope_blend` — one-call setup: creates masks and wires them into an existing `LandscapeLayerBlend` node with `LB_AlphaBlend` layers

| # | Feature | Actions | Priority | Est. Effort | Risk |
|---|---------|---------|----------|-------------|------|
| 1 | Mesh-to-Landscape Projection | 3 | Critical | 3–4 days | Medium |
| 2 | Semantic Terrain Features | 8 | Critical | 5–7 days | Medium-High |
| 3 | Terrain Analysis & Queries | 5 | High | 2–3 days | Low |
| 4 | Batch Geometry Sampling | 2 | Medium | 1–2 days | Low |
| **Total** | | **18** | | **11–16 days** | |

After implementation: **~64 actions** across **~13 categories**.

---

## Implementation Plan

### Phase 1: Mesh Projection (Feature 1)
**Why first:** Directly unblocks the failed workflow that motivated this design doc. All other features build on this foundation. Batch line tracing (Feature 5) is used internally.

**Files modified:**
- `ULandscapeService.h` — 2 new USTRUCTs + 3 UFUNCTIONs
- `ULandscapeService.cpp` — implement projection with collision management and line tracing

### Phase 2: Batch Geometry Sampling (Feature 5)
**Why second:** Generic utility used by mesh projection internally. Exposing it as a public API has near-zero marginal cost once the projection internals are written.

**Files modified:**
- `ULandscapeService.h` — 2 UFUNCTIONs
- `ULandscapeService.cpp` — extract internal tracing into public functions

### Phase 3: Terrain Analysis (Feature 3)
**Why third:** The semantic terrain features and auto-painting need slope/height analysis. Building analysis first enables the later features to use it.

**Files modified:**
- `ULandscapeService.h` — 1 new USTRUCT + 5 UFUNCTIONs
- `ULandscapeService.cpp` — gradient computation from height data

### Phase 4: Semantic Terrain Features (Feature 2)
**Why last:** Most complex feature, but also benefits from all prior work (analysis for blending decisions, batch operations for performance). Can be implemented incrementally — start with `create_mountain` and `create_valley`, add others over time.

**Files modified:**
- `ULandscapeService.h` — 2 new USTRUCTs + 8 UFUNCTIONs
- `ULandscapeService.cpp` — procedural generation algorithms

---

## How This Changes the AI Agent Workflow

### Before (Current State)

```
User: "Create a landscape that matches these two mountains"

Agent thinking:
  1. Get mesh positions... need line traces
  2. Create landscape... OK
  3. Sample mesh geometry... 1.4M Python line traces? Too slow.
  4. Fall back to cosine approximation... doesn't match.
  5. ❌ Failed

Agent thinking (alternative):
  1. "Make it look mountainous"
  2. Only tool: set_height_in_region with manual math
  3. Must compute every height value manually
  4. Results look artificial
  5. ❌ Doesn't meet expectations
```

### After (With v3)

```
User: "Create a landscape that matches these two mountains"

Agent thinking:
  1. Create landscape ✅
  2. project_meshes_to_landscape(configs for both meshes) ✅ (single call, <1 second)
  3. apply_erosion() for natural weathering ✅ (single call)
  4. Height/slope blending handled automatically by the landscape material ✅
  5. ✅ Done in 3 calls

User: "Make the terrain more mountainous on the north side"

Agent thinking:
  1. analyze_terrain(north region) → understand current state
  2. create_ridge(path along north, peak_height=15000) ✅
  3. apply_erosion(north region) ✅
  4. ✅ Done in 3 calls, no raw math needed
```

---

## Help System Updates

New help files needed at `Content/Help/landscape/`:

```
# Feature 1: Mesh Projection
project_mesh_to_landscape.md
project_meshes_to_landscape.md
sample_mesh_heights.md

# Feature 2: Semantic Terrain
create_mountain.md
create_valley.md
create_ridge.md
create_plateau.md
apply_erosion.md
create_crater.md
create_terraces.md
blend_terrain_features.md

# Feature 3: Terrain Analysis
analyze_terrain.md
get_slope_at_location.md
get_normal_at_location.md
get_slope_map.md
find_flat_areas.md

# Feature 4: Batch Geometry
batch_line_trace.md
batch_line_trace_grid.md
```

The `ULandscapeService` UCLASS doc comment must be updated to list all ~64 actions.

---

## Open Questions

1. **Should `batch_line_trace` live on a separate GeometryService?** It's not landscape-specific. However, creating a new service adds complexity. Recommend keeping it on LandscapeService for now and moving later if other services need it.

2. **Should erosion support thermal erosion in addition to hydraulic?** Thermal erosion (talus slope simulation) is simpler and produces different effects (scree slopes). Could be a separate `apply_thermal_erosion` or a parameter on `apply_erosion`. Recommend starting with hydraulic only.

3. **Should semantic features support undo?** Currently no landscape operation supports undo (they write directly to the heightmap). This is a broader issue. Recommend deferring to a future "landscape transaction" system.

4. **Performance budget for erosion simulation.** 50K iterations on a 500×500 vertex region takes ~2 seconds in optimized C++. 100K iterations on full landscape (~4M vertices) could take 10+ seconds. Should there be a progress callback or async option? Recommend synchronous with a reasonable default (50K iterations) and document the expected time.

5. **Should `project_mesh_to_landscape` work with skeletal meshes?** Skeletal meshes (characters, animated objects) have different collision shapes. Recommend static mesh only for v3, skeletal mesh in a future version.

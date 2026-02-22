# ULandscapeService v2 — Design Document

## Overview

This document specifies five new feature areas for the VibeUE `ULandscapeService` API. These fill the most critical gaps remaining after the v1 implementation (26 actions covering CRUD, heightmap, sculpting, layers, painting, properties, and visibility).

**Current state:** 26 actions across 7 categories.  
**After this work:** ~42 actions across 10 categories.

---

## Feature 1: Landscape Splines

**Priority: Critical**  
Splines are the standard UE mechanism for roads, rivers, paths, and terrain-following deformation. Without them, users must manually sculpt channels—slow and imprecise.

### UE Engine Classes Used

| Class | Header | Role |
|-------|--------|------|
| `ULandscapeSplinesComponent` | `LandscapeSplinesComponent.h` | Container for all spline control points and segments on a landscape |
| `ULandscapeSplineControlPoint` | `LandscapeSplineControlPoint.h` | A waypoint with location, rotation, width, falloff, layer paint name, and optional mesh |
| `ULandscapeSplineSegment` | `LandscapeSplineSegment.h` | A curve between two control points, with spline meshes and terrain deformation settings |
| `ALandscapeSplineActor` | `LandscapeSplineActor.h` | Standalone spline actor (UE 5.0+) |

### Key Properties on Control Points

```
Location        : FVector      — position in landscape space
Rotation        : FRotator     — tangent direction
Width           : float        — half-width of the spline
SideFalloff     : float        — falloff at the sides
EndFalloff      : float        — falloff at start/end tips
LayerName       : FName        — paint layer to apply under spline
bRaiseTerrain   : bool         — raise ground to spline level
bLowerTerrain   : bool         — lower ground to spline level
Mesh            : UStaticMesh* — optional mesh at control point
MeshScale       : FVector      — scale of the CP mesh
SegmentMeshOffset : float      — vertical offset for segment mesh (e.g. river surface)
```

### Key Properties on Segments

```
Connections[2]  : FLandscapeSplineSegmentConnection — start/end control points + tangent lengths
SplineMeshes    : TArray<FLandscapeSplineMeshEntry> — meshes placed along segment (road surface, etc.)
LayerName       : FName        — paint layer under segment
bRaiseTerrain   : bool         — raise terrain under segment
bLowerTerrain   : bool         — lower terrain under segment
bEnableCollision: bool         — collision on spline meshes
```

### New USTRUCT Types

```cpp
USTRUCT(BlueprintType)
struct FLandscapeSplinePointInfo
{
    UPROPERTY() int32 PointIndex;          // Index within the spline
    UPROPERTY() FVector Location;          // World-space location
    UPROPERTY() FRotator Rotation;         // Tangent rotation
    UPROPERTY() float Width;               // Half-width
    UPROPERTY() float SideFalloff;
    UPROPERTY() float EndFalloff;
    UPROPERTY() FString LayerName;         // Paint layer name (empty = none)
    UPROPERTY() bool bRaiseTerrain;
    UPROPERTY() bool bLowerTerrain;
};

USTRUCT(BlueprintType)
struct FLandscapeSplineSegmentInfo
{
    UPROPERTY() int32 SegmentIndex;
    UPROPERTY() int32 StartPointIndex;
    UPROPERTY() int32 EndPointIndex;
    UPROPERTY() float StartTangentLength;
    UPROPERTY() float EndTangentLength;
    UPROPERTY() FString LayerName;
    UPROPERTY() bool bRaiseTerrain;
    UPROPERTY() bool bLowerTerrain;
};

USTRUCT(BlueprintType)
struct FLandscapeSplineInfo
{
    UPROPERTY() int32 NumControlPoints;
    UPROPERTY() int32 NumSegments;
    UPROPERTY() TArray<FLandscapeSplinePointInfo> ControlPoints;
    UPROPERTY() TArray<FLandscapeSplineSegmentInfo> Segments;
};

USTRUCT(BlueprintType)
struct FSplineCreateResult
{
    UPROPERTY() bool bSuccess;
    UPROPERTY() int32 PointIndex;          // Index of newly created point
    UPROPERTY() FString ErrorMessage;
};
```

### New Actions (8 actions)

#### `create_spline_point`
Create a new control point on the landscape's spline component.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
static FSplineCreateResult CreateSplinePoint(
    const FString& LandscapeNameOrLabel,
    FVector WorldLocation,
    float Width = 500.0f,
    float SideFalloff = 500.0f,
    float EndFalloff = 500.0f,
    const FString& PaintLayerName = TEXT(""),
    bool bRaiseTerrain = true,
    bool bLowerTerrain = true);
```

**Implementation sketch:**
1. Find landscape via `FindLandscapeByIdentifier()`
2. Get or create `ULandscapeSplinesComponent` via `Landscape->GetSplinesComponent()` / `Landscape->CreateSplinesComponent()`
3. Call `SplinesComp->ModifySplines()`
4. Create new `ULandscapeSplineControlPoint` with `NewObject<>(SplinesComp)`
5. Set Location (convert world→landscape space), Width, SideFalloff, EndFalloff, LayerName, bRaiseTerrain, bLowerTerrain
6. Add to `SplinesComp->GetControlPoints()`
7. Call `ControlPoint->UpdateSplinePoints()`
8. Return index

#### `connect_spline_points`
Create a segment between two existing control points.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
static bool ConnectSplinePoints(
    const FString& LandscapeNameOrLabel,
    int32 StartPointIndex,
    int32 EndPointIndex,
    float TangentLength = 0.0f,               // 0 = auto-calculate
    const FString& PaintLayerName = TEXT(""),
    bool bRaiseTerrain = true,
    bool bLowerTerrain = true);
```

**Implementation sketch:**
1. Get splines component, validate point indices
2. Create `ULandscapeSplineSegment` with `NewObject<>(SplinesComp)`
3. Set `Connections[0]` and `Connections[1]` to the two control points
4. If TangentLength == 0, auto-calculate from distance between points
5. Set LayerName, bRaiseTerrain, bLowerTerrain
6. Add segment to `SplinesComp->GetSegments()`
7. Add connection references to both control points' `ConnectedSegments`
8. Call `Segment->UpdateSplinePoints()` and both `ControlPoint->UpdateSplinePoints()`
9. Optionally call `ControlPoint->AutoCalcRotation(true)` for smooth tangents

#### `create_spline_from_points`
High-level convenience: create an entire spline path from an array of world locations. Creates control points and connects them sequentially.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
static FLandscapeSplineInfo CreateSplineFromPoints(
    const FString& LandscapeNameOrLabel,
    const TArray<FVector>& WorldLocations,
    float Width = 500.0f,
    float SideFalloff = 500.0f,
    float EndFalloff = 500.0f,
    const FString& PaintLayerName = TEXT(""),
    bool bRaiseTerrain = true,
    bool bLowerTerrain = true,
    bool bClosedLoop = false);
```

This is the "just draw me a road" action. Internally calls `CreateSplinePoint` + `ConnectSplinePoints` in a loop.

#### `get_spline_info`
Get all control points and segments on a landscape's spline component.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
static FLandscapeSplineInfo GetSplineInfo(
    const FString& LandscapeNameOrLabel);
```

#### `modify_spline_point`
Modify an existing control point's properties.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
static bool ModifySplinePoint(
    const FString& LandscapeNameOrLabel,
    int32 PointIndex,
    FVector WorldLocation,
    float Width = -1.0f,           // -1 = don't change
    float SideFalloff = -1.0f,
    float EndFalloff = -1.0f,
    const FString& PaintLayerName = TEXT("__unchanged__"));
```

#### `delete_spline_point`
Remove a control point and its connected segments.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
static bool DeleteSplinePoint(
    const FString& LandscapeNameOrLabel,
    int32 PointIndex);
```

#### `delete_all_splines`
Clear all control points and segments from the landscape.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
static bool DeleteAllSplines(
    const FString& LandscapeNameOrLabel);
```

#### `apply_splines_to_landscape`
Apply terrain deformation and layer painting for all splines. This triggers UE's built-in spline→landscape rastering.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Splines")
static bool ApplySplinestoLandscape(
    const FString& LandscapeNameOrLabel);
```

**Implementation:** Calls `ULandscapeInfo::ApplySplinesToLandscape()` or iterates spline segments and uses the rasterizer from `LandscapeSplineRaster.h`.

### Python Usage Example

```python
import unreal

svc = unreal.LandscapeService

# Create a road from point A to point B with waypoints
road_points = [
    unreal.Vector(-20000, 0, 500),
    unreal.Vector(-10000, 5000, 600),
    unreal.Vector(0, 0, 700),
    unreal.Vector(10000, -5000, 600),
    unreal.Vector(20000, 0, 500),
]
result = svc.create_spline_from_points(
    "landscape4", road_points,
    width=400.0, side_falloff=300.0,
    paint_layer_name="L2",        # paint dirt under road
    raise_terrain=True, lower_terrain=True)

# Apply to deform terrain
svc.apply_splines_to_landscape("landscape4")
```

---

## Feature 2: Weight Map Import/Export

**Priority: High**  
Heightmap import/export exists but there's no equivalent for layer weight maps. Painting 1,500 brush strokes takes minutes; importing a weight texture would be near-instant.

### UE Engine APIs

- `TAlphamapAccessor<false>` (from `LandscapeEdit.h`) — bulk read/write weight data per-component
- `FScopedSetLandscapeEditingLayer` + `ResolveEditLayerGuid()` — edit layer compatibility (same pattern as the `PaintLayerAtLocation` fix)
- `FLandscapeEditDataInterface::GetWeightDataFast()` / `SetAlphaData()` — component-level bulk access

### New USTRUCT Types

```cpp
USTRUCT(BlueprintType)
struct FWeightMapExportResult
{
    UPROPERTY() bool bSuccess;
    UPROPERTY() FString FilePath;
    UPROPERTY() int32 Width;
    UPROPERTY() int32 Height;
    UPROPERTY() FString ErrorMessage;
};

USTRUCT(BlueprintType)
struct FWeightMapImportResult
{
    UPROPERTY() bool bSuccess;
    UPROPERTY() int32 VerticesModified;
    UPROPERTY() FString ErrorMessage;
};
```

### New Actions (4 actions)

#### `export_weight_map`
Export a single layer's weight data as an 8-bit grayscale PNG.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Painting")
static FWeightMapExportResult ExportWeightMap(
    const FString& LandscapeNameOrLabel,
    const FString& LayerName,
    const FString& OutputFilePath);
```

**Implementation:**
1. Get landscape info for resolution (W×H)
2. Use `TAlphamapAccessor<false>` with edit layer scope to `GetData()` for the layer across all components
3. Build a W×H array of uint8 weights (0-255)
4. Write as 8-bit grayscale PNG using `FImageUtils`

#### `import_weight_map`
Import layer weights from an 8-bit grayscale PNG or RAW file.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Painting")
static FWeightMapImportResult ImportWeightMap(
    const FString& LandscapeNameOrLabel,
    const FString& LayerName,
    const FString& FilePath);
```

**Implementation:**
1. Load image, verify dimensions match landscape resolution
2. Use `TAlphamapAccessor<false>` with edit layer scope
3. Write weight data via `SetData()` with `ELandscapeLayerPaintingRestriction::None`
4. Call `Flush()` then `ForceLayersFullUpdate()` once

#### `get_weights_in_region`
Bulk-read layer weights as a flat array (like `get_height_in_region` but for paint).

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Painting")
static TArray<float> GetWeightsInRegion(
    const FString& LandscapeNameOrLabel,
    const FString& LayerName,
    int32 StartX, int32 StartY,
    int32 SizeX, int32 SizeY);
```

Returns values 0.0–1.0. Row-major, size = SizeX × SizeY.

#### `set_weights_in_region`
Bulk-write layer weights from a flat array.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Painting")
static bool SetWeightsInRegion(
    const FString& LandscapeNameOrLabel,
    const FString& LayerName,
    int32 StartX, int32 StartY,
    int32 SizeX, int32 SizeY,
    const TArray<float>& Weights);
```

Values 0.0–1.0. Internally maps to uint8 0–255. Uses `TAlphamapAccessor<false>` + edit layer scope.

### Python Usage Example

```python
import unreal

svc = unreal.LandscapeService

# Export current grass weights
result = svc.export_weight_map("landscape4", "L1", "C:/Temp/grass_weights.png")

# Import a pre-painted weight map for rock layer
result = svc.import_weight_map("landscape4", "L3", "C:/Temp/rock_mask.png")

# Bulk-read/write for procedural painting
weights = svc.get_weights_in_region("landscape4", "L1", 0, 0, 1009, 1009)
# Modify programmatically...
svc.set_weights_in_region("landscape4", "L1", 0, 0, 1009, 1009, weights)
```

---

## Feature 3: Landscape Holes (Visibility Mask)

**Priority: High**  
UE supports per-vertex visibility via `ALandscapeProxy::VisibilityLayer`. Holes are used for caves, tunnels, basements, and any place where the landscape surface should be missing.

### UE Engine APIs

- `ALandscapeProxy::VisibilityLayer` — static `ULandscapeLayerInfoObject*` representing the visibility layer
- `TAlphamapAccessor<false>` — same accessor used for painting, but targeting the visibility layer
- Weight value 255 = hole (invisible), 0 = solid (visible)

### New Actions (3 actions)

#### `set_hole_at_location`
Punch or fill a circular hole at a world position.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Holes")
static bool SetHoleAtLocation(
    const FString& LandscapeNameOrLabel,
    float WorldX, float WorldY,
    float BrushRadius,
    bool bCreateHole = true);           // true = punch hole, false = fill hole
```

**Implementation:**  
Same as `PaintLayerAtLocation` but targeting `ALandscapeProxy::VisibilityLayer` instead of a named layer. Weight 255 for hole, 0 for fill.

#### `set_hole_in_region`
Set hole visibility for a rectangular region of vertices.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Holes")
static bool SetHoleInRegion(
    const FString& LandscapeNameOrLabel,
    int32 StartX, int32 StartY,
    int32 SizeX, int32 SizeY,
    bool bCreateHole = true);
```

Fills an entire rectangular vertex region as hole or solid.

#### `get_hole_at_location`
Check if a world position is inside a hole.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Holes")
static bool GetHoleAtLocation(
    const FString& LandscapeNameOrLabel,
    float WorldX, float WorldY);
```

Returns true if the vertex at that location is a hole. Reads the visibility layer weight.

### Python Usage Example

```python
import unreal

svc = unreal.LandscapeService

# Create a cave entrance
svc.set_hole_at_location("landscape4", 5000.0, 3000.0, 500.0, create_hole=True)

# Fill it back
svc.set_hole_at_location("landscape4", 5000.0, 3000.0, 500.0, create_hole=False)

# Check if a point is a hole
is_hole = svc.get_hole_at_location("landscape4", 5000.0, 3000.0)
```

---

## Feature 4: Paint Layer in Region (Batch Painting)

**Priority: High**  
The current `paint_layer_at_location` paints one circular brush stroke. Painting an entire landscape requires thousands of calls. A region-based batch paint would be 100x faster.

### New Actions (2 actions)

#### `paint_layer_in_region`
Paint a layer across a rectangular vertex region with uniform weight.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Painting")
static bool PaintLayerInRegion(
    const FString& LandscapeNameOrLabel,
    const FString& LayerName,
    int32 StartX, int32 StartY,
    int32 SizeX, int32 SizeY,
    float Strength = 1.0f);
```

**Implementation:**  
Uses `TAlphamapAccessor<false>` with edit layer scope. Reads current weights, blends target layer up by Strength, writes back. Calls `Flush()` + `ForceLayersFullUpdate()` once at end.

This is essentially `set_weights_in_region` but with blend logic (respects Strength parameter and normalizes all layer weights together).

#### `paint_layer_in_world_rect`
Same as above but using world-space coordinates instead of vertex indices.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape|Painting")
static bool PaintLayerInWorldRect(
    const FString& LandscapeNameOrLabel,
    const FString& LayerName,
    float WorldMinX, float WorldMinY,
    float WorldMaxX, float WorldMaxY,
    float Strength = 1.0f);
```

Internally converts world coords to vertex indices, then calls the same logic as `PaintLayerInRegion`.

### Python Usage Example

```python
import unreal

svc = unreal.LandscapeService

# Paint entire southern half with grass in one call
info = svc.get_landscape_info("landscape4")
half_y = info.resolution_y // 2
svc.paint_layer_in_region("landscape4", "L1", 0, half_y, info.resolution_x, half_y, 1.0)

# Or use world coordinates
svc.paint_layer_in_world_rect("landscape4", "L3", -10000.0, -10000.0, 10000.0, 10000.0, 0.8)
```

---

## Feature 5: Landscape Resize / Retopologize

**Priority: Medium**  
Changing the resolution of an existing landscape after creation. UE doesn't natively support in-place resize, so this must be implemented as export-recreate-reimport.

### New Actions (1 action)

#### `resize_landscape`
Resize a landscape to a new resolution, resampling height and weight data.

```cpp
UFUNCTION(BlueprintCallable, Category = "VibeUE|Landscape")
static FLandscapeCreateResult ResizeLandscape(
    const FString& LandscapeNameOrLabel,
    int32 NewComponentCountX,
    int32 NewComponentCountY,
    int32 NewQuadsPerSection = -1,              // -1 = keep current
    int32 NewSectionsPerComponent = -1);        // -1 = keep current
```

**Implementation (export → recreate → reimport):**
1. Get current landscape info (location, rotation, scale, material, layers, height data, all weight maps)
2. Export height data via `GetHeightInRegion` for full resolution
3. Export all layer weight data via `GetWeightsInRegion` for each layer
4. Calculate new resolution from parameters
5. Bilinear-resample height array to new resolution
6. Bilinear-resample each weight array to new resolution
7. Delete old landscape
8. Create new landscape with new dimensions (same location/rotation/scale)
9. Set material, add layers
10. Import resampled heights via `SetHeightInRegion`
11. Import resampled weights via `SetWeightsInRegion`
12. Return new landscape info

### Caveats
- Splines must be preserved separately and reconnected
- Foliage instances on the old landscape are NOT automatically transferred (user should re-scatter)
- This is a destructive operation — warn users via docstring

### Python Usage Example

```python
import unreal

svc = unreal.LandscapeService

# Double the resolution of a landscape (8x8 → 16x16 components)
result = svc.resize_landscape("landscape4",
    new_component_count_x=16, new_component_count_y=16)
```

---

## Implementation Plan

### Phase 1: Batch Painting (Feature 4)
**Estimated effort: 1 day**  
Lowest risk, highest immediate value. Uses the same `TAlphamapAccessor` pattern we just fixed in `PaintLayerAtLocation`. Directly addresses the performance bottleneck we hit when painting 1,485 strokes.

**Files modified:**
- `ULandscapeService.h` — add 2 UFUNCTION declarations
- `ULandscapeService.cpp` — implement using existing edit layer pattern

### Phase 2: Weight Map Import/Export (Feature 2)
**Estimated effort: 1–2 days**  
Natural extension of Phase 1. Export uses the same `TAlphamapAccessor::GetData()` path; import uses `SetData()`. PNG I/O follows the same pattern as `ExportHeightmap`/`ImportHeightmap`.

**Files modified:**
- `ULandscapeService.h` — add 2 USTRUCTs + 4 UFUNCTION declarations
- `ULandscapeService.cpp` — implement (follow heightmap pattern)

### Phase 3: Landscape Holes (Feature 3)
**Estimated effort: 0.5 day**  
Nearly identical to painting, but targeting `ALandscapeProxy::VisibilityLayer`. Very low risk.

**Files modified:**
- `ULandscapeService.h` — add 3 UFUNCTION declarations
- `ULandscapeService.cpp` — implement (reuse paint pattern)

### Phase 4: Landscape Splines (Feature 1)
**Estimated effort: 3–4 days**  
Largest feature. Requires creating/managing `ULandscapeSplineControlPoint` and `ULandscapeSplineSegment` objects, coordinate space conversion (world ↔ landscape), and triggering the terrain rasterizer.

**Files modified:**
- `ULandscapeService.h` — add 4 USTRUCTs + 8 UFUNCTION declarations
- `ULandscapeService.cpp` — implement all spline operations
- `VibeUE.Build.cs` — add `"LandscapeEditor"` module dependency if needed for spline rasterizer

### Phase 5: Landscape Resize (Feature 5)
**Estimated effort: 1–2 days**  
Composes existing actions (export/create/import) with bilinear resampling. Medium risk due to the destructive nature.

**Files modified:**
- `ULandscapeService.h` — add 1 UFUNCTION declaration
- `ULandscapeService.cpp` — implement using existing helpers + bilinear resample

---

## Help System Updates

Each new action needs a corresponding markdown file at:
```
Content/Help/landscape/<action_name>.md
```

New files needed:
```
Content/Help/landscape/create_spline_point.md
Content/Help/landscape/connect_spline_points.md
Content/Help/landscape/create_spline_from_points.md
Content/Help/landscape/get_spline_info.md
Content/Help/landscape/modify_spline_point.md
Content/Help/landscape/delete_spline_point.md
Content/Help/landscape/delete_all_splines.md
Content/Help/landscape/apply_splines_to_landscape.md
Content/Help/landscape/export_weight_map.md
Content/Help/landscape/import_weight_map.md
Content/Help/landscape/get_weights_in_region.md
Content/Help/landscape/set_weights_in_region.md
Content/Help/landscape/set_hole_at_location.md
Content/Help/landscape/set_hole_in_region.md
Content/Help/landscape/get_hole_at_location.md
Content/Help/landscape/paint_layer_in_region.md
Content/Help/landscape/paint_layer_in_world_rect.md
Content/Help/landscape/resize_landscape.md
```

The docstring on `ULandscapeService` class header must be updated to list all ~42 actions.

---

## Summary

| Feature | Actions | Priority | Est. Effort | Risk |
|---------|---------|----------|-------------|------|
| Landscape Splines | 8 | Critical | 3–4 days | Medium |
| Weight Map Import/Export | 4 | High | 1–2 days | Low |
| Landscape Holes | 3 | High | 0.5 day | Low |
| Batch Painting | 2 | High | 1 day | Low |
| Landscape Resize | 1 | Medium | 1–2 days | Medium |
| **Total** | **18** | | **6.5–9.5 days** | |

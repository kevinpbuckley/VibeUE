---
name: foliage
display_name: Foliage & Vegetation
description: Place and manage foliage instances on landscapes using FoliageService
vibeue_classes:
  - FoliageService
unreal_classes:
  - InstancedFoliageActor
  - FoliageType
  - FoliageType_InstancedStaticMesh
keywords:
  - foliage
  - vegetation
  - trees
  - grass
  - bushes
  - scatter
  - instances
  - forest
  - flora
  - plants
  - shrubs
---

# Foliage & Vegetation Skill

## Critical Rules

### Mesh Path Required

All placement methods require a valid `UStaticMesh` or `UFoliageType` asset path. Use `discover_python_class('unreal.FoliageService')` to see available methods.

### Surface Trace

All scatter/add methods trace downward to find the ground surface. If no surface is hit (e.g., position is outside the landscape bounds), that instance is rejected. Check `instances_rejected` in the result.

### Foliage Type Auto-Creation

When you pass a `UStaticMesh` path, the service auto-creates a transient `UFoliageType` internally. For persistent, reusable foliage types with custom properties, use `create_foliage_type()` first.

### Seed for Reproducibility

Use the `seed` parameter to get reproducible scatter patterns. Same seed + same parameters = same layout. Use `seed=0` for random.

### Layer-Aware Placement Requires Landscape

`scatter_foliage_on_layer()` requires:
1. A valid landscape name/label
2. A painted layer on that landscape
3. Layer weight threshold (0.0-1.0) — only places where weight exceeds threshold

### ⚠️ Common Mistakes to Avoid

| WRONG | CORRECT |
|-------|---------|
| `result.success` | `result.success` (Python auto-converts `bSuccess`) |
| `result.count` | `result.instances_added` |
| `result.instances_placed` | `result.instances_added` (**NOT** `instances_placed`) |
| `ft.asset_path` on VibeUEFoliageTypeInfo | `ft.foliage_type_path` (**NOT** `asset_path`) |
| `scatter_foliage(..., landscape="X")` | `scatter_foliage(..., landscape_name_or_label="X")` |
| `clear_all_foliage()` to clear one landscape | `clear_all_foliage()` removes ALL foliage from the ENTIRE level — use `remove_foliage_in_radius()` for targeted removal |
| Manually scattering foliage on a cloned landscape | If source uses LandscapeGrassType (procedural), just copy paint layers — foliage auto-generates. Check `list_foliage_types()`: if 0, foliage is procedural. |
| Forgetting to check `instances_rejected` | Always check — tells you how many failed surface trace or layer filter |

---

## Return Types - EXACT Properties

### FFoliageScatterResult (from scatter_foliage, scatter_foliage_rect, add_foliage_instances, scatter_foliage_on_layer)

| Property | Type | Description |
|----------|------|-------------|
| `success` | bool | Whether operation succeeded |
| `instances_added` | int | Number of instances actually placed |
| `instances_requested` | int | Number of instances requested |
| `instances_rejected` | int | Number rejected (no surface, slope, layer weight) |
| `error_message` | str | Error details if failed |

### FFoliageRemoveResult (from remove_foliage_in_radius, remove_all_foliage_of_type, clear_all_foliage)

| Property | Type | Description |
|----------|------|-------------|
| `success` | bool | Whether operation succeeded |
| `instances_removed` | int | Number of instances removed |
| `error_message` | str | Error details if failed |

### VibeUEFoliageTypeInfo (from list_foliage_types)

| Property | Type | Description |
|----------|------|-------------|
| `foliage_type_name` | str | Name of the foliage type |
| `mesh_path` | str | Path to the static mesh |
| `instance_count` | int | Number of instances in level |
| `foliage_type_path` | str | Asset path of the foliage type |

### FFoliageTypeCreateResult (from create_foliage_type)

| Property | Type | Description |
|----------|------|-------------|
| `success` | bool | Whether creation succeeded |
| `asset_path` | str | Path to the created foliage type asset |
| `error_message` | str | Error details if failed |

### FFoliageQueryResult (from get_foliage_in_radius)

| Property | Type | Description |
|----------|------|-------------|
| `success` | bool | Whether query succeeded |
| `total_instances` | int | Total matching instances (may exceed returned list) |
| `instances` | Array | List of FFoliageInstanceInfo |
| `error_message` | str | Error details if failed |

### FFoliageInstanceInfo (individual instance data)

| Property | Type | Description |
|----------|------|-------------|
| `location` | Vector | World location |
| `rotation` | Rotator | World rotation |
| `scale` | Vector | Scale (DrawScale3D) |
| `instance_index` | int | Index in the foliage info array |

---

## Workflows

### Quick Scatter Trees

```python
import unreal

result = unreal.FoliageService.scatter_foliage(
    "/Game/Meshes/SM_Tree_01",
    0.0, 0.0,     # center X, Y
    5000.0,        # radius
    200,           # count
    min_scale=0.8,
    max_scale=1.2,
    seed=42
)
if result.success:
    print(f"Placed {result.instances_added}/{result.instances_requested}")
    print(f"Rejected: {result.instances_rejected}")
```

### Create Foliage Type + Scatter

```python
import unreal

# Step 1: Create a reusable foliage type with custom properties
ft = unreal.FoliageService.create_foliage_type(
    mesh_path="/Game/Meshes/SM_Pine_Tree",
    save_path="/Game/Foliage",
    asset_name="FT_PineTree",
    min_scale=0.7,
    max_scale=1.3,
    align_to_normal=True,
    align_to_normal_max_angle=30.0,
    ground_slope_max_angle=40.0,
    cull_distance_max=25000.0
)
if ft.success:
    print(f"Created: {ft.asset_path}")

    # Step 2: Scatter using the foliage type
    result = unreal.FoliageService.scatter_foliage(
        ft.asset_path, 0.0, 0.0, 8000.0, 500, seed=42)
```

### Scatter on Paint Layer

```python
import unreal

# Place grass clumps only where the "Grass" paint layer is dominant
result = unreal.FoliageService.scatter_foliage_on_layer(
    "/Game/Meshes/SM_Grass_Clump",
    "MyTerrain",            # landscape name
    "Grass",                 # layer name
    1000,                    # count
    min_scale=0.5,
    max_scale=1.5,
    layer_weight_threshold=0.6,
    seed=99
)
print(f"Placed {result.instances_added}, rejected {result.instances_rejected}")
```

### Scatter in Rectangle

```python
import unreal

result = unreal.FoliageService.scatter_foliage_rect(
    "/Game/Meshes/SM_Bush",
    -5000.0, -5000.0,  # min X, Y
    5000.0, 5000.0,     # max X, Y
    300,                 # count
    min_scale=0.5,
    max_scale=1.0,
    seed=7
)
```

### Place at Specific Locations

```python
import unreal

locations = [
    unreal.Vector(0, 0, 0),
    unreal.Vector(500, 500, 0),
    unreal.Vector(-500, 500, 0),
    unreal.Vector(500, -500, 0),
    unreal.Vector(-500, -500, 0),
]

result = unreal.FoliageService.add_foliage_instances(
    "/Game/Meshes/SM_Tree_01",
    locations,
    min_scale=1.0,
    max_scale=1.0,
    align_to_normal=True,
    random_yaw=True,
    trace_to_surface=True
)
```

### Multi-Type Forest

```python
import unreal

configs = [
    ("/Game/Meshes/SM_Oak",   300, 0.8, 1.2),
    ("/Game/Meshes/SM_Pine",  200, 0.7, 1.3),
    ("/Game/Meshes/SM_Birch", 100, 0.6, 1.0),
    ("/Game/Meshes/SM_Bush",  400, 0.4, 0.8),
]

for mesh, count, min_s, max_s in configs:
    result = unreal.FoliageService.scatter_foliage(
        mesh, 0.0, 0.0, 10000.0, count,
        min_scale=min_s, max_scale=max_s, seed=42)
    print(f"{mesh}: {result.instances_added} placed")
```

### List and Query

```python
import unreal

# List all foliage types in the level
types = unreal.FoliageService.list_foliage_types()
for ft in types:
    print(f"{ft.foliage_type_name}: {ft.instance_count} instances ({ft.mesh_path})")

# Get count for a specific type
count = unreal.FoliageService.get_instance_count("/Game/Meshes/SM_Tree_01")
print(f"Tree count: {count}")

# Query instances in a region
query = unreal.FoliageService.get_foliage_in_radius(
    "/Game/Meshes/SM_Tree_01", 0.0, 0.0, 2000.0, max_results=50)
for inst in query.instances:
    print(f"  [{inst.instance_index}] at {inst.location}")
```

### Remove Foliage

```python
import unreal

# Remove trees from a building site
result = unreal.FoliageService.remove_foliage_in_radius(
    "/Game/Meshes/SM_Tree_01", 2000.0, 3000.0, 500.0)
print(f"Removed {result.instances_removed}")

# Remove ALL instances of one type
result = unreal.FoliageService.remove_all_foliage_of_type("/Game/Meshes/SM_Bush")

# Nuclear option: remove everything
result = unreal.FoliageService.clear_all_foliage()
print(f"Cleared {result.instances_removed} total instances")
```

### Check Existence

```python
import unreal

# Does the mesh/foliage type exist as an asset?
if unreal.FoliageService.foliage_type_exists("/Game/Meshes/SM_Tree_01"):
    print("Asset exists")

# Are there any instances of this type in the level?
if unreal.FoliageService.has_foliage_instances("/Game/Meshes/SM_Tree_01"):
    print("Instances exist in level")
```

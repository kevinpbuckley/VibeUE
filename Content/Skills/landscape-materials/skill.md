---
name: landscape-materials
display_name: Landscape Material System
description: Create landscape materials with layer blending, texture setup, and layer info objects using LandscapeMaterialService
vibeue_classes:
  - LandscapeMaterialService
  - MaterialService
  - MaterialNodeService
unreal_classes:
  - MaterialExpressionLandscapeLayerBlend
  - MaterialExpressionLandscapeLayerWeight
  - MaterialExpressionLandscapeLayerCoords
  - LandscapeLayerInfoObject
keywords:
  - landscape material
  - layer blend
  - terrain material
  - weight blend
  - layer info
  - terrain texture
---

# Landscape Material System Skill

## Critical Rules

### Two Services Work Together

- **LandscapeMaterialService** - Landscape-specific nodes (LayerBlend, LayerCoords, LayerInfo)
- **MaterialService + MaterialNodeService** - Standard material operations (create, compile, save, connect other nodes)

### Layer Blend Types

| Type | Use |
|------|-----|
| `LB_WeightBlend` | Standard paintable layers (most common) |
| `LB_AlphaBlend` | Non-normalized blending |
| `LB_HeightBlend` | Height-based auto-blending between layers |

### Always Create Layer Info Objects

Each layer name needs a matching `ULandscapeLayerInfoObject` asset:
- Create **before** assigning material to landscape
- Asset name convention: `LI_<LayerName>` (e.g., `LI_Grass`, `LI_Rock`)
- **ALWAYS store and use `.asset_path`** from `create_layer_info_object()` result — never guess paths
- **WRONG**: `"/Game/Landscape/Grass_LayerInfo"`  → **CORRECT**: `grass_info.asset_path` (which is `"/Game/Landscape/LI_Grass"`)

### ⚠️ assign_material_to_landscape Will Silently Skip Bad Paths

If `layer_info_paths` contains wrong paths, `assign_material_to_landscape` may return `True` (material set) but layers will be **empty**. Always verify with `get_landscape_info().layers` after assignment.

### Compile + Save After Changes

```python
unreal.MaterialService.compile_material(mat_path)
unreal.EditorAssetLibrary.save_asset(mat_path)
```

### ⚠️ compile_material Is Slow — Use Separate Code Blocks

`compile_material()` can take **minutes** for landscape materials with blend nodes. **NEVER** put create + compile in the same code block — it will exceed the Python execution timeout.

Split into separate steps:
1. **Block 1**: Create material, add blend nodes, add layers, connect outputs, save
2. **Block 2**: `compile_material(mat_path)` alone
3. **Block 3**: Create layer info objects, assign to landscape

### Texture Tiling

- Landscapes are large; textures need tiling via LandscapeLayerCoords
- Default `MappingScale=0.01` tiles every 100 units
- Smaller values = more tiling, larger values = less tiling

---

## Workflows

### Create Complete Landscape Material

**IMPORTANT: Split into multiple code blocks to avoid timeout!**

**Block 1 — Create and connect (fast):**
```python
import unreal

# 1. Create material
mat = unreal.LandscapeMaterialService.create_landscape_material("M_Terrain", "/Game/Materials")
mat_path = mat.asset_path

# 2. Create layer blend node
blend = unreal.LandscapeMaterialService.create_layer_blend_node(mat_path, -400, 0)

# 3. Add layers
unreal.LandscapeMaterialService.add_layer_to_blend_node(mat_path, blend.node_id, "Grass", "LB_WeightBlend")
unreal.LandscapeMaterialService.add_layer_to_blend_node(mat_path, blend.node_id, "Rock", "LB_WeightBlend")
unreal.LandscapeMaterialService.add_layer_to_blend_node(mat_path, blend.node_id, "Sand", "LB_WeightBlend")

# 4. Connect blend to BaseColor output
unreal.MaterialNodeService.connect_to_output(mat_path, blend.node_id, "", "BaseColor")

# 5. Save (but do NOT compile here — compile is slow!)
unreal.EditorAssetLibrary.save_asset(mat_path)
print(f"Created: {mat_path}")
```

**Block 2 — Compile (slow, run separately):**
```python
import unreal
unreal.MaterialService.compile_material("/Game/Materials/M_Terrain")
print("Compiled")
```

**Block 3 — Layer info objects and assignment:**
```python
import unreal

# Create layer info objects — ALWAYS use .asset_path, naming is LI_<Name>
grass_info = unreal.LandscapeMaterialService.create_layer_info_object("Grass", "/Game/Landscape")
rock_info = unreal.LandscapeMaterialService.create_layer_info_object("Rock", "/Game/Landscape")
sand_info = unreal.LandscapeMaterialService.create_layer_info_object("Sand", "/Game/Landscape")
# grass_info.asset_path = "/Game/Landscape/LI_Grass" (NOT "Grass_LayerInfo")

# Assign material with EXACT paths from create results
unreal.LandscapeMaterialService.assign_material_to_landscape(
    "MyTerrain", "/Game/Materials/M_Terrain",
    {
        "Grass": grass_info.asset_path,
        "Rock": rock_info.asset_path,
        "Sand": sand_info.asset_path
    }
)

# VERIFY layers were added (assign can silently skip bad paths)
info = unreal.LandscapeService.get_landscape_info("MyTerrain")
print(f"Layers: {[l.layer_name for l in info.layers]}")
```

### Setup Layer with Textures

```python
import unreal

mat_path = "/Game/Materials/M_Terrain"
blend_node_id = "..."  # from create_layer_blend_node

# Setup all textures for a layer in one call
unreal.LandscapeMaterialService.setup_layer_textures(
    mat_path,
    blend_node_id,
    "Grass",
    "/Game/Textures/T_Grass_D",      # Diffuse
    "/Game/Textures/T_Grass_N",      # Normal (optional)
    "",                               # Roughness (optional)
    0.01                              # Tiling scale
)

unreal.MaterialService.compile_material(mat_path)
```

### Create Layer Info Objects

```python
import unreal

# Create layer info objects for each layer
grass_info = unreal.LandscapeMaterialService.create_layer_info_object("Grass", "/Game/Landscape", True)
rock_info = unreal.LandscapeMaterialService.create_layer_info_object("Rock", "/Game/Landscape", True)
sand_info = unreal.LandscapeMaterialService.create_layer_info_object("Sand", "/Game/Landscape", True)

print(f"Grass: {grass_info.asset_path}")
print(f"Rock: {rock_info.asset_path}")
print(f"Sand: {sand_info.asset_path}")
```

### Assign Material to Landscape

```python
import unreal

# IMPORTANT: Use EXACT paths from create_layer_info_object().asset_path
# The naming convention is LI_<LayerName>, NOT <LayerName>_LayerInfo
# If paths are wrong, assign returns True but layers will be EMPTY!
unreal.LandscapeMaterialService.assign_material_to_landscape(
    "MyTerrain",
    "/Game/Materials/M_Terrain",
    {
        "Grass": "/Game/Landscape/LI_Grass",
        "Rock": "/Game/Landscape/LI_Rock.LI_Rock",
        "Sand": "/Game/Landscape/LI_Sand.LI_Sand"
    }
)

# Always verify after assignment
info = unreal.LandscapeService.get_landscape_info("MyTerrain")
if not info.layers:
    print("WARNING: No layers found - check layer_info_paths!")
```

### Manual Texture Connection with Layer Coords

```python
import unreal

mat_path = "/Game/Materials/M_Terrain"

# Create UV coords node
coords_id = unreal.LandscapeMaterialService.create_layer_coords_node(mat_path, 0.01, -800, 0)

# Create texture sample manually
tex_id = unreal.MaterialNodeService.create_expression(mat_path, "TextureSample", -600, 0)
unreal.MaterialNodeService.set_expression_property(mat_path, tex_id.id, "Texture", "/Game/Textures/T_Grass_D")

# Connect UV to texture
unreal.MaterialNodeService.connect_expressions(mat_path, coords_id, "", tex_id.id, "UVs")

# Connect texture to blend node layer input
unreal.LandscapeMaterialService.connect_to_layer_input(mat_path, tex_id.id, "", blend_node_id, "Grass", "Layer")
```

### Using Layer Weight Nodes (Alternative)

```python
import unreal

mat_path = "/Game/Materials/M_Terrain"

# LandscapeLayerWeight is an alternative to LandscapeLayerBlend
# Use when you need more control over blending logic
grass_weight = unreal.LandscapeMaterialService.create_layer_weight_node(mat_path, "Grass", 1.0, -400, 0)
rock_weight = unreal.LandscapeMaterialService.create_layer_weight_node(mat_path, "Rock", 0.0, -400, 200)

# Connect layer weight inputs and chain them together
# (more manual but more flexible than LayerBlend)
```

### Check Existence

```python
import unreal

if not unreal.LandscapeMaterialService.landscape_material_exists("/Game/Materials/M_Terrain"):
    unreal.LandscapeMaterialService.create_landscape_material("M_Terrain", "/Game/Materials")

if not unreal.LandscapeMaterialService.layer_info_exists("/Game/Landscape/LI_Grass"):
    unreal.LandscapeMaterialService.create_layer_info_object("Grass", "/Game/Landscape")
```

### Create Layer Sample Node

Use `LandscapeLayerSample` to sample a single layer's weight (alternative to LayerBlend/LayerWeight):

```python
import unreal

mat_path = "/Game/Materials/M_Terrain"

# Creates a LandscapeLayerSample node for a specific layer
# Returns the scalar weight of that layer (0-1)
grass_sample = unreal.LandscapeMaterialService.create_layer_sample_node(
    mat_path, "Grass", -600, 0
)

# Use the sample weight to drive blending or masking
mult_id = unreal.MaterialNodeService.create_expression(mat_path, "Multiply", -400, 0)
unreal.MaterialNodeService.connect_expressions(mat_path, grass_sample, "", mult_id, "A")
```

### Create Grass Output Node

Create a `LandscapeGrassOutput` node for procedural foliage placement:

```python
import unreal

mat_path = "/Game/Materials/M_Terrain"

# Create grass output with grass type mappings
# Keys are display names, values are ULandscapeGrassType asset paths
grass_id = unreal.LandscapeMaterialService.create_grass_output(
    mat_path,
    {
        "Grass": "/Game/Foliage/GT_Grass",
        "Flowers": "/Game/Foliage/GT_Flowers"
    },
    -200, 400
)

# Connect layer weights to the grass output inputs
# Each grass type gets an input pin matching its name
unreal.MaterialNodeService.connect_expressions(mat_path, grass_weight_id, "", grass_id, "Grass")
unreal.MaterialNodeService.connect_expressions(mat_path, flower_weight_id, "", grass_id, "Flowers")

unreal.MaterialService.compile_material(mat_path)
```

### Complete End-to-End Workflow

```python
import unreal

# 1. Create landscape material
mat = unreal.LandscapeMaterialService.create_landscape_material("M_Terrain", "/Game/Materials")
mat_path = mat.asset_path

# 2. Create blend node and add layers
blend = unreal.LandscapeMaterialService.create_layer_blend_node(mat_path)
unreal.LandscapeMaterialService.add_layer_to_blend_node(mat_path, blend.node_id, "Grass")
unreal.LandscapeMaterialService.add_layer_to_blend_node(mat_path, blend.node_id, "Rock")

# 3. Setup textures for each layer
unreal.LandscapeMaterialService.setup_layer_textures(
    mat_path, blend.node_id, "Grass", "/Game/Textures/T_Grass_D", "/Game/Textures/T_Grass_N")
unreal.LandscapeMaterialService.setup_layer_textures(
    mat_path, blend.node_id, "Rock", "/Game/Textures/T_Rock_D", "/Game/Textures/T_Rock_N")

# 4. Connect blend to material output
unreal.MaterialNodeService.connect_to_output(mat_path, blend.node_id, "", "BaseColor")

# 5. Compile and save
unreal.MaterialService.compile_material(mat_path)
unreal.EditorAssetLibrary.save_asset(mat_path)

# 6. Create layer info objects — ALWAYS use .asset_path result
grass_info = unreal.LandscapeMaterialService.create_layer_info_object("Grass", "/Game/Landscape")
rock_info = unreal.LandscapeMaterialService.create_layer_info_object("Rock", "/Game/Landscape")
# grass_info.asset_path = "/Game/Landscape/LI_Grass"

# 7. Create landscape
ls = unreal.LandscapeService.create_landscape(
    unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), unreal.Vector(100, 100, 100),
    1, 63, 8, 8, "MyTerrain")

# 8. Assign material with layer info
unreal.LandscapeMaterialService.assign_material_to_landscape(
    ls.actor_label, mat_path,
    {"Grass": grass_info.asset_path, "Rock": rock_info.asset_path})

print("Landscape setup complete!")
```

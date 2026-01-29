---
name: materials
display_name: Material System
description: Create and edit materials and material instances using MaterialService and MaterialNodeService
vibeue_classes:
  - MaterialService
  - MaterialNodeService
unreal_classes:
  - EditorAssetLibrary
keywords:
  - material
  - shader
  - expression
  - node
  - parameter
  - texture
---

# Material System Skill

## Critical Rules

### ⚠️ Two Services

- **MaterialService** - Create materials, instances, manage properties
- **MaterialNodeService** - Build material graphs, expressions, parameters

### ⚠️ Compile After Graph Changes

```python
unreal.MaterialNodeService.create_parameter(path, "Vector", "BaseColor", ...)
unreal.MaterialNodeService.connect_to_output(path, node_id, "", "BaseColor")
unreal.MaterialService.compile_material(path)  # REQUIRED
unreal.EditorAssetLibrary.save_asset(path)
```

### ⚠️ Parameter Types

| Type | Use | Example Value |
|------|-----|---------------|
| `Scalar` | Single float | `0.5` |
| `Vector` | Color/vector | `(R=1.0,G=0.0,B=0.0,A=1.0)` |
| `Texture` | Texture parameter | `/Game/T_Diffuse.T_Diffuse` |

### ⚠️ Material Output Names

- `BaseColor`, `Metallic`, `Specular`, `Roughness`
- `EmissiveColor`, `Normal`, `Opacity`, `OpacityMask`
- `WorldPositionOffset`, `AmbientOcclusion`

### ⚠️ Check Node Existence

```python
# WRONG - crashes if not found
add_node = next((n for n in nodes if "Add" in n.display_name))

# CORRECT
add_node = next((n for n in nodes if "Add" in n.display_name), None)
if add_node:
    pins = unreal.MaterialNodeService.get_expression_pins(mat_path, add_node.id)
```

### ⚠️ Property Names

Use `discover_python_class()` first:
- `MaterialExpressionTypeInfo` uses `display_name`, NOT `name`
- `MaterialOutputConnectionInfo` uses `connected_expression_id`, NOT `expression_id`

---

## Workflows

### Create Basic Material

```python
import unreal

path = unreal.MaterialService.create_material("Character", "/Game/Materials/")

# Add color parameter
node_id = unreal.MaterialNodeService.create_parameter(path, "Vector", "BaseColor", "Surface", "", -500, 0)
unreal.MaterialNodeService.connect_to_output(path, node_id, "", "BaseColor")

# Add roughness
rough_id = unreal.MaterialNodeService.create_parameter(path, "Scalar", "Roughness", "Surface", "0.5", -500, 100)
unreal.MaterialNodeService.connect_to_output(path, rough_id, "", "Roughness")

unreal.MaterialService.compile_material(path)
unreal.EditorAssetLibrary.save_asset(path)
```

### Create Material Instance

```python
import unreal

instance = unreal.MaterialService.create_instance("/Game/Materials/M_Character", "PlayerRed", "/Game/Materials/")
unreal.MaterialService.set_instance_vector_parameter(instance, "BaseColor", 1.0, 0.0, 0.0, 1.0)
unreal.MaterialService.set_instance_scalar_parameter(instance, "Roughness", 0.3)
unreal.EditorAssetLibrary.save_asset(instance)
```

### Add Texture Parameter

```python
import unreal

tex_id = unreal.MaterialNodeService.create_parameter(path, "Texture", "DiffuseMap", "Textures", "", -500, 0)
unreal.MaterialNodeService.connect_to_output(path, tex_id, "", "BaseColor")
unreal.MaterialService.compile_material(path)
```

### Create Math Expression

```python
import unreal

path = "/Game/Materials/M_Tint"

color_id = unreal.MaterialNodeService.create_parameter(path, "Vector", "TintColor", "Surface", "", -600, 0)
mult_id = unreal.MaterialNodeService.create_expression(path, "Multiply", -300, 0)
intensity_id = unreal.MaterialNodeService.create_parameter(path, "Scalar", "Intensity", "Surface", "1.0", -600, 100)

unreal.MaterialNodeService.connect_expressions(path, color_id, "", mult_id, "A")
unreal.MaterialNodeService.connect_expressions(path, intensity_id, "", mult_id, "B")
unreal.MaterialNodeService.connect_to_output(path, mult_id, "", "BaseColor")
unreal.MaterialService.compile_material(path)
```

### Set Material Properties

```python
import unreal

unreal.MaterialService.set_blend_mode(path, "Translucent")  # For transparency
unreal.MaterialService.set_shading_model(path, "DefaultLit")
unreal.MaterialService.set_two_sided(path, True)
unreal.MaterialService.compile_material(path)
```

### Get Material Info

```python
import unreal

info = unreal.MaterialService.get_material_info("/Game/Materials/M_Character")
if info:
    print(f"Material: {info.name}, Blend Mode: {info.blend_mode}")
```

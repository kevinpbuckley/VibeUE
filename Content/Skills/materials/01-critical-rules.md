# Material Critical Rules

---

## ⚠️ CRITICAL: Two Services

- **MaterialService** - Create materials, instances, manage properties
- **MaterialNodeService** - Build material graphs, expressions, parameters

---

## ⚠️ CRITICAL: Compile After Graph Changes

```python
import unreal

# Make graph changes
unreal.MaterialNodeService.create_parameter(path, "Vector", "BaseColor", ...)
unreal.MaterialNodeService.connect_to_material(path, node_id, "BaseColor")

# MUST compile
unreal.MaterialService.compile_material(path)

# Then save
unreal.EditorAssetLibrary.save_asset(path)
```

---

## ⚠️ CRITICAL: Parameter Types

| Type | Use | Example Value |
|------|-----|---------------|
| `Scalar` | Single float | `0.5` |
| `Vector` | Color/vector | `(R=1.0,G=0.0,B=0.0,A=1.0)` |
| `Texture` | Texture parameter | `/Game/T_Diffuse.T_Diffuse` |
| `StaticBool` | Compile-time toggle | `True` |

---

## ⚠️ CRITICAL: Material Outputs

Common material output names for `connect_to_material()`:

- `BaseColor` - Albedo/diffuse color
- `Metallic` - Metallic factor (0-1)
- `Specular` - Specular factor (0-1)
- `Roughness` - Roughness factor (0-1)
- `EmissiveColor` - Emissive/glow
- `Opacity` - Alpha (needs Blend Mode)
- `OpacityMask` - Masked opacity (needs Blend Mode)
- `Normal` - Normal map
- `WorldPositionOffset` - Vertex offset
- `AmbientOcclusion` - AO factor

---

## ⚠️ CRITICAL: Expression Position

Nodes need X,Y positions. Negative X = left of output:

```python
# Left side of graph (inputs)
unreal.MaterialNodeService.create_parameter(path, "Vector", "Color", "Surface", "", -500, 0)

# Middle
unreal.MaterialNodeService.create_expression(path, "Multiply", -300, 0)

# Material output is at X=0
```

---

## ⚠️ CRITICAL: Blend Modes for Transparency

To use Opacity, set blend mode first:

```python
# For opacity
unreal.MaterialService.set_blend_mode(path, "Translucent")

# For opacity mask
unreal.MaterialService.set_blend_mode(path, "Masked")
```

---

## ⚠️ CRITICAL: Material Instance Parent

Creating instances requires a valid parent material:

```python
instance_path = unreal.MaterialService.create_instance(
    "/Game/Materials/M_Base",  # Parent material path
    "Character",               # Instance name
    "/Game/Materials/"         # Destination folder
)
```

---

## ⚠️ CRITICAL: Set Instance Parameters

Use type-specific methods:

```python
# Scalar
unreal.MaterialService.set_scalar_parameter(instance_path, "Roughness", 0.5)

# Vector (color)
unreal.MaterialService.set_vector_parameter(instance_path, "BaseColor", "(R=1,G=0,B=0,A=1)")

# Texture
unreal.MaterialService.set_texture_parameter(instance_path, "Diffuse", "/Game/T_Tex.T_Tex")
```

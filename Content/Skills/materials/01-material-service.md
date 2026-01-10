# MaterialService API Reference

All methods are called via `unreal.MaterialService.<method_name>(...)`.

**ALWAYS use `discover_python_class("unreal.MaterialService")` for parameter details before calling.**

---

## Lifecycle

### create_material(name, path)
Create a new material asset.

**Returns:** Full material path (str)

**Example:**
```python
import unreal

path = unreal.MaterialService.create_material("Character", "/Game/Materials/")
print(f"Created: {path}")  # "/Game/Materials/M_Character"
```

### create_instance(parent_path, name, dest_path)
Create material instance from parent material.

**Returns:** Material instance path (str)

**Example:**
```python
import unreal

mi_path = unreal.MaterialService.create_instance(
    "/Game/Materials/M_Character",
    "MI_Character_Red",
    "/Game/Materials/"
)
print(f"Created instance: {mi_path}")
```

### save_material(path)
Save material to disk.

### compile_material(path)
Compile/rebuild material shaders.

**Example:**
```python
import unreal

unreal.MaterialService.compile_material("/Game/Materials/M_Character")
```

### refresh_editor(path)
Refresh open Material Editor.

### open_in_editor(path)
Open material in Material Editor.

---

## Information

### get_material_info(path)
Get comprehensive material info (domain, blend mode, shading model, parameters).

**Returns:** MaterialInfo struct

**Example:**
```python
import unreal

info = unreal.MaterialService.get_material_info("/Game/Materials/M_Character")
print(f"Domain: {info.material_domain}")
print(f"Blend Mode: {info.blend_mode}")
print(f"Shading Model: {info.shading_model}")
print(f"Parameters: {info.parameter_count}")
```

### summarize(path)
Get AI-friendly material summary.

**Returns:** str

### list_properties(path, include_advanced=False)
List all editable properties.

**Returns:** Array of property info

**Example:**
```python
import unreal

props = unreal.MaterialService.list_properties("/Game/Materials/M_Character", False)
for prop in props:
    print(f"{prop.name}: {prop.type}")
```

### get_property(path, property_name)
Get a property value as string.

**Returns:** str or None

### get_property_info(path, property_name)
Get detailed property metadata.

**Returns:** PropertyInfo or None

---

## Property Management

### set_property(path, property_name, value)
Set a material property.

**Example:**
```python
import unreal

# Set blend mode
unreal.MaterialService.set_property("/Game/Materials/M_Glass", "BlendMode", "Translucent")

# Set two-sided
unreal.MaterialService.set_property("/Game/Materials/M_Foliage", "TwoSided", "true")

# Set shading model
unreal.MaterialService.set_property("/Game/Materials/M_Skin", "ShadingModel", "Subsurface")
```

### set_properties(path, properties_map)
Set multiple properties at once.

**Example:**
```python
import unreal

props = {
    "BlendMode": "Translucent",
    "TwoSided": "true",
    "OpacityMaskClipValue": "0.5"
}
unreal.MaterialService.set_properties("/Game/Materials/M_Glass", props)
```

---

## Parameter Management

### list_parameters(path)
List all material parameters (scalar, vector, texture).

**Returns:** Array of parameter info

**Example:**
```python
import unreal

params = unreal.MaterialService.list_parameters("/Game/Materials/M_Character")
for param in params:
    print(f"{param.name} ({param.type}): {param.default_value}")
```

### get_parameter(path, param_name)
Get parameter value and info.

**Returns:** ParameterInfo or None

### set_parameter_default(path, param_name, value)
Set parameter default value.

**Example:**
```python
import unreal

# Set scalar parameter default
unreal.MaterialService.set_parameter_default("/Game/Materials/M_Character", "Roughness", "0.5")

# Set vector parameter default (format: R,G,B,A)
unreal.MaterialService.set_parameter_default("/Game/Materials/M_Character", "BaseColor", "1.0,0.0,0.0,1.0")
```

---

## Instance Information

### get_instance_info(path)
Get material instance details (parent, parameter overrides).

**Returns:** MaterialInstanceInfo struct

**Example:**
```python
import unreal

info = unreal.MaterialService.get_instance_info("/Game/Materials/MI_Character_Red")
print(f"Parent: {info.parent_material}")
print(f"Scalar overrides: {len(info.scalar_parameters)}")
print(f"Vector overrides: {len(info.vector_parameters)}")
```

### list_instance_properties(path, include_advanced=False)
List instance editable properties.

**Returns:** Array of property info

### get_instance_property(path, property_name)
Get instance property value.

**Returns:** str or None

### set_instance_property(path, property_name, value)
Set instance property.

---

## Instance Parameters

### list_instance_parameters(path)
List parameter overrides with status (whether overridden or using parent default).

**Returns:** Array of instance parameter info

**Example:**
```python
import unreal

params = unreal.MaterialService.list_instance_parameters("/Game/Materials/MI_Character_Red")
for param in params:
    print(f"{param.name}: Overridden={param.is_overridden}, Value={param.value}")
```

### set_instance_scalar_parameter(path, name, value)
Set scalar parameter override.

**Example:**
```python
import unreal

unreal.MaterialService.set_instance_scalar_parameter(
    "/Game/Materials/MI_Character_Red",
    "Roughness",
    0.8
)
```

### set_instance_vector_parameter(path, name, r, g, b, a)
Set vector/color parameter override.

**Example:**
```python
import unreal

# Set red color
unreal.MaterialService.set_instance_vector_parameter(
    "/Game/Materials/MI_Character_Red",
    "BaseColor",
    1.0, 0.0, 0.0, 1.0
)
```

### set_instance_texture_parameter(path, name, texture_path)
Set texture parameter override.

**Example:**
```python
import unreal

unreal.MaterialService.set_instance_texture_parameter(
    "/Game/Materials/MI_Character_Red",
    "DiffuseTexture",
    "/Game/Textures/T_Character_Red"
)
```

### clear_instance_parameter_override(path, name)
Clear parameter override (revert to parent default).

**Example:**
```python
import unreal

# Revert Roughness to parent default
unreal.MaterialService.clear_instance_parameter_override(
    "/Game/Materials/MI_Character_Red",
    "Roughness"
)
```

### save_instance(path)
Save material instance to disk.

---

## Common Property Values

### Blend Modes
- "Opaque" - Solid, non-transparent
- "Masked" - Binary transparency (fully opaque or fully transparent)
- "Translucent" - Allows varying transparency
- "Additive" - Additive blending
- "Modulate" - Multiplicative blending

### Shading Models
- "DefaultLit" - Standard PBR lighting
- "Unlit" - No lighting calculations
- "Subsurface" - Subsurface scattering (skin, wax)
- "PreintegratedSkin" - Optimized skin rendering
- "ClearCoat" - Car paint, lacquered surfaces
- "TwoSidedFoliage" - Leaves and foliage

### Material Domains
- "Surface" - Standard 3D surface
- "DeferredDecal" - Decal material
- "LightFunction" - Light function material
- "PostProcess" - Post-process material
- "UserInterface" - UI material

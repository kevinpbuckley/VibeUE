# Material Management Guide

## Overview

The `manage_material` tool provides comprehensive material lifecycle, property, and parameter management for both base Materials and Material Instance Constants (MICs).

## Key Concepts

### Base Materials vs Material Instances

| Type | Asset Type | Use Case |
|------|------------|----------|
| Base Material | `UMaterial` | Define shader logic, nodes, parameters |
| Material Instance | `UMaterialInstanceConstant` | Override parent parameters, no shader recompile |

### Path Parameters

- **material_path**: For base material operations
- **instance_path**: For material instance operations
- **value**: Property/parameter value to set (string format)

## Base Material Actions

### Lifecycle Actions

```python
# Create a new material
manage_material(
    action="create",
    destination_path="/Game/Materials",
    material_name="M_NewMaterial",
    initial_properties={"TwoSided": "true", "BlendMode": "Translucent"}
)

# Save material to disk
manage_material(action="save", material_path="/Game/Materials/M_MyMaterial")

# Recompile shaders
manage_material(action="compile", material_path="/Game/Materials/M_MyMaterial")
```

### Information Actions

```python
# Get comprehensive material info
manage_material(action="get_info", material_path="/Game/Materials/M_MyMaterial")

# List all editable properties
manage_material(action="list_properties", material_path="/Game/Materials/M_MyMaterial", include_advanced=True)

# Get single property value
manage_material(action="get_property", material_path="/Game/Materials/M_MyMaterial", property_name="TwoSided")
```

### Property Actions

```python
# Set single property
manage_material(
    action="set_property",
    material_path="/Game/Materials/M_MyMaterial",
    property_name="TwoSided",
    value="true"
)

# Set multiple properties
manage_material(
    action="set_properties",
    material_path="/Game/Materials/M_MyMaterial",
    properties={"TwoSided": "true", "BlendMode": "Masked"}
)
```

### Parameter Actions

```python
# List all parameters (from expression nodes)
manage_material(action="list_parameters", material_path="/Game/Materials/M_MyMaterial")

# Set parameter default value
manage_material(
    action="set_parameter_default",
    material_path="/Game/Materials/M_MyMaterial",
    parameter_name="Roughness",
    value="0.5"
)
```

## Material Instance Actions

### Create Instance

```python
manage_material(
    action="create_instance",
    parent_material_path="/Game/Materials/M_MyMaterial",
    destination_path="/Game/Materials/Instances",
    instance_name="MI_MyMaterial_Red",
    scalar_parameters={"Roughness": 0.8},
    vector_parameters={"BaseColor": [1.0, 0.0, 0.0, 1.0]}
)
```

### Instance Information

```python
# Get instance info (parent, overrides, etc.)
manage_material(action="get_instance_info", instance_path="/Game/Materials/MI_MyMaterial_Red")

# List instance properties (PhysMaterial, etc.)
manage_material(action="list_instance_properties", instance_path="/Game/Materials/MI_MyMaterial_Red")

# List parameter overrides
manage_material(action="list_instance_parameters", instance_path="/Game/Materials/MI_MyMaterial_Red")
```

### Instance Property Actions

```python
# Get instance property
manage_material(
    action="get_instance_property",
    instance_path="/Game/Materials/MI_MyMaterial_Red",
    property_name="PhysMaterial"
)

# Set instance property (e.g., PhysMaterial)
manage_material(
    action="set_instance_property",
    instance_path="/Game/Materials/MI_MyMaterial_Red",
    property_name="PhysMaterial",
    value="/Game/PhysicalMaterials/PM_Metal"
)
```

### Instance Parameter Actions

```python
# Set scalar parameter
manage_material(
    action="set_instance_scalar_parameter",
    instance_path="/Game/Materials/MI_MyMaterial_Red",
    parameter_name="Roughness",
    value="0.5"
)

# Set vector/color parameter (RGBA 0.0-1.0)
manage_material(
    action="set_instance_vector_parameter",
    instance_path="/Game/Materials/MI_MyMaterial_Red",
    parameter_name="BaseColor",
    r=0.0, g=1.0, b=0.0, a=1.0
)

# Set texture parameter
manage_material(
    action="set_instance_texture_parameter",
    instance_path="/Game/Materials/MI_MyMaterial_Red",
    parameter_name="DiffuseTexture",
    texture_path="/Game/Textures/T_Custom"
)

# Clear parameter override (revert to parent)
manage_material(
    action="clear_instance_parameter_override",
    instance_path="/Game/Materials/MI_MyMaterial_Red",
    parameter_name="Roughness"
)

# Save instance
manage_material(action="save_instance", instance_path="/Game/Materials/MI_MyMaterial_Red")
```

## Common Properties

### Rendering Properties

| Property | Type | Description |
|----------|------|-------------|
| TwoSided | bool | Render both faces |
| BlendMode | enum | Opaque, Masked, Translucent, Additive, Modulate |
| MaterialDomain | enum | Surface, DeferredDecal, LightFunction, Volume, PostProcess, UI |
| ShadingModel | enum | DefaultLit, Unlit, Subsurface, ClearCoat, etc. |

### Physical Material Properties (Material Instances)

| Property | Type | Description |
|----------|------|-------------|
| PhysMaterial | object | Physical material for sounds/effects |
| PhysMaterialMask | object | Physical material mask |
| PhysicalMaterialMap | object | Physical material map for masks |

## Complete Workflow Example

```python
# 1. Search for materials
manage_asset(action="search", asset_type="Material", search_term="M_Base")

# 2. Get material info
manage_material(action="get_info", material_path="/Game/Materials/M_Base")

# 3. Create material instance
manage_material(
    action="create_instance",
    parent_material_path="/Game/Materials/M_Base",
    destination_path="/Game/Materials/Instances",
    instance_name="MI_Base_Variant"
)

# 4. Override parameters
manage_material(
    action="set_instance_scalar_parameter",
    instance_path="/Game/Materials/Instances/MI_Base_Variant",
    parameter_name="Metallic",
    value="1.0"
)

# 5. Set physical material
manage_material(
    action="set_instance_property",
    instance_path="/Game/Materials/Instances/MI_Base_Variant",
    property_name="PhysMaterial",
    value="/Game/PhysicalMaterials/PM_Metal"
)

# 6. Save the instance
manage_material(action="save_instance", instance_path="/Game/Materials/Instances/MI_Base_Variant")
```

## Troubleshooting

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| "Material not found" | Invalid path | Use `manage_asset(action="search")` to find correct path |
| "Property not found" | Wrong property name | Use `list_properties` or `list_instance_properties` first |
| "Parameter not found" | Parameter doesn't exist | Use `list_parameters` or `list_instance_parameters` first |
| "value is required" | Missing value parameter | Ensure `value` parameter is provided |

### Tips

1. **Always search first**: Use `manage_asset(action="search")` to find exact paths
2. **Check properties**: Use `list_*_properties` to discover available properties
3. **Check parameters**: Use `list_*_parameters` before setting parameter values
4. **Save after changes**: Use `save` or `save_instance` to persist modifications

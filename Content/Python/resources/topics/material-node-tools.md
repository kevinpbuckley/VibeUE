# Material Node Tools Reference

## Overview

The `manage_material_node` tool provides complete control over material graph expressions (nodes). Unlike Blueprint nodes, material expressions define shader logic for rendering.

**Key Difference from Blueprint Nodes:**
- Blueprint nodes: Logic execution flow
- Material nodes: Shader calculation flow (parallel, stateless)
- Material graphs do NOT support split/recombine pins - use ComponentMask and AppendVector instead

## Quick Reference: Complete Material Node Workflow

```python
# 1. DISCOVER available expression types
types = manage_material_node(
    action="discover_types",
    material_path="/Game/Materials/M_MyMaterial",
    category="Math",
    search_term="Add"
)

# 2. CREATE expression nodes
constant = manage_material_node(
    action="create",
    material_path="/Game/Materials/M_MyMaterial",
    expression_class="Constant",
    pos_x=0,
    pos_y=0
)

add = manage_material_node(
    action="create",
    material_path="/Game/Materials/M_MyMaterial",
    expression_class="Add",
    pos_x=200,
    pos_y=0
)

# 3. CONFIGURE expression properties
manage_material_node(
    action="set_property",
    material_path="/Game/Materials/M_MyMaterial",
    expression_id=constant["expression_id"],
    property_name="R",
    value="0.5"
)

# 4. CONNECT expressions
manage_material_node(
    action="connect",
    material_path="/Game/Materials/M_MyMaterial",
    source_expression_id=constant["expression_id"],
    source_output="",  # First output
    target_expression_id=add["expression_id"],
    target_input="A"
)

# 5. CONNECT to material output
manage_material_node(
    action="connect_to_output",
    material_path="/Game/Materials/M_MyMaterial",
    expression_id=add["expression_id"],
    output_name="",  # First output
    material_property="BaseColor"
)

# 6. COMPILE and SAVE
manage_material(action="compile", material_path="/Game/Materials/M_MyMaterial")
manage_material(action="save", material_path="/Game/Materials/M_MyMaterial")
```

## Actions by Category

### Discovery Actions

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| `discover_types` | Find available expression types | `category`, `search_term`, `max_results` |
| `get_categories` | List expression categories | (none) |

```python
# Discover expression types
manage_material_node(
    action="discover_types",
    material_path="/Game/Materials/M_MyMaterial",
    category="Math"
)

# Get all categories
manage_material_node(action="get_categories")
```

### Expression Lifecycle Actions

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| `create` | Create new expression | `expression_class`, `pos_x`, `pos_y` |
| `delete` | Remove expression | `expression_id` |
| `move` | Reposition expression | `expression_id`, `pos_x`, `pos_y` |

```python
# Create expression
manage_material_node(
    action="create",
    material_path="/Game/Materials/M_MyMaterial",
    expression_class="TextureSample",
    pos_x=-300,
    pos_y=0
)

# Move expression
manage_material_node(
    action="move",
    material_path="/Game/Materials/M_MyMaterial",
    expression_id="MaterialExpressionAdd_0x12345678",
    pos_x=400,
    pos_y=200
)

# Delete expression
manage_material_node(
    action="delete",
    material_path="/Game/Materials/M_MyMaterial",
    expression_id="MaterialExpressionAdd_0x12345678"
)
```

### Information Actions

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| `list` | List all expressions in material | (none) |
| `get_details` | Get detailed expression info | `expression_id` |
| `get_pins` | Get all pins for expression | `expression_id` |

```python
# List all expressions
manage_material_node(
    action="list",
    material_path="/Game/Materials/M_MyMaterial"
)

# Get expression details
manage_material_node(
    action="get_details",
    material_path="/Game/Materials/M_MyMaterial",
    expression_id="MaterialExpressionAdd_0x12345678"
)

# Get expression pins
manage_material_node(
    action="get_pins",
    material_path="/Game/Materials/M_MyMaterial",
    expression_id="MaterialExpressionAdd_0x12345678"
)
```

### Connection Actions

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| `connect` | Connect two expressions | `source_expression_id`, `target_expression_id`, `target_input` |
| `disconnect` | Disconnect input | `expression_id`, `input_name` |
| `list_connections` | List all connections | (none) |
| `connect_to_output` | Connect to material output | `expression_id`, `material_property` |
| `disconnect_output` | Disconnect material output | `material_property` |

```python
# Connect expressions
manage_material_node(
    action="connect",
    material_path="/Game/Materials/M_MyMaterial",
    source_expression_id="MaterialExpressionConstant_0x12345",
    source_output="",  # First output
    target_expression_id="MaterialExpressionAdd_0x67890",
    target_input="A"
)

# Connect to material output
manage_material_node(
    action="connect_to_output",
    material_path="/Game/Materials/M_MyMaterial",
    expression_id="MaterialExpressionMultiply_0x12345",
    output_name="",  # First output
    material_property="BaseColor"
)

# List all connections
manage_material_node(
    action="list_connections",
    material_path="/Game/Materials/M_MyMaterial"
)
```

### Property Actions

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| `list_properties` | List editable properties | `expression_id` |
| `get_property` | Get property value | `expression_id`, `property_name` |
| `set_property` | Set property value | `expression_id`, `property_name`, `value` |

```python
# List expression properties
manage_material_node(
    action="list_properties",
    material_path="/Game/Materials/M_MyMaterial",
    expression_id="MaterialExpressionConstant_0x12345"
)

# Set property value
manage_material_node(
    action="set_property",
    material_path="/Game/Materials/M_MyMaterial",
    expression_id="MaterialExpressionConstant_0x12345",
    property_name="R",
    value="0.75"
)
```

### Material Output Actions

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| `get_output_properties` | List available outputs | (none) |
| `get_output_connections` | Get current output connections | (none) |

```python
# Get available material outputs
manage_material_node(
    action="get_output_properties",
    material_path="/Game/Materials/M_MyMaterial"
)
# Returns: BaseColor, Metallic, Roughness, Normal, etc.

# Get current output connections
manage_material_node(
    action="get_output_connections",
    material_path="/Game/Materials/M_MyMaterial"
)
```

### Parameter Actions

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| `create_parameter` | Create parameter expression | `parameter_type`, `parameter_name`, `default_value` |
| `promote_to_parameter` | Convert constant to parameter | `expression_id`, `parameter_name` |
| `set_parameter_metadata` | Set parameter group/priority | `expression_id`, `group_name`, `sort_priority` |

```python
# Create a scalar parameter
manage_material_node(
    action="create_parameter",
    material_path="/Game/Materials/M_MyMaterial",
    parameter_type="Scalar",
    parameter_name="Roughness",
    group_name="Surface",
    default_value="0.5",
    pos_x=-200,
    pos_y=100
)

# Promote existing constant to parameter
manage_material_node(
    action="promote_to_parameter",
    material_path="/Game/Materials/M_MyMaterial",
    expression_id="MaterialExpressionConstant_0x12345",
    parameter_name="RoughnessValue",
    group_name="Surface"
)

# Set parameter metadata
manage_material_node(
    action="set_parameter_metadata",
    material_path="/Game/Materials/M_MyMaterial",
    expression_id="MaterialExpressionScalarParameter_0x12345",
    group_name="Surface",
    sort_priority=10
)
```

## Common Expression Types

### Constants
| Expression | Description |
|------------|-------------|
| `Constant` | Single float value |
| `Constant2Vector` | 2D vector (UV) |
| `Constant3Vector` | RGB color |
| `Constant4Vector` | RGBA color |

### Math
| Expression | Description |
|------------|-------------|
| `Add` | A + B |
| `Subtract` | A - B |
| `Multiply` | A * B |
| `Divide` | A / B |
| `Lerp` (LinearInterpolate) | Blend between A and B |
| `OneMinus` | 1 - X |
| `Power` | Base ^ Exponent |
| `Clamp` | Clamp between Min/Max |

### Texture
| Expression | Description |
|------------|-------------|
| `TextureSample` | Sample texture at UV |
| `TextureObject` | Texture reference |
| `TextureCoordinate` | UV coordinates |
| `Panner` | Animate UVs |

### Parameters
| Expression | Description |
|------------|-------------|
| `ScalarParameter` | Float parameter (exposed to instances) |
| `VectorParameter` | Color/vector parameter |
| `TextureSampleParameter2D` | Texture parameter |
| `StaticBoolParameter` | Compile-time switch |

### Utility
| Expression | Description |
|------------|-------------|
| `ComponentMask` | Extract RGB/A channels |
| `AppendVector` | Combine channels |
| `Fresnel` | View-angle effect |
| `Time` | Animated time value |

## Material Output Properties

| Property | Type | Description |
|----------|------|-------------|
| BaseColor | RGB | Surface color |
| Metallic | Scalar | 0=non-metal, 1=metal |
| Specular | Scalar | Non-metallic specular |
| Roughness | Scalar | 0=smooth, 1=rough |
| EmissiveColor | RGB | Glow/emission |
| Normal | RGB | Normal map |
| Opacity | Scalar | Transparency (Translucent only) |
| OpacityMask | Scalar | Cutout (Masked only) |
| WorldPositionOffset | XYZ | Vertex displacement |

## Complete PBR Material Example

```python
# Create a complete PBR material with parameters

material_path = "/Game/Materials/M_PBR"

# 1. Create parameters for Material Instance overrides
roughness = manage_material_node(
    action="create_parameter",
    material_path=material_path,
    parameter_type="Scalar",
    parameter_name="Roughness",
    default_value="0.5",
    group_name="Surface",
    pos_x=-400, pos_y=0
)

metallic = manage_material_node(
    action="create_parameter",
    material_path=material_path,
    parameter_type="Scalar",
    parameter_name="Metallic",
    default_value="0.0",
    group_name="Surface",
    pos_x=-400, pos_y=100
)

base_color = manage_material_node(
    action="create_parameter",
    material_path=material_path,
    parameter_type="Vector",
    parameter_name="BaseColor",
    default_value="1.0,0.5,0.0,1.0",  # Orange
    group_name="Color",
    pos_x=-400, pos_y=200
)

# 2. Connect to material outputs
manage_material_node(
    action="connect_to_output",
    material_path=material_path,
    expression_id=roughness["expression_id"],
    material_property="Roughness"
)

manage_material_node(
    action="connect_to_output",
    material_path=material_path,
    expression_id=metallic["expression_id"],
    material_property="Metallic"
)

manage_material_node(
    action="connect_to_output",
    material_path=material_path,
    expression_id=base_color["expression_id"],
    material_property="BaseColor"
)

# 3. Compile and save
manage_material(action="compile", material_path=material_path)
manage_material(action="save", material_path=material_path)

# 4. Create instance with overrides
manage_material(
    action="create_instance",
    parent_material_path=material_path,
    destination_path="/Game/Materials/Instances",
    instance_name="MI_PBR_Red",
    scalar_parameters={"Roughness": 0.8, "Metallic": 0.0},
    vector_parameters={"BaseColor": [1.0, 0.0, 0.0, 1.0]}
)
```

## Troubleshooting

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| "Material not found" | Invalid path | Use `manage_asset(action="search", asset_type="Material")` |
| "Expression not found" | Invalid expression_id | Use `list` action to get valid IDs |
| "Property not found" | Wrong property name | Use `list_properties` first |
| "Cannot connect" | Incompatible types | Check pin types with `get_pins` |

### Tips

1. **Always list first**: Use `list` and `get_details` before modifying
2. **Use discover_types**: Find exact expression class names
3. **Compile after changes**: Material graphs need shader recompilation
4. **Save your work**: Use `manage_material(action="save")` to persist
5. **Position matters**: Use pos_x/pos_y for readable graph layouts

## Related Topics

- `get_help(topic="material-management")` - Material properties and instances
- `get_help(topic="asset-discovery")` - Finding materials in project

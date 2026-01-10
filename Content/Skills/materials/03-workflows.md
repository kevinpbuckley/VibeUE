# Material Workflows

Common patterns for working with Materials and Material Instances.

---

## Create Material with Parameters

```python
import unreal

# 1. Create base material
mat_path = unreal.MaterialService.create_material("Character", "/Game/Materials/")

# 2. Create parameter expressions
base_color_id = unreal.MaterialNodeService.create_parameter(
    mat_path,
    "Vector",
    "BaseColor",
    "Surface",
    "1.0,0.8,0.6,1.0",  # Default skin tone
    -500, 0
)

roughness_id = unreal.MaterialNodeService.create_parameter(
    mat_path,
    "Scalar",
    "Roughness",
    "Surface",
    "0.5",
    -500, 200
)

metallic_id = unreal.MaterialNodeService.create_parameter(
    mat_path,
    "Scalar",
    "Metallic",
    "Surface",
    "0.0",
    -500, 400
)

# 3. Connect to material outputs
unreal.MaterialNodeService.connect_to_output(mat_path, base_color_id, "", "BaseColor")
unreal.MaterialNodeService.connect_to_output(mat_path, roughness_id, "", "Roughness")
unreal.MaterialNodeService.connect_to_output(mat_path, metallic_id, "", "Metallic")

# 4. Compile and save
unreal.MaterialService.compile_material(mat_path)
unreal.MaterialService.save_material(mat_path)

print(f"Created material: {mat_path}")
```

---

## Build Material Graph

```python
import unreal

mat_path = "/Game/Materials/M_Test"

# 1. Create material if it doesn't exist
existing = unreal.AssetDiscoveryService.find_asset_by_path(mat_path)
if not existing:
    mat_path = unreal.MaterialService.create_material("Test", "/Game/Materials/")

# 2. Discover available expression types
types = unreal.MaterialNodeService.discover_types("", "Constant", 10)
for t in types:
    print(f"Available: {t.class_name}")

# 3. Create expressions
const_color = unreal.MaterialNodeService.create_expression(
    mat_path,
    "MaterialExpressionConstant3Vector",
    -400, 0
)

multiply = unreal.MaterialNodeService.create_expression(
    mat_path,
    "MaterialExpressionMultiply",
    -200, 0
)

scalar = unreal.MaterialNodeService.create_expression(
    mat_path,
    "MaterialExpressionConstant",
    -400, 200
)

# 4. Set expression properties
unreal.MaterialNodeService.set_expression_property(mat_path, const_color, "R", "1.0")
unreal.MaterialNodeService.set_expression_property(mat_path, const_color, "G", "0.0")
unreal.MaterialNodeService.set_expression_property(mat_path, const_color, "B", "0.0")

unreal.MaterialNodeService.set_expression_property(mat_path, scalar, "R", "0.5")

# 5. Connect expressions
unreal.MaterialNodeService.connect_expressions(mat_path, const_color, "", multiply, "A")
unreal.MaterialNodeService.connect_expressions(mat_path, scalar, "", multiply, "B")

# 6. Connect to material output
unreal.MaterialNodeService.connect_to_output(mat_path, multiply, "", "BaseColor")

# 7. Compile
unreal.MaterialService.compile_material(mat_path)

print("Material graph built successfully")
```

---

## Create Material Instance with Overrides

```python
import unreal

parent_path = "/Game/Materials/M_Character"
mi_path = "/Game/Materials/MI_Character_Red"

# 1. Check if parent exists
parent = unreal.AssetDiscoveryService.find_asset_by_path(parent_path)
if not parent:
    print(f"Parent material not found: {parent_path}")
else:
    # 2. Create instance
    mi_path = unreal.MaterialService.create_instance(
        parent_path,
        "MI_Character_Red",
        "/Game/Materials/"
    )

    # 3. Set parameter overrides
    unreal.MaterialService.set_instance_vector_parameter(
        mi_path,
        "BaseColor",
        1.0, 0.0, 0.0, 1.0  # Red
    )

    unreal.MaterialService.set_instance_scalar_parameter(
        mi_path,
        "Roughness",
        0.6
    )

    unreal.MaterialService.set_instance_scalar_parameter(
        mi_path,
        "Metallic",
        0.1
    )

    # 4. Save instance
    unreal.MaterialService.save_instance(mi_path)

    print(f"Created instance: {mi_path}")
```

---

## Inspect Material Graph

```python
import unreal

mat_path = "/Game/Materials/M_Character"

# 1. Get material info
info = unreal.MaterialService.get_material_info(mat_path)
print(f"Material: {info.name}")
print(f"Domain: {info.material_domain}")
print(f"Blend Mode: {info.blend_mode}")
print(f"Shading Model: {info.shading_model}")

# 2. List expressions
expressions = unreal.MaterialNodeService.list_expressions(mat_path)
print(f"\nExpressions ({len(expressions)}):")
for expr in expressions:
    print(f"  ID {expr.expression_id}: {expr.class_name} at ({expr.pos_x}, {expr.pos_y})")

# 3. List connections
connections = unreal.MaterialNodeService.list_connections(mat_path)
print(f"\nConnections ({len(connections)}):")
for conn in connections:
    print(f"  {conn.source_id}:{conn.source_output} → {conn.target_id}:{conn.target_input}")

# 4. Get output connections
output_conns = unreal.MaterialNodeService.get_output_connections(mat_path)
print(f"\nOutput Connections:")
for conn in output_conns:
    print(f"  {conn.material_property} ← Expression {conn.expression_id}")

# 5. List parameters
params = unreal.MaterialService.list_parameters(mat_path)
print(f"\nParameters ({len(params)}):")
for param in params:
    print(f"  {param.name} ({param.type}): {param.default_value}")
```

---

## Create Textured Material

```python
import unreal

mat_path = "/Game/Materials/M_Textured"

# 1. Create material
mat_path = unreal.MaterialService.create_material("Textured", "/Game/Materials/")

# 2. Create texture parameter
tex_param = unreal.MaterialNodeService.create_parameter(
    mat_path,
    "Texture",
    "DiffuseTexture",
    "Textures",
    "",
    -600, 0
)

# 3. Create texture sample (if you need to manipulate the texture)
# Note: Texture parameters already output the texture, so you can connect directly
# But if you need UV manipulation, create a TextureSample node

tex_coord = unreal.MaterialNodeService.create_expression(
    mat_path,
    "MaterialExpressionTextureCoordinate",
    -800, 100
)

# 4. Create multiply for tiling
multiply = unreal.MaterialNodeService.create_expression(
    mat_path,
    "MaterialExpressionMultiply",
    -600, 100
)

tiling_constant = unreal.MaterialNodeService.create_expression(
    mat_path,
    "MaterialExpressionConstant2Vector",
    -800, 200
)

# Set tiling to 2x2
unreal.MaterialNodeService.set_expression_property(mat_path, tiling_constant, "R", "2.0")
unreal.MaterialNodeService.set_expression_property(mat_path, tiling_constant, "G", "2.0")

# 5. Connect UV manipulation
unreal.MaterialNodeService.connect_expressions(mat_path, tex_coord, "", multiply, "A")
unreal.MaterialNodeService.connect_expressions(mat_path, tiling_constant, "", multiply, "B")

# 6. Connect texture to output
unreal.MaterialNodeService.connect_to_output(mat_path, tex_param, "", "BaseColor")

# 7. Compile
unreal.MaterialService.compile_material(mat_path)
unreal.MaterialService.save_material(mat_path)
```

---

## Modify Material Properties

```python
import unreal

mat_path = "/Game/Materials/M_Glass"

# 1. Get current properties
props = unreal.MaterialService.list_properties(mat_path, False)
print("Available properties:")
for prop in props:
    print(f"  {prop.name}: {prop.type}")

# 2. Set blend mode to translucent
unreal.MaterialService.set_property(mat_path, "BlendMode", "Translucent")

# 3. Enable two-sided rendering
unreal.MaterialService.set_property(mat_path, "TwoSided", "true")

# 4. Set opacity mask clip value
unreal.MaterialService.set_property(mat_path, "OpacityMaskClipValue", "0.5")

# 5. Set multiple properties at once
props_map = {
    "CastDynamicShadowAsMasked": "true",
    "ShadingModel": "DefaultLit"
}
unreal.MaterialService.set_properties(mat_path, props_map)

# 6. Compile
unreal.MaterialService.compile_material(mat_path)
```

---

## Promote Constant to Parameter

```python
import unreal

mat_path = "/Game/Materials/M_Character"

# 1. Find constant expression
expressions = unreal.MaterialNodeService.list_expressions(mat_path)
const_id = None
for expr in expressions:
    if "Constant" in expr.class_name:
        const_id = expr.expression_id
        break

if const_id:
    # 2. Promote to parameter
    unreal.MaterialNodeService.promote_to_parameter(
        mat_path,
        const_id,
        "CustomValue",
        "Custom"
    )

    # 3. Compile
    unreal.MaterialService.compile_material(mat_path)

    print(f"Promoted constant {const_id} to parameter")
```

---

## Clone Material Instance

```python
import unreal

source_path = "/Game/Materials/MI_Character_Red"
dest_path = "/Game/Materials/MI_Character_Blue"

# 1. Duplicate asset
unreal.AssetDiscoveryService.duplicate_asset(source_path, dest_path)

# 2. Modify parameter overrides
unreal.MaterialService.set_instance_vector_parameter(
    dest_path,
    "BaseColor",
    0.0, 0.0, 1.0, 1.0  # Blue
)

# 3. Save
unreal.MaterialService.save_instance(dest_path)

print(f"Cloned and modified: {dest_path}")
```

---

## Best Practices

1. **Always compile after graph changes**
   ```python
   unreal.MaterialService.compile_material(path)
   ```

2. **Use parameters for artist-tweakable values**
   ```python
   # Good - Parameter
   unreal.MaterialNodeService.create_parameter(path, "Scalar", "Roughness", "Surface", "0.5", x, y)

   # Less flexible - Constant
   # Artists can't tweak without opening Material Editor
   ```

3. **Check if material exists before creating**
   ```python
   existing = unreal.AssetDiscoveryService.find_asset_by_path(path)
   if not existing:
       # Create material
   ```

4. **Get expression pins before connecting**
   ```python
   pins = unreal.MaterialNodeService.get_expression_pins(mat_path, expr_id)
   # Inspect pin names before calling connect_expressions
   ```

5. **Save materials after modifications**
   ```python
   unreal.MaterialService.save_material(path)
   # Or save all assets
   unreal.AssetDiscoveryService.save_all_assets()
   ```

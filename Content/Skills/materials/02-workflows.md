# Material Workflows

---

## Create Basic Material

```python
import unreal

# Create material
path = unreal.MaterialService.create_material("Character", "/Game/Materials/")

# Add color parameter
node_id = unreal.MaterialNodeService.create_parameter(
    path, "Vector", "BaseColor", "Surface", "", -500, 0
)

# Connect to output
unreal.MaterialNodeService.connect_to_output(path, node_id, "", "BaseColor")

# Add roughness parameter
rough_id = unreal.MaterialNodeService.create_parameter(
    path, "Scalar", "Roughness", "Surface", "0.5", -500, 100
)
unreal.MaterialNodeService.connect_to_output(path, rough_id, "", "Roughness")

# Compile and save
unreal.MaterialService.compile_material(path)
unreal.EditorAssetLibrary.save_asset(path)
```

---

## Create Material Instance

```python
import unreal

# Create instance from parent
instance = unreal.MaterialService.create_instance(
    "/Game/Materials/M_Character",  # Parent
    "PlayerRed",                    # Name
    "/Game/Materials/"              # Folder
)

# Override parameters
unreal.MaterialService.set_instance_vector_parameter(instance, "BaseColor", 1.0, 0.0, 0.0, 1.0)
unreal.MaterialService.set_instance_scalar_parameter(instance, "Roughness", 0.3)

# Save
unreal.EditorAssetLibrary.save_asset(instance)
```

---

## Add Texture Parameter

```python
import unreal

path = "/Game/Materials/M_Character"

# Create texture parameter
tex_id = unreal.MaterialNodeService.create_parameter(
    path, "Texture", "DiffuseMap", "Textures", "", -500, 0
)

# Connect to BaseColor
unreal.MaterialNodeService.connect_to_output(path, tex_id, "", "BaseColor")

# Compile
unreal.MaterialService.compile_material(path)
```

---

## Create Math Expression

```python
import unreal

path = "/Game/Materials/M_Tint"

# Color parameter
color_id = unreal.MaterialNodeService.create_parameter(
    path, "Vector", "TintColor", "Surface", "", -600, 0
)

# Multiply expression
mult_id = unreal.MaterialNodeService.create_expression(path, "Multiply", -300, 0)

# Connect: Color → Multiply.A
unreal.MaterialNodeService.connect_expressions(path, color_id, "", mult_id, "A")

# Scalar intensity
intensity_id = unreal.MaterialNodeService.create_parameter(
    path, "Scalar", "Intensity", "Surface", "1.0", -600, 100
)

# Connect: Intensity → Multiply.B
unreal.MaterialNodeService.connect_expressions(path, intensity_id, "", mult_id, "B")

# Connect: Multiply → BaseColor
unreal.MaterialNodeService.connect_to_output(path, mult_id, "", "BaseColor")

# Compile
unreal.MaterialService.compile_material(path)
```

---

## Set Material Properties

```python
import unreal

path = "/Game/Materials/M_Glass"

# Set blend mode for transparency
unreal.MaterialService.set_blend_mode(path, "Translucent")

# Set shading model
unreal.MaterialService.set_shading_model(path, "DefaultLit")

# Set two-sided
unreal.MaterialService.set_two_sided(path, True)

# Compile
unreal.MaterialService.compile_material(path)
```

---

## Get Material Info

```python
import unreal

# Get material info
info = unreal.MaterialService.get_material_info("/Game/Materials/M_Character")
if info:
    print(f"Material: {info.name}")
    print(f"Blend Mode: {info.blend_mode}")
    print(f"Shading Model: {info.shading_model}")
    print(f"Parameters: {info.parameter_count}")
```
---

## Working with Expression Discovery (Error-Free Pattern)

```python
import unreal

mat_path = "/Game/Materials/M_Test"

# 1. Discover expression types first
# Use discover_python_class to see struct properties before accessing them
add_types = unreal.MaterialNodeService.discover_types(search_term="Add", max_results=5)

# 2. Safely access struct properties (use display_name, NOT name)
print("Available Add nodes:")
for expr_type in add_types:
    # CORRECT: MaterialExpressionTypeInfo has 'display_name' property
    print(f"  {expr_type.display_name} ({expr_type.class_name})")

# 3. List existing expressions and safely access properties
expressions = unreal.MaterialNodeService.list_expressions(mat_path)

# 4. Safe pattern: check existence before operations
add_node = next((e for e in expressions if "Add" in e.display_name), None)
if add_node:
    # Safe to use add_node.id now
    pins = unreal.MaterialNodeService.get_expression_pins(mat_path, add_node.id)
    print(f"Add node pins: {[p.name for p in pins]}")
else:
    # Create if doesn't exist
    add_node = unreal.MaterialNodeService.create_expression(mat_path, "MaterialExpressionAdd", -200, 0)
    print(f"Created Add node: {add_node.id}")

# 5. Verify output connections (use correct property names)
connections = unreal.MaterialNodeService.get_output_connections(mat_path)
for conn in connections:
    if conn.is_connected:
        # CORRECT: use 'connected_expression_id', NOT 'expression_id'
        print(f"{conn.property_name} connected to {conn.connected_expression_id}")
```
# MaterialNodeService API Reference

All methods are called via `unreal.MaterialNodeService.<method_name>(...)`.

**ALWAYS use `discover_python_class("unreal.MaterialNodeService")` for parameter details before calling.**

---

## Discovery

### discover_types(category="", search_term="", max_results=50)
Find available material expression types.

**Returns:** Array of expression type info with class names

**Example:**
```python
import unreal

# Find all constant types
constants = unreal.MaterialNodeService.discover_types("", "Constant", 10)
for expr in constants:
    print(f"{expr.name}: {expr.class_name}")

# Find math operations
math_ops = unreal.MaterialNodeService.discover_types("Math", "", 20)
```

### get_categories()
Get all expression categories.

**Returns:** Array[str] of category names

**Example:**
```python
import unreal

categories = unreal.MaterialNodeService.get_categories()
for cat in categories:
    print(cat)
# Output: "Math", "Textures", "Constants", "Parameters", "Utility", etc.
```

---

## Lifecycle

### create_expression(material_path, expression_class, pos_x, pos_y)
Create a new expression in the material graph.

**Returns:** Expression ID (int)

**Example:**
```python
import unreal

# Create constant
const_id = unreal.MaterialNodeService.create_expression(
    "/Game/Materials/M_Test",
    "MaterialExpressionConstant3Vector",
    -400, 0
)

# Create texture sample
tex_id = unreal.MaterialNodeService.create_expression(
    "/Game/Materials/M_Test",
    "MaterialExpressionTextureSample",
    -600, 0
)

print(f"Created expressions: {const_id}, {tex_id}")
```

### delete_expression(material_path, expression_id)
Delete an expression from the material.

**Returns:** bool

### move_expression(material_path, expression_id, pos_x, pos_y)
Move expression to new position.

---

## Information

### list_expressions(material_path)
List all expressions in material with IDs and positions.

**Returns:** Array of expression info

**Example:**
```python
import unreal

expressions = unreal.MaterialNodeService.list_expressions("/Game/Materials/M_Test")
for expr in expressions:
    print(f"ID {expr.expression_id}: {expr.class_name} at ({expr.pos_x}, {expr.pos_y})")
```

### get_expression_details(material_path, expression_id)
Get detailed expression info (class, pins, properties, connections).

**Returns:** ExpressionDetailInfo or None

**Example:**
```python
import unreal

details = unreal.MaterialNodeService.get_expression_details("/Game/Materials/M_Test", 5)
if details:
    print(f"Expression: {details.class_name}")
    print(f"Inputs: {len(details.input_pins)}")
    print(f"Outputs: {len(details.output_pins)}")
```

### get_expression_pins(material_path, expression_id)
Get all pins (inputs/outputs) for an expression.

**Returns:** ExpressionPinsInfo with input_pins and output_pins arrays

**Example:**
```python
import unreal

pins = unreal.MaterialNodeService.get_expression_pins("/Game/Materials/M_Test", 5)
print("Inputs:")
for pin in pins.input_pins:
    print(f"  {pin.name} ({pin.type})")
print("Outputs:")
for pin in pins.output_pins:
    print(f"  {pin.name} ({pin.type})")
```

---

## Connections

### connect_expressions(material_path, source_id, source_output, target_id, target_input)
Connect two expressions together.

**Parameters:**
- `source_output`: Output pin name (use "" for default output)
- `target_input`: Input pin name

**Returns:** bool

**Example:**
```python
import unreal

# Connect texture (ID 1) to multiply node (ID 2)
unreal.MaterialNodeService.connect_expressions(
    "/Game/Materials/M_Test",
    1,    # Source expression ID
    "",   # Default output
    2,    # Target expression ID
    "A"   # Input pin name
)

# Connect with named output
unreal.MaterialNodeService.connect_expressions(
    "/Game/Materials/M_Test",
    3,              # Source ID
    "RGB",          # Named output
    4,              # Target ID
    "BaseColor"     # Input name
)
```

### disconnect_input(material_path, expression_id, input_name)
Disconnect an input pin.

**Returns:** bool

### list_connections(material_path)
List all connections in material.

**Returns:** Array of connection info

**Example:**
```python
import unreal

connections = unreal.MaterialNodeService.list_connections("/Game/Materials/M_Test")
for conn in connections:
    print(f"{conn.source_id}:{conn.source_output} → {conn.target_id}:{conn.target_input}")
```

### connect_to_output(material_path, expression_id, output_name, material_property)
Connect expression to material output (BaseColor, Roughness, etc.).

**Parameters:**
- `output_name`: Expression output pin name (use "" for default)
- `material_property`: "BaseColor", "Metallic", "Roughness", "Normal", "Emissive", "Opacity", etc.

**Returns:** bool

**Example:**
```python
import unreal

# Connect expression 5 to Base Color output
unreal.MaterialNodeService.connect_to_output(
    "/Game/Materials/M_Test",
    5,              # Expression ID
    "",             # Default output
    "BaseColor"     # Material output
)

# Connect to multiple outputs
unreal.MaterialNodeService.connect_to_output(mat_path, 10, "", "Roughness")
unreal.MaterialNodeService.connect_to_output(mat_path, 11, "", "Metallic")
```

### disconnect_output(material_path, material_property)
Disconnect material output.

**Returns:** bool

---

## Properties

### get_expression_property(material_path, expression_id, property_name)
Get expression property value.

**Returns:** str or None

### set_expression_property(material_path, expression_id, property_name, value)
Set expression property value.

**Returns:** bool

**Example:**
```python
import unreal

# Set constant value (MaterialExpressionConstant)
unreal.MaterialNodeService.set_expression_property(
    "/Game/Materials/M_Test",
    5,          # Expression ID
    "R",        # Property name
    "0.5"       # Value
)

# Set texture reference (MaterialExpressionTextureSample)
unreal.MaterialNodeService.set_expression_property(
    "/Game/Materials/M_Test",
    8,
    "Texture",
    "/Game/Textures/T_Character"
)
```

### list_expression_properties(material_path, expression_id)
List all editable properties for an expression.

**Returns:** Array of property info

**Example:**
```python
import unreal

props = unreal.MaterialNodeService.list_expression_properties("/Game/Materials/M_Test", 5)
for prop in props:
    print(f"{prop.name}: {prop.type} = {prop.value}")
```

---

## Parameters

### create_parameter(material_path, parameter_type, parameter_name, group_name, default_value, pos_x, pos_y)
Create a parameter expression.

**Parameters:**
- `parameter_type`: "Scalar", "Vector", "Texture"
- `group_name`: Parameter group (e.g., "Surface", "Details")
- `default_value`: Default value as string

**Returns:** Expression ID (int)

**Example:**
```python
import unreal

# Create scalar parameter
roughness_id = unreal.MaterialNodeService.create_parameter(
    "/Game/Materials/M_Test",
    "Scalar",           # Type
    "Roughness",        # Name
    "Surface",          # Group
    "0.5",              # Default value
    -500, 0             # Position
)

# Create vector parameter for color
color_id = unreal.MaterialNodeService.create_parameter(
    "/Game/Materials/M_Test",
    "Vector",
    "BaseColor",
    "Surface",
    "1.0,1.0,1.0,1.0",  # R,G,B,A
    -500, 100
)

# Create texture parameter
tex_id = unreal.MaterialNodeService.create_parameter(
    "/Game/Materials/M_Test",
    "Texture",
    "DiffuseTexture",
    "Textures",
    "",                 # No default value needed
    -700, 0
)
```

### promote_to_parameter(material_path, expression_id, parameter_name, group_name)
Promote a constant expression to a parameter.

**Returns:** bool

**Example:**
```python
import unreal

# Promote constant to parameter
unreal.MaterialNodeService.promote_to_parameter(
    "/Game/Materials/M_Test",
    5,                  # Constant expression ID
    "CustomValue",      # Parameter name
    "Custom"            # Group name
)
```

### set_parameter_metadata(material_path, expression_id, group_name, sort_priority)
Set parameter group and sort priority.

**Returns:** bool

---

## Material Outputs

### get_output_properties(material_path)
Get available material output properties (BaseColor, Roughness, etc.).

**Returns:** Array[str] of output property names

**Example:**
```python
import unreal

outputs = unreal.MaterialNodeService.get_output_properties("/Game/Materials/M_Test")
for output in outputs:
    print(output)
# Output: "BaseColor", "Metallic", "Roughness", "Normal", "Emissive", etc.
```

### get_output_connections(material_path)
Get current material output connections.

**Returns:** Array of output connection info

**Example:**
```python
import unreal

connections = unreal.MaterialNodeService.get_output_connections("/Game/Materials/M_Test")
for conn in connections:
    print(f"{conn.material_property} connected to expression {conn.expression_id}")
```

---

## Common Expression Classes

### Constants
- `MaterialExpressionConstant` - Single float value
- `MaterialExpressionConstant2Vector` - 2D vector (UV coordinates)
- `MaterialExpressionConstant3Vector` - 3D vector (RGB color)
- `MaterialExpressionConstant4Vector` - 4D vector (RGBA color)

### Textures
- `MaterialExpressionTextureSample` - Sample a texture
- `MaterialExpressionTextureCoordinate` - UV coordinates

### Math
- `MaterialExpressionAdd` - Add two values
- `MaterialExpressionSubtract` - Subtract
- `MaterialExpressionMultiply` - Multiply
- `MaterialExpressionDivide` - Divide
- `MaterialExpressionPower` - Power (exponent)
- `MaterialExpressionClamp` - Clamp between min/max
- `MaterialExpressionLerp` - Linear interpolation

### Utility
- `MaterialExpressionAppendVector` - Combine channels
- `MaterialExpressionComponentMask` - Extract channels (R, G, B, A)
- `MaterialExpressionFresnel` - Fresnel effect
- `MaterialExpressionIf` - Conditional logic

### Parameters
- `MaterialExpressionScalarParameter` - Exposed scalar
- `MaterialExpressionVectorParameter` - Exposed vector/color
- `MaterialExpressionTextureObjectParameter` - Exposed texture

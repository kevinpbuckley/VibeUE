# FColor Property Testing Guide

## Overview
This guide provides comprehensive tests for setting FColor properties across all VibeUE contexts:
- Blueprint Components (SpotLight, PointLight, etc.)
- Blueprint Variables
- Level Actors (Light actors)
- Materials (Color parameters)
- UMG Widgets (Note: Uses FLinearColor, not FColor)

## ⚠️ CRITICAL: FColor vs FLinearColor

**FColor** (used in lights, some materials):
- **Range**: 0-255 (byte values)
- **Format**: JSON object `{"R": 255, "G": 0, "B": 0, "A": 255}` OR array `[255, 0, 0, 255]`
- **Order**: RGB (Red, Green, Blue, Alpha)
- **Example**: `{"R": 0, "G": 0, "B": 255, "A": 255}` = Blue

**FLinearColor** (used in UMG widgets, some materials):
- **Range**: 0.0-1.0 (normalized floats)
- **Format**: JSON object `{"R": 1.0, "G": 0.0, "B": 0.0, "A": 1.0}` OR array `[1.0, 0.0, 0.0, 1.0]`
- **Order**: RGB (Red, Green, Blue, Alpha)
- **Example**: `{"R": 0.0, "G": 0.0, "B": 1.0, "A": 1.0}` = Blue

---

## Test 1: Blueprint Component - SpotLight LightColor

### Setup
```python
# Create a test blueprint with a SpotLight component
manage_blueprint(
    action="create",
    blueprint_name="/Game/Test/BP_ColorTest",
    parent_class="/Script/Engine.Actor"
)

# Add SpotLight component
manage_blueprint_component(
    action="create",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    component_class="/Script/Engine.SpotLightComponent"
)
```

### Test Cases

#### Test 1.1: Set LightColor using JSON object format (0-255)
```python
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="LightColor",
    property_value={"R": 0, "G": 0, "B": 255, "A": 255}  # Blue
)
# Expected: Success, light should be blue
```

#### Test 1.2: Set LightColor using array format (0-255)
```python
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="LightColor",
    property_value=[255, 0, 0, 255]  # Red (RGB order)
)
# Expected: Success, light should be red
```

#### Test 1.3: Set LightColor with 3-element array (A defaults to 255)
```python
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="LightColor",
    property_value=[0, 255, 0]  # Green, alpha defaults to 255
)
# Expected: Success, light should be green
```

#### Test 1.4: Verify LightColor was set correctly
```python
result = manage_blueprint_component(
    action="get_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="LightColor"
)
# Expected: Returns color values, verify they match what was set
```

#### Test 1.5: Common Colors Reference
```python
# Red
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="LightColor",
    property_value={"R": 255, "G": 0, "B": 0, "A": 255}
)

# Green
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="LightColor",
    property_value={"R": 0, "G": 255, "B": 0, "A": 255}
)

# Blue
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="LightColor",
    property_value={"R": 0, "G": 0, "B": 255, "A": 255}
)

# Yellow (Red + Green)
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="LightColor",
    property_value={"R": 255, "G": 255, "B": 0, "A": 255}
)

# White
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="LightColor",
    property_value={"R": 255, "G": 255, "B": 255, "A": 255}
)

# Orange (RGB: 255, 165, 0)
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="LightColor",
    property_value={"R": 255, "G": 165, "B": 0, "A": 255}
)
```

---

## Test 2: Blueprint Component - PointLight LightColor

### Setup
```python
manage_blueprint_component(
    action="create",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestPointLight",
    component_class="/Script/Engine.PointLightComponent"
)
```

### Test Cases

#### Test 2.1: Set PointLight LightColor
```python
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestPointLight",
    property_name="LightColor",
    property_value=[255, 192, 203, 255]  # Pink (array format)
)
# Expected: Success
```

---

## Test 3: Blueprint Variable - FColor Variable

### Setup
```python
# Create a Blueprint variable of type FColor
manage_blueprint_variable(
    action="create",
    blueprint_name="/Game/Test/BP_ColorTest",
    variable_name="MyColor",
    variable_type="struct:Color"  # FColor struct type
)
```

### Test Cases

#### Test 3.1: Set FColor variable using JSON object
```python
manage_blueprint_variable(
    action="set_value",
    blueprint_name="/Game/Test/BP_ColorTest",
    variable_name="MyColor",
    variable_value={"R": 128, "G": 0, "B": 128, "A": 255}  # Purple
)
# Expected: Success
```

#### Test 3.2: Set FColor variable using array
```python
manage_blueprint_variable(
    action="set_value",
    blueprint_name="/Game/Test/BP_ColorTest",
    variable_name="MyColor",
    variable_value=[255, 20, 147, 255]  # Deep pink
)
# Expected: Success
```

#### Test 3.3: Get FColor variable value
```python
result = manage_blueprint_variable(
    action="get_value",
    blueprint_name="/Game/Test/BP_ColorTest",
    variable_name="MyColor"
)
# Expected: Returns the color value that was set
```

---

## Test 4: Level Actor - SpotLight Actor LightColor

### Setup
```python
# Create a SpotLight actor in the level
manage_level_actors(
    action="add",
    actor_class="/Script/Engine.SpotLight",
    location=[0, 0, 300],
    actor_label="TestSpotLight"
)
```

### Test Cases

#### Test 4.1: Set LightColor on level actor using string format
```python
manage_level_actors(
    action="set_property",
    actor_label="TestSpotLight",
    property_path="LightComponent0.LightColor",
    property_value="(R=0,G=255,B=0,A=255)"  # Green (string format for level actors)
)
# Expected: Success
```

#### Test 4.2: Set LightColor using different colors
```python
# Cyan
manage_level_actors(
    action="set_property",
    actor_label="TestSpotLight",
    property_path="LightComponent0.LightColor",
    property_value="(R=0,G=255,B=255,A=255)"
)

# Magenta
manage_level_actors(
    action="set_property",
    actor_label="TestSpotLight",
    property_path="LightComponent0.LightColor",
    property_value="(R=255,G=0,B=255,A=255)"
)
```

#### Test 4.3: Verify LightColor was set
```python
result = manage_level_actors(
    action="get_property",
    actor_label="TestSpotLight",
    property_path="LightComponent0.LightColor"
)
# Expected: Returns color in format like (R=0,G=255,B=0,A=255)
```

---

## Test 5: Material - Color Parameter (FColor or FLinearColor)

### Setup
```python
# Create a material
manage_material(
    action="create",
    material_name="/Game/Test/M_ColorTest"
)

# Add a VectorParameter (can be used for color)
manage_material_node(
    action="create_parameter",
    material_name="/Game/Test/M_ColorTest",
    parameter_name="BaseColor",
    parameter_type="VectorParameter"
)
```

### Test Cases

#### Test 5.1: Set Material Color Parameter (FLinearColor - normalized 0-1)
```python
# Note: Material parameters typically use FLinearColor (0-1 range)
manage_material(
    action="set_parameter",
    material_name="/Game/Test/M_ColorTest",
    parameter_name="BaseColor",
    parameter_value=[1.0, 0.0, 0.0, 1.0]  # Red (normalized)
)
# Expected: Success
```

#### Test 5.2: Set Material Color Parameter using object format
```python
manage_material(
    action="set_parameter",
    material_name="/Game/Test/M_ColorTest",
    parameter_name="BaseColor",
    parameter_value={"R": 0.0, "G": 1.0, "B": 0.0, "A": 1.0}  # Green (normalized)
)
# Expected: Success
```

---

## Test 6: UMG Widget - ColorAndOpacity (FLinearColor)

### Setup
```python
# Create a widget
manage_umg_widget(
    action="create",
    widget_name="/Game/Test/WBP_ColorTest"
)

# Add a TextBlock component
manage_umg_widget(
    action="add_component",
    widget_name="/Game/Test/WBP_ColorTest",
    component_type="TextBlock",
    component_name="TestText"
)
```

### Test Cases

#### Test 6.1: Set ColorAndOpacity using JSON object (0-1 normalized)
```python
manage_umg_widget(
    action="set_property",
    widget_name="/Game/Test/WBP_ColorTest",
    component_name="TestText",
    property_name="ColorAndOpacity",
    property_value={"R": 1.0, "G": 0.0, "B": 0.0, "A": 1.0}  # Red (FLinearColor)
)
# Expected: Success
```

#### Test 6.2: Set ColorAndOpacity using array format
```python
manage_umg_widget(
    action="set_property",
    widget_name="/Game/Test/WBP_ColorTest",
    component_name="TestText",
    property_name="ColorAndOpacity",
    property_value=[0.0, 1.0, 1.0, 1.0]  # Cyan (array format)
)
# Expected: Success
```

#### Test 6.3: Set semi-transparent color
```python
manage_umg_widget(
    action="set_property",
    widget_name="/Game/Test/WBP_ColorTest",
    component_name="TestText",
    property_name="ColorAndOpacity",
    property_value={"R": 1.0, "G": 1.0, "B": 1.0, "A": 0.5}  # White, 50% opacity
)
# Expected: Success
```

#### Test 6.4: Set Border BrushColor
```python
# Add a Border component first
manage_umg_widget(
    action="add_component",
    widget_name="/Game/Test/WBP_ColorTest",
    component_type="Border",
    component_name="TestBorder"
)

# Set BrushColor (also FLinearColor)
manage_umg_widget(
    action="set_property",
    widget_name="/Game/Test/WBP_ColorTest",
    component_name="TestBorder",
    property_name="BrushColor",
    property_value={"R": 0.2, "G": 0.2, "B": 0.2, "A": 1.0}  # Dark gray
)
# Expected: Success
```

---

## Test 7: Error Cases - Invalid Formats

### Test 7.1: Wrong range for FColor (using 0-1 instead of 0-255)
```python
# This might work if code auto-converts, but should ideally use 0-255
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="LightColor",
    property_value={"R": 1.0, "G": 0.0, "B": 0.0, "A": 1.0}  # Wrong: normalized values
)
# Expected: May fail or convert incorrectly
```

### Test 7.2: Wrong range for FLinearColor (using 0-255 instead of 0-1)
```python
manage_umg_widget(
    action="set_property",
    widget_name="/Game/Test/WBP_ColorTest",
    component_name="TestText",
    property_name="ColorAndOpacity",
    property_value={"R": 255, "G": 0, "B": 0, "A": 255}  # Wrong: byte values
)
# Expected: May fail or clamp values incorrectly
```

### Test 7.3: Missing alpha channel
```python
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="LightColor",
    property_value=[255, 0, 0]  # Only 3 values, alpha should default to 255
)
# Expected: Should work (alpha defaults to 255)
```

### Test 7.4: Invalid property name
```python
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_ColorTest",
    component_name="TestSpotLight",
    property_name="InvalidColorProperty",
    property_value={"R": 255, "G": 0, "B": 0, "A": 255}
)
# Expected: Error - property not found
```

---

## Test 8: Comprehensive Workflow

### Complete Color Setup Workflow
```python
# 1. Create blueprint with multiple light components
manage_blueprint(
    action="create",
    blueprint_name="/Game/Test/BP_MultiLight",
    parent_class="/Script/Engine.Actor"
)

# 2. Add SpotLight (warm white)
manage_blueprint_component(
    action="create",
    blueprint_name="/Game/Test/BP_MultiLight",
    component_name="WarmLight",
    component_class="/Script/Engine.SpotLightComponent"
)
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_MultiLight",
    component_name="WarmLight",
    property_name="LightColor",
    property_value={"R": 255, "G": 220, "B": 180, "A": 255}  # Warm white
)

# 3. Add PointLight (cool blue)
manage_blueprint_component(
    action="create",
    blueprint_name="/Game/Test/BP_MultiLight",
    component_name="CoolLight",
    component_class="/Script/Engine.PointLightComponent"
)
manage_blueprint_component(
    action="set_property",
    blueprint_name="/Game/Test/BP_MultiLight",
    component_name="CoolLight",
    property_name="LightColor",
    property_value=[135, 206, 250, 255]  # Light blue (array format)
)

# 4. Create FColor variable
manage_blueprint_variable(
    action="create",
    blueprint_name="/Game/Test/BP_MultiLight",
    variable_name="AmbientColor",
    variable_type="struct:Color"
)
manage_blueprint_variable(
    action="set_value",
    blueprint_name="/Game/Test/BP_MultiLight",
    variable_name="AmbientColor",
    variable_value={"R": 50, "G": 50, "B": 50, "A": 255}  # Dark gray ambient
)

# 5. Compile blueprint
compile_blueprint("/Game/Test/BP_MultiLight")

# 6. Verify all colors were set correctly
warm_result = manage_blueprint_component(
    action="get_property",
    blueprint_name="/Game/Test/BP_MultiLight",
    component_name="WarmLight",
    property_name="LightColor"
)

cool_result = manage_blueprint_component(
    action="get_property",
    blueprint_name="/Game/Test/BP_MultiLight",
    component_name="CoolLight",
    property_name="LightColor"
)

ambient_result = manage_blueprint_variable(
    action="get_value",
    blueprint_name="/Game/Test/BP_MultiLight",
    variable_name="AmbientColor"
)

# Expected: All should return the colors that were set
```

---

## Color Reference Tables

### FColor (0-255 byte values) - Common Colors
| Color | R | G | B | A | JSON Object | Array |
|-------|---|---|---|---|-------------|-------|
| Red | 255 | 0 | 0 | 255 | `{"R": 255, "G": 0, "B": 0, "A": 255}` | `[255, 0, 0, 255]` |
| Green | 0 | 255 | 0 | 255 | `{"R": 0, "G": 255, "B": 0, "A": 255}` | `[0, 255, 0, 255]` |
| Blue | 0 | 0 | 255 | 255 | `{"R": 0, "G": 0, "B": 255, "A": 255}` | `[0, 0, 255, 255]` |
| Yellow | 255 | 255 | 0 | 255 | `{"R": 255, "G": 255, "B": 0, "A": 255}` | `[255, 255, 0, 255]` |
| Cyan | 0 | 255 | 255 | 255 | `{"R": 0, "G": 255, "B": 255, "A": 255}` | `[0, 255, 255, 255]` |
| Magenta | 255 | 0 | 255 | 255 | `{"R": 255, "G": 0, "B": 255, "A": 255}` | `[255, 0, 255, 255]` |
| White | 255 | 255 | 255 | 255 | `{"R": 255, "G": 255, "B": 255, "A": 255}` | `[255, 255, 255, 255]` |
| Black | 0 | 0 | 0 | 255 | `{"R": 0, "G": 0, "B": 0, "A": 255}` | `[0, 0, 0, 255]` |
| Orange | 255 | 165 | 0 | 255 | `{"R": 255, "G": 165, "B": 0, "A": 255}` | `[255, 165, 0, 255]` |
| Purple | 128 | 0 | 128 | 255 | `{"R": 128, "G": 0, "B": 128, "A": 255}` | `[128, 0, 128, 255]` |

### FLinearColor (0.0-1.0 normalized) - Common Colors
| Color | R | G | B | A | JSON Object | Array |
|-------|---|---|---|---|-------------|-------|
| Red | 1.0 | 0.0 | 0.0 | 1.0 | `{"R": 1.0, "G": 0.0, "B": 0.0, "A": 1.0}` | `[1.0, 0.0, 0.0, 1.0]` |
| Green | 0.0 | 1.0 | 0.0 | 1.0 | `{"R": 0.0, "G": 1.0, "B": 0.0, "A": 1.0}` | `[0.0, 1.0, 0.0, 1.0]` |
| Blue | 0.0 | 0.0 | 1.0 | 1.0 | `{"R": 0.0, "G": 0.0, "B": 1.0, "A": 1.0}` | `[0.0, 0.0, 1.0, 1.0]` |
| Yellow | 1.0 | 1.0 | 0.0 | 1.0 | `{"R": 1.0, "G": 1.0, "B": 0.0, "A": 1.0}` | `[1.0, 1.0, 0.0, 1.0]` |
| White | 1.0 | 1.0 | 1.0 | 1.0 | `{"R": 1.0, "G": 1.0, "B": 1.0, "A": 1.0}` | `[1.0, 1.0, 1.0, 1.0]` |
| Black | 0.0 | 0.0 | 0.0 | 1.0 | `{"R": 0.0, "G": 0.0, "B": 0.0, "A": 1.0}` | `[0.0, 0.0, 0.0, 1.0]` |
| Orange | 1.0 | 0.647 | 0.0 | 1.0 | `{"R": 1.0, "G": 0.647, "B": 0.0, "A": 1.0}` | `[1.0, 0.647, 0.0, 1.0]` |

---

## Troubleshooting

### Issue: "Failed to set property 'LightColor' value"
**Possible Causes:**
1. Wrong format (using FLinearColor format for FColor or vice versa)
2. Wrong range (using 0-1 for FColor or 0-255 for FLinearColor)
3. Invalid property name or component name
4. Property is read-only

**Solutions:**
- Verify property type: Use `get_property_metadata` to check if it's FColor or FLinearColor
- Use correct range: FColor = 0-255, FLinearColor = 0.0-1.0
- Use correct format: JSON object `{"R": ..., "G": ..., "B": ..., "A": ...}` OR array `[R, G, B, A]`
- Check component/actor exists and property is writable

### Issue: "Color appears wrong (wrong hue)"
**Possible Causes:**
1. Using wrong color type (FColor vs FLinearColor)
2. Values out of range causing clamping
3. Alpha channel affecting display

**Solutions:**
- Double-check you're using FColor (0-255) for lights, FLinearColor (0-1) for widgets
- Verify values are within correct range
- Set alpha to 255 (FColor) or 1.0 (FLinearColor) for full opacity

### Issue: "Property set succeeded but no visual change"
**Possible Causes:**
1. Blueprint not compiled
2. Component not visible/active
3. Property overridden by other systems

**Solutions:**
- Compile blueprint after setting properties: `compile_blueprint(blueprint_name)`
- Verify component is active and visible
- Check if property is being overridden at runtime

---

## Summary

**Key Points:**
1. **FColor** (lights, some materials): 0-255 byte values, RGB order
2. **FLinearColor** (UMG widgets, some materials): 0.0-1.0 normalized, RGB order
3. Both support JSON object `{"R": ..., "G": ..., "B": ..., "A": ...}` OR array `[R, G, B, A]` formats
4. Always verify property type before setting
5. Compile blueprints after setting component properties
6. Level actors use string format: `"(R=255,G=0,B=0,A=255)"`


# Property Setting Complete Guide

## 🎯 When to Use This Guide
- Setting component properties (lights, cameras, meshes)
- Property format errors or unexpected values
- Color/vector/struct property confusion
- Trial-and-error not working

---

## 🎨 Color Properties (FColor, FLinearColor)

### ✅ CORRECT Formats:

**Bright Yellow Light**:
```python
property_value=[255, 255, 0, 255]  # R, G, B, A (0-255 range)
```

**Common Colors Reference**:
- **Yellow**: `[255, 255, 0, 255]`
- **Red**: `[255, 0, 0, 255]`  
- **Blue**: `[0, 0, 255, 255]`
- **Green**: `[0, 255, 0, 255]`
- **Cyan**: `[0, 255, 255, 255]`
- **Magenta**: `[255, 0, 255, 255]`
- **Orange**: `[255, 165, 0, 255]`
- **Purple**: `[128, 0, 128, 255]`
- **White**: `[255, 255, 255, 255]`
- **Black**: `[0, 0, 0, 255]`

### ❌ COMMON MISTAKES:

**Mistake 1**: Using 0.0-1.0 range
```python
# ❌ WRONG - This creates nearly black color!
property_value=[1.0, 1.0, 0.0, 1.0]
# Result: Appears black, not yellow

# ✅ CORRECT - Full brightness yellow
property_value=[255, 255, 0, 255]
# Result: Bright yellow as expected
```

**Mistake 2**: Dictionary format (unreliable)
```python
# ❌ AVOID - May timeout or fail
property_value={"R": 255, "G": 255, "B": 0, "A": 255}

# ✅ USE ARRAY INSTEAD
property_value=[255, 255, 0, 255]
```

**Mistake 3**: Missing alpha channel
```python
# ❌ INCOMPLETE - May fail
property_value=[255, 255, 0]

# ✅ INCLUDE ALPHA
property_value=[255, 255, 0, 255]
```

---

## 💡 Light-Specific Properties

### **LightColor** (FColor):
- **Format**: `[R, G, B, A]` where each value is 0-255
- **Example**: `[255, 255, 0, 255]` for yellow
- **Tips**: 
  - 255 = full brightness for that channel
  - 0 = no contribution from that channel
  - Alpha usually 255 (fully opaque)

### **Intensity** (float):
- **Range**: Typically 1000-10000 (lumens)
- **Example**: `5000` for bright indoor light

### **AttenuationRadius** (float):
- **Range**: Unreal units (typically 500-5000)
- **Example**: `2000` for medium-range light

### **InnerConeAngle** / **OuterConeAngle** (float):
- **Range**: 0-89 degrees
- **Example**: Inner: `10`, Outer: `45`
- **Tips**: OuterConeAngle must be > InnerConeAngle

---

## 🔄 Complete Workflow Example

```python
# Step 1: Set the color to yellow
manage_blueprint_components(
    blueprint_name="/Game/Blueprints/Characters/BP_Player",
    action="set_property",
    component_name="SpotLight_Top",
    property_name="LightColor",
    property_value=[255, 255, 0, 255]  # Bright yellow
)

# Step 2: Verify it was set
result = manage_blueprint_components(
    action="get_property",
    component_name="SpotLight_Top",
    property_name="LightColor"
)
# Expected: {"value": [255, 255, 0, 255]}

# Step 3: Compile the Blueprint
compile_blueprint("/Game/Blueprints/Characters/BP_Player")
```

---

## 🆘 Troubleshooting

### "Color appears black instead of yellow"
**Cause**: Used 1 or 1.0 instead of 255  
**Fix**: 
```python
property_value=[255, 255, 0, 255]  # Not [1, 1, 0, 1]
```

### "Property set succeeded but no change"
**Cause**: Blueprint not compiled  
**Fix**: Call `compile_blueprint()` after setting properties

### "Failed to set property"
**Common Causes**: 
1. Wrong format (dictionary instead of array)
2. Invalid values (negative, > 255)
3. Property is read-only

**Fix**: Use array format with valid ranges
```python
property_value=[255, 255, 0, 255]  # Simple array
```

---

## 📋 Property Type Quick Reference

| Type | Format | Example |
|------|--------|---------|
| **FColor** | `[R,G,B,A]` | `[255,255,0,255]` |
| **FVector** | `[X,Y,Z]` | `[100.0,0.0,50.0]` |
| **FRotator** | `[Pitch,Yaw,Roll]` | `[0.0,90.0,0.0]` |
| **float** | number | `5000` |
| **bool** | boolean | `true` |

---

## 💡 Pro Tips

1. **Always use arrays for colors**: `[255, 255, 0, 255]` is most reliable
2. **Compile after changes**: Properties don't take effect until compiled
3. **Verify with get_property**: Check value was actually set
4. **Use get_help() when stuck**: Don't waste time on trial-and-error!

---

**Remember**: When you encounter property setting issues, call `get_help(topic="properties")` FIRST!

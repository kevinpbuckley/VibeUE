# Property Setting Complete Guide

## 🎯 When to Use This Guide
- Setting component properties (lights, cameras, meshes)
- Property format errors or unexpected values
- Color/vector/struct property confusion
- Trial-and-error not working

---

## 🎨 Color Properties (FColor, FLinearColor)

### ⚠️ CRITICAL: Light Components Use BGR Format with Normalized Values!

**Light components (SpotLight, PointLight, etc.) require:**
- **BGR order** (Blue, Green, Red, Alpha) - NOT RGB!
- **Normalized floats** (0.0-1.0 range) - NOT bytes (0-255)!

### ✅ CORRECT Formats for Light LightColor Property:

**Bright Yellow Light**:
```python
property_value=[0, 1, 1, 1]  # B, G, R, A (BGR order, 0.0-1.0 normalized)
```

**Common Colors Reference (Light Components - BGR Normalized)**:
- **Yellow**: `[0, 1, 1, 1]` (Blue=0, Green=1, Red=1)
- **Orange**: `[0, 0.647, 1, 1]` (Blue=0, Green=0.647, Red=1)
- **Red**: `[0, 0, 1, 1]` (Blue=0, Green=0, Red=1)
- **Green**: `[0, 1, 0, 1]` (Blue=0, Green=1, Red=0)
- **Blue**: `[1, 0, 0, 1]` (Blue=1, Green=0, Red=0)
- **Cyan**: `[1, 1, 0, 1]` (Blue=1, Green=1, Red=0)
- **Magenta**: `[1, 0, 1, 1]` (Blue=1, Green=0, Red=1)
- **Purple**: `[0.502, 0, 0.502, 1]` (Blue=128/255, Green=0, Red=128/255)
- **White**: `[1, 1, 1, 1]`
- **Black**: `[0, 0, 0, 1]`

**Widget Colors (UMG) - Different Format (RGB 0-255)**:
- See `get_help(topic="umg-guide")` for widget color formats
- Widgets use RGB order with 0-255 byte values
- DO NOT confuse with light component format!

### ❌ COMMON MISTAKES:

**Mistake 1**: Using RGB order instead of BGR
```python
# ❌ WRONG - RGB order for light components
property_value=[1, 1, 0, 1]  # Intended yellow, but this is CYAN in BGR!
# Result: Light appears cyan (Blue=1, Green=1, Red=0)

# ✅ CORRECT - BGR order for yellow
property_value=[0, 1, 1, 1]  # Blue=0, Green=1, Red=1
# Result: Bright yellow as expected
```

**Mistake 2**: Using byte values (0-255) instead of normalized floats
```python
# ❌ WRONG - Byte values get clamped/normalized incorrectly
property_value=[0, 255, 255, 255]
# Result: Values get misinterpreted, unpredictable colors

# ✅ CORRECT - Normalized float values
property_value=[0, 1, 1, 1]
# Result: Proper yellow light
```

**Mistake 3**: Dictionary format (unreliable)
```python
# ❌ AVOID - May fail completely
property_value={"R": 1, "G": 1, "B": 0, "A": 1}

# ✅ USE ARRAY INSTEAD
property_value=[0, 1, 1, 1]  # BGR order!
```

**Mistake 4**: Missing alpha channel
```python
# ❌ INCOMPLETE - May fail
property_value=[0, 1, 1]

# ✅ INCLUDE ALPHA
property_value=[0, 1, 1, 1]
```

---

## 💡 Light-Specific Properties

### **LightColor** (FColor):
- **Format**: `[B, G, R, A]` - **BGR ORDER!** Each value 0.0-1.0 normalized
- **Example**: `[0, 1, 1, 1]` for yellow (Blue=0, Green=1, Red=1)
- **Tips**: 
  - 1.0 = full brightness for that channel
  - 0.0 = no contribution from that channel
  - Alpha usually 1.0 (fully opaque)
  - **CRITICAL**: Lights use BGR, widgets use RGB!
  - To convert RGB to BGR: Swap first and third values
    - RGB Yellow `[1, 1, 0]` → BGR Yellow `[0, 1, 1]`
    - RGB Orange `[1, 0.647, 0]` → BGR Orange `[0, 0.647, 1]`

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
# Step 1: Set the color to yellow (remember BGR format!)
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/Characters/BP_Player",
    action="set_property",
    component_name="SpotLight_Top",
    property_name="LightColor",
    property_value=[0, 1, 1, 1]  # BGR: Blue=0, Green=1, Red=1 = Yellow!
)

# Step 2: Verify it was set (values returned as bytes 0-255)
result = manage_blueprint_component(
    action="get_property",
    component_name="SpotLight_Top",
    property_name="LightColor"
)
# Expected: {"value": [0, 255, 255, 255]} (bytes, but we set with normalized floats)

# Step 3: Set orange light (RGB 255,165,0 → BGR normalized)
manage_blueprint_component(
    action="set_property",
    component_name="SpotLight_Fill",
    property_name="LightColor",
    property_value=[0, 0.647, 1, 1]  # BGR: Blue=0, Green=165/255, Red=1
)

# Step 4: Compile the Blueprint
compile_blueprint("/Game/Blueprints/Characters/BP_Player")
```

---

## 🆘 Troubleshooting

### "Color appears wrong (cyan instead of yellow, etc.)"
**Cause**: Used RGB order instead of BGR for light components  
**Fix**: 
```python
# ❌ WRONG: RGB [1, 1, 0] = Cyan in BGR format!
property_value=[1, 1, 0, 1]

# ✅ CORRECT: BGR [0, 1, 1] = Yellow
property_value=[0, 1, 1, 1]
```

### "Light appears dim/black"
**Cause**: Used byte values (0-255) instead of normalized floats  
**Fix**: Convert to 0.0-1.0 range
```python
# ❌ WRONG: Byte values
property_value=[0, 255, 255, 255]

# ✅ CORRECT: Normalized floats
property_value=[0, 1, 1, 1]
```

### "Property set succeeded but no change visible"
**Cause**: Blueprint not compiled  
**Fix**: Call `compile_blueprint()` after setting properties

### "Failed to set property 'LightColor'"
**Common Causes**: 
1. Wrong format (dictionary instead of array)
2. Invalid values (negative, > 1.0 for normalized, or > 255 for bytes)
3. Property is read-only (check with get_property_metadata)

**Fix**: Use array format with normalized values in BGR order
```python
property_value=[0, 1, 1, 1]  # Simple BGR array, normalized
```

---

## 📋 Property Type Quick Reference

| Type | Format | Example | Notes |
|------|--------|---------|-------|
| **FColor (Lights)** | `[B,G,R,A]` | `[0,1,1,1]` | BGR order, 0.0-1.0 normalized |
| **FColor (Widgets)** | `[R,G,B,A]` | `[255,255,0,255]` | RGB order, 0-255 bytes |
| **FVector** | `[X,Y,Z]` | `[100.0,0.0,50.0]` | World coordinates |
| **FRotator** | `[Pitch,Yaw,Roll]` | `[0.0,90.0,0.0]` | Degrees |
| **float** | number | `5000` | Single value |
| **bool** | boolean | `true` | true/false |

---

## 💡 Pro Tips

1. **Light colors use BGR normalized**: `[0, 1, 1, 1]` for yellow, NOT `[255, 255, 0, 255]`
2. **Widget colors use RGB bytes**: See `get_help(topic="umg-guide")` for widget formats
3. **Compile after changes**: Properties don't take effect until compiled
4. **Verify with get_property**: Check value was actually set (returned as bytes)
5. **Use get_help() when stuck**: Don't waste time on trial-and-error!
6. **Remember the swap**: RGB → BGR means swap first and third values
   - RGB `[R, G, B, A]` → BGR `[B, G, R, A]`

---

**Remember**: Light component colors are the OPPOSITE order from widget colors! Call `get_help(topic="properties")` when confused!

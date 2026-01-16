# Enhanced Input Critical Rules

**Note:** Method signatures are in `vibeue_apis` from skill loader. This file contains gotchas that discovery can't tell you.

---

## ⚠️ CRITICAL: Property Names on Info Structs

**ALWAYS use the correct property names or you'll get AttributeError:**

```python
# ✅ InputTypeDiscoveryResult - Use action_value_types (NOT value_types)
types = unreal.InputService.discover_types()
print(types.action_value_types)  # ✅ CORRECT
print(types.value_types)  # ❌ AttributeError!

# ✅ KeyMappingInfo - Use key_name (NOT key)
mappings = unreal.InputService.get_mappings(context_path)
for m in mappings:
    print(m.key_name)  # ✅ CORRECT
    print(m.key)  # ❌ AttributeError!

# ✅ InputModifierInfo - Use type_name or display_name (NOT modifier_type)
mods = unreal.InputService.get_modifiers(context_path, 0)
for mod in mods:
    print(mod.type_name)  # ✅ CORRECT (e.g., "InputModifierNegate")
    print(mod.display_name)  # ✅ CORRECT (e.g., "Negate")
    print(mod.modifier_type)  # ❌ AttributeError!

# ✅ InputTriggerInfo - Use type_name or display_name (NOT trigger_type)
trigs = unreal.InputService.get_triggers(context_path, 0)
for trig in trigs:
    print(trig.type_name)  # ✅ CORRECT (e.g., "InputTriggerPressed")
    print(trig.display_name)  # ✅ CORRECT (e.g., "Pressed")
    print(trig.trigger_type)  # ❌ AttributeError!
```

---

## ⚠️ CRITICAL: Value Types

Input Actions have specific value types:

| Value Type | Use Case |
|------------|----------|
| `Boolean` | Simple press/release (Jump, Fire) |
| `Axis1D` | Single axis (Throttle, Zoom) |
| `Axis2D` | Two axes (Move, Look) |
| `Axis3D` | Three axes (3D manipulation) |

```python
unreal.InputService.create_action("Jump", "/Game/Input", "Boolean")
unreal.InputService.create_action("Move", "/Game/Input", "Axis2D")
```

---

## ⚠️ CRITICAL: Key Names

Use Unreal key names exactly:

**Keyboard:**
- `SpaceBar`, `LeftShift`, `LeftControl`, `LeftAlt`
- `W`, `A`, `S`, `D`, `E`, `Q` (uppercase)
- `One`, `Two`, ... `Nine`, `Zero`
- `F1`, `F2`, ... `F12`

**Mouse:**
- `LeftMouseButton`, `RightMouseButton`, `MiddleMouseButton`
- `MouseScrollUp`, `MouseScrollDown`

**Gamepad:**
- `Gamepad_LeftThumbstick`, `Gamepad_RightThumbstick`
- `Gamepad_LeftTrigger`, `Gamepad_RightTrigger`
- `Gamepad_FaceButton_Bottom` (A/X)
- `Gamepad_FaceButton_Right` (B/Circle)
- `Gamepad_FaceButton_Left` (X/Square)
- `Gamepad_FaceButton_Top` (Y/Triangle)

---

## ⚠️ CRITICAL: Asset Paths

Input assets need full paths:

```python
action_path = "/Game/Input/IA_Jump"  # Input Action
context_path = "/Game/Input/IMC_Default"  # Mapping Context
```

---

## ⚠️ CRITICAL: Triggers and Modifiers

**Common Triggers:**
- `Pressed` - Fire on key down
- `Released` - Fire on key up
- `Down` - Fire every frame while held
- `Hold` - Fire after hold threshold
- `Tap` - Fire on quick press
- `Pulse` - Fire at interval while held

**Common Modifiers:**
- `Negate` - Invert axis value
- `DeadZone` - Apply dead zone
- `Scalar` - Scale value
- `SwizzleInputAxisValues` - Swap X/Y axes

```python
# Add trigger to mapping (requires mapping_index, not action_path)
context_path = "/Game/Input/IMC_Default"
mapping_index = 0  # Get from get_mappings() first
unreal.InputService.add_trigger(context_path, mapping_index, "Hold")

# Add modifier (requires mapping_index, not action_path)
unreal.InputService.add_modifier(context_path, mapping_index, "DeadZone")
```

---

## ⚠️ CRITICAL: Save After Changes

```python
unreal.EditorAssetLibrary.save_asset(action_path)
unreal.EditorAssetLibrary.save_asset(context_path)
```

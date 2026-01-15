# Enhanced Input Critical Rules

---

## 📋 Service Discovery

Discover Input services with module search:

```python
# Use discover_python_module to find Input services
discover_python_module(module_name="unreal", name_filter="InputService", include_classes=True)
# Returns: InputService

# Then discover specific service methods:
discover_python_class(class_name="unreal.InputService")
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

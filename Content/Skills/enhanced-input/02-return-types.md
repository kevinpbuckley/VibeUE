# InputService Return Types

Critical documentation for return type struct properties. **DO NOT guess property names!**

---

## InputTypeDiscoveryResult

Returned by: `discover_types()`

### Properties
```python
types = unreal.InputService.discover_types()

types.action_value_types  # Array[str]: ["Boolean", "Axis1D", "Axis2D", "Axis3D"]
types.modifier_types      # Array[str]: ["Negate", "DeadZone", "Scalar", ...]
types.trigger_types       # Array[str]: ["Pressed", "Released", "Hold", "Tap", ...]
```

---

## InputCreateResult

Returned by: `create_action()`, `create_mapping_context()`

### Properties
```python
result = unreal.InputService.create_action("IA_Jump", "/Game/Input", "Boolean")

result.success        # bool: True if created successfully
result.asset_path     # str: "/Game/Input/IA_Jump" (full path to created asset)
result.error_message  # str: Error message if failed (empty string if success)
```

### Usage Pattern
```python
import unreal

result = unreal.InputService.create_action("IA_Move", "/Game/Input", "Axis2D")
if result.success:
    # CORRECT - Use asset_path
    action_path = result.asset_path
    unreal.InputService.configure_action(action_path, True, False, "Movement")
else:
    print(f"Failed: {result.error_message}")

# WRONG - Using result directly as string
# unreal.InputService.configure_action(result, ...)  # ERROR!
```

---

## InputActionDetailedInfo

Returned by: `get_input_action_info()`

### Properties
```python
info = unreal.InputService.get_input_action_info("/Game/Input/IA_Jump")

info.action_name         # str: "IA_Jump"
info.action_path         # str: "/Game/Input/IA_Jump"
info.value_type          # str: "Boolean", "Axis1D", "Axis2D", or "Axis3D"
info.consume_input       # bool: Whether action consumes input
info.trigger_when_paused # bool: Whether triggers when game paused
info.description         # str: Action description text
```

### Common Mistakes
```python
# WRONG property names:
# info.action_description  # Use info.description
# info.type                # Use info.value_type
```

---

## MappingContextDetailedInfo

Returned by: `get_mapping_context_info()`

### Properties
```python
info = unreal.InputService.get_mapping_context_info("/Game/Input/IMC_Default")

info.context_name    # str: "IMC_Default"
info.context_path    # str: "/Game/Input/IMC_Default"
info.priority        # int: Context priority (higher = processed first)
info.mapped_actions  # Array[str]: Paths of mapped Input Actions
```

---

## KeyMappingInfo

Returned by: `get_mappings()`

### Properties
```python
mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Default")
for m in mappings:
    m.mapping_index   # int: Index in mapping context (0, 1, 2...)
    m.action_name     # str: "IA_Jump" (action name only)
    m.action_path     # str: "/Game/Input/IA_Jump" (full path)
    m.key_name        # str: "SpaceBar", "E", "Gamepad_RightTrigger"
    m.modifier_count  # int: Number of modifiers on this mapping
    m.trigger_count   # int: Number of triggers on this mapping
```

### Usage Pattern
```python
import unreal

mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Default")

# Find mapping by action name
for i, m in enumerate(mappings):
    if m.action_name == "IA_Jump":
        print(f"Jump mapped to: {m.key_name}")
        print(f"Mapping index: {m.mapping_index}")

        # Use mapping_index for modifiers/triggers
        unreal.InputService.add_trigger("/Game/Input/IMC_Default", m.mapping_index, "Pressed")
        break
```

---

## InputModifierInfo

Returned by: `get_modifiers()`

### Properties
```python
mods = unreal.InputService.get_modifiers("/Game/Input/IMC_Default", 0)
for m in mods:
    m.modifier_index  # int: Index in modifier array (0, 1, 2...)
    m.type_name       # str: "InputModifierNegate", "InputModifierDeadZone", etc.
    m.display_name    # str: Human-readable name ("Negate", "Dead Zone", etc.)
```

### Common Mistakes
```python
# WRONG property names:
# m.modifier_type  # Use m.type_name
# m.name           # Use m.display_name
```

---

## InputTriggerInfo

Returned by: `get_triggers()`

### Properties
```python
triggers = unreal.InputService.get_triggers("/Game/Input/IMC_Default", 0)
for t in triggers:
    t.trigger_index   # int: Index in trigger array (0, 1, 2...)
    t.type_name       # str: "InputTriggerPressed", "InputTriggerHold", etc.
    t.display_name    # str: Human-readable name ("Pressed", "Hold", etc.)
```

### Common Mistakes
```python
# WRONG property names:
# t.trigger_type  # Use t.type_name
# t.name          # Use t.display_name
```

---

## Safe Iteration Patterns

### Using next() with Default
```python
import unreal

# WRONG - Can cause StopIteration crash
# idx = next(i for i, m in enumerate(mappings) if m.action_name == "IA_Jump")

# CORRECT - Provide default value
mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Default")
idx = next((i for i, m in enumerate(mappings) if m.action_name == "IA_Jump"), None)

if idx is not None:
    unreal.InputService.add_trigger("/Game/Input/IMC_Default", idx, "Pressed")
else:
    print("Action not found in mappings")
```

### Using Explicit Loop
```python
import unreal

mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Default")

# CORRECT - Explicit loop with break
for i, m in enumerate(mappings):
    if m.action_name == "IA_Jump":
        unreal.InputService.add_trigger("/Game/Input/IMC_Default", i, "Pressed")
        break
```

---

## Valid Key Names

### Always Verify First
```python
import unreal

# Get available keys for a platform
gamepad_keys = unreal.InputService.get_available_keys("Gamepad")
print("Available gamepad keys:")
for key in gamepad_keys:
    if "Stick" in key or "Trigger" in key:
        print(f"  {key}")
```

### Common Valid Gamepad Keys
```python
# Analog sticks (individual axes)
"Gamepad_LeftX"            # Left stick horizontal
"Gamepad_LeftY"            # Left stick vertical
"Gamepad_RightX"           # Right stick horizontal
"Gamepad_RightY"           # Right stick vertical

# Triggers
"Gamepad_LeftTriggerAxis"  # Left trigger (analog)
"Gamepad_RightTriggerAxis" # Right trigger (analog)

# Face buttons
"Gamepad_FaceButton_Bottom" # A (Xbox), X (PS)
"Gamepad_FaceButton_Right"  # B (Xbox), Circle (PS)
"Gamepad_FaceButton_Left"   # X (Xbox), Square (PS)
"Gamepad_FaceButton_Top"    # Y (Xbox), Triangle (PS)

# D-Pad
"Gamepad_DPad_Up"
"Gamepad_DPad_Down"
"Gamepad_DPad_Left"
"Gamepad_DPad_Right"
```

### Invalid Key Names
```python
# WRONG - These don't exist:
# "Gamepad_LeftStick_2D"    # Not a valid key name
# "Gamepad_LeftStick"       # Not a valid key name
# "Gamepad_A"               # Use "Gamepad_FaceButton_Bottom"
```

---

## Type Casting and Comparisons

### Struct Comparison
```python
import unreal

# Get action info
info1 = unreal.InputService.get_input_action_info("/Game/Input/IA_Jump")
info2 = unreal.InputService.get_input_action_info("/Game/Input/IA_Jump")

# Compare properties, not structs directly
if info1 and info2:
    if info1.action_path == info2.action_path:
        print("Same action")
```

### None Checks
```python
import unreal

# ALWAYS check for None before accessing properties
info = unreal.InputService.get_input_action_info("/Game/Input/IA_Jump")

if info:
    print(f"Value type: {info.value_type}")
else:
    print("Action not found")
```

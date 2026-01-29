---
name: enhanced-input
display_name: Enhanced Input System
description: Create and configure Input Actions, Mapping Contexts, triggers, and modifiers
vibeue_classes:
  - InputService
unreal_classes:
  - EditorAssetLibrary
---

# Enhanced Input Skill

## Critical Rules

### ⚠️ Property Names on Info Structs

| Struct | WRONG | CORRECT |
|--------|-------|---------|
| `InputTypeDiscoveryResult` | `value_types` | `action_value_types` |
| `KeyMappingInfo` | `key` | `key_name` |
| `InputModifierInfo` | `modifier_type` | `type_name` or `display_name` |
| `InputTriggerInfo` | `trigger_type` | `type_name` or `display_name` |

### ⚠️ Value Types

| Value Type | Use Case |
|------------|----------|
| `Boolean` | Simple press/release (Jump, Fire) |
| `Axis1D` | Single axis (Throttle, Zoom) |
| `Axis2D` | Two axes (Move, Look) |
| `Axis3D` | Three axes (3D manipulation) |

### ⚠️ Key Names

**Keyboard:** `SpaceBar`, `LeftShift`, `W`, `A`, `S`, `D`, `F1`...  
**Mouse:** `LeftMouseButton`, `RightMouseButton`, `MouseScrollUp`  
**Gamepad:** `Gamepad_FaceButton_Bottom`, `Gamepad_LeftThumbstick`

### ⚠️ Triggers and Modifiers

**Triggers:** `Pressed`, `Released`, `Down`, `Hold`, `Tap`, `Pulse`  
**Modifiers:** `Negate`, `DeadZone`, `Scalar`, `SwizzleInputAxisValues`

---

## Workflows

### Create Input Action

```python
import unreal

result = unreal.InputService.create_action("Jump", "/Game/Input", "Boolean")
if result.success:
    unreal.EditorAssetLibrary.save_asset(result.asset_path)
```

### Create Mapping Context

```python
import unreal

result = unreal.InputService.create_mapping_context("Default", "/Game/Input", 0)
unreal.EditorAssetLibrary.save_asset(result.asset_path)
```

### Add Key Mapping

```python
import unreal

context_path = "/Game/Input/IMC_Default"
action_path = "/Game/Input/IA_Jump"
unreal.InputService.add_key_mapping(context_path, action_path, "SpaceBar")
unreal.EditorAssetLibrary.save_asset(context_path)
```

### Add Triggers and Modifiers

```python
import unreal

context_path = "/Game/Input/IMC_Default"
action_path = "/Game/Input/IA_Fire"

unreal.InputService.add_key_mapping(context_path, action_path, "LeftMouseButton")

# Get mapping index
mappings = unreal.InputService.get_mappings(context_path)
mapping_index = len(mappings) - 1

# Add trigger/modifier using mapping index
unreal.InputService.add_trigger(context_path, mapping_index, "Pressed")
unreal.InputService.add_modifier(context_path, mapping_index, "DeadZone")
unreal.EditorAssetLibrary.save_asset(context_path)
```

### Get Mappings Info

```python
import unreal

mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Default")
for m in mappings:
    print(f"Action: {m.action_name}, Key: {m.key_name}")

info = unreal.InputService.get_input_action_info("/Game/Input/IA_Jump")
if info:
    print(f"Action: {info.action_name}, ValueType: {info.value_type}")
```

### Discover Available Types

```python
import unreal

types = unreal.InputService.discover_types()
print(f"Value Types: {types.action_value_types}")
print(f"Modifiers: {types.modifier_types}")
print(f"Triggers: {types.trigger_types}")

keys = unreal.InputService.get_available_keys()
```

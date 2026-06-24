---
name: enhanced-input
display_name: Enhanced Input System
description: Create and configure Enhanced Input — Input Actions, Mapping Contexts, key mappings, triggers, and modifiers (InputService). Use when the user asks to set up player input, create an Input Action (IA_) or Input Mapping Context (IMC_), bind keys to actions, or configure input triggers/modifiers.
vibeue_classes:
  - InputService
unreal_classes:
  - EditorAssetLibrary
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "enhanced-input"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Enhanced Input Skill

## Critical Rules

### 🚨 Real Python method names — do NOT search for the docstring's "action" names

The `InputService` class docstring lists MCP-style action ids (`action_create`,
`mapping_list_contexts`, `action_configure`, ...). Those are **not** the Python method
names — `discover_python_class(method_filter=...)` matches nothing for them. The complete
real API is below; you rarely need a discovery call at all:

| Area | Methods |
|------|---------|
| Reflection | `discover_types()`, `get_available_keys(filter="")`, `get_available_modifier_types()`, `get_available_trigger_types()` |
| Actions | `create_action(name, path, value_type="Axis1D")`, `list_input_actions()`, `get_input_action_info(action_path)` → info or None, `configure_action(action_path, consume_input=True, trigger_when_paused=False, description="")`, `input_action_exists(action_path)` |
| Contexts | `create_mapping_context(name, path, priority=0)`, `list_mapping_contexts()`, `get_mapping_context_info(context_path)` → info or None, `mapping_context_exists(context_path)` |
| Mappings | `get_mappings(context_path)`, `add_key_mapping(context_path, action_path, key_name)`, `remove_mapping(context_path, mapping_index)`, `key_mapping_exists(context_path, action_path)` |
| Modifiers | `add_modifier(context_path, mapping_index, modifier_type)`, `remove_modifier(context_path, mapping_index, modifier_index)`, `get_modifiers(context_path, mapping_index)` |
| Triggers | `add_trigger(context_path, mapping_index, trigger_type)`, `remove_trigger(context_path, mapping_index, trigger_index)`, `get_triggers(context_path, mapping_index)` |

`create_action` value types are the strings `discover_types()` returns: `"Boolean"`
(alias `"Digital"`), `"Axis1D"`, `"Axis2D"`, `"Axis3D"`. Modifiers and triggers are
addressed by **index on the mapping**, so `get_mappings` / `get_modifiers` /
`get_triggers` first, then add/remove by index.

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

**Keyboard:** `SpaceBar`, `LeftShift`, `W`, `A`, `S`, `D`, `F1`, `Enter`, `Escape`, `BackSpace`...  
**Mouse:** `LeftMouseButton`, `RightMouseButton`, `MouseScrollUp`  
**Gamepad:** `Gamepad_FaceButton_Bottom`, `Gamepad_LeftThumbstick`, `Gamepad_LeftTrigger`, `Gamepad_RightTrigger`  
**Paired axes (use these for Axis2D bindings):** `Mouse2D` (mouse look), `Gamepad_Left2D` / `Gamepad_Right2D` (analog sticks)

Verify any other name with `get_available_keys("thumb")` (substring filter) instead of guessing.

### ⚠️ Triggers and Modifiers

**Triggers:** `Pressed`, `Released`, `Down`, `Hold`, `Tap`, `Pulse`  
**Modifiers:** `Negate`, `DeadZone`, `Scalar`, `SwizzleAxis` (also: `Smooth`, `SmoothDelta`, `ScaleByDeltaTime` — use `get_available_modifier_types()` for the full set)

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

## Sample scripts (run via `execute_python_code`)

- **`scripts/setup_input.txt`** — create an Input Action + Mapping Context and bind a key.

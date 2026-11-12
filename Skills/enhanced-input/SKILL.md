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
`reflection_discover_types`, ...). Those are **not** the Python method
names — `get_symbol(method_filter=...)` matches nothing for them. The complete
real API is below; you rarely need a discovery call at all:

| Area | Methods |
|------|---------|
| Reflection | `discover_types()`, `get_available_keys(filter="")`, `get_available_modifier_types()`, `get_available_trigger_types()` |
| Actions | `create_action(name, path, value_type="Axis1D")` |
| Contexts | `create_mapping_context(name, path, priority=0)`, `get_mapping_context_info(context_path)` → info or None |
| Mappings | `get_mappings(context_path)`, `add_key_mapping(context_path, action_path, key_name)`, `remove_mapping(context_path, mapping_index)`, `key_mapping_exists(context_path, action_path)` |
| Modifiers | `add_modifier(context_path, mapping_index, modifier_type)`, `remove_modifier(context_path, mapping_index, modifier_index)`, `get_modifiers(context_path, mapping_index)` |
| Triggers | `add_trigger(context_path, mapping_index, trigger_type, properties_json="")`, `add_action_trigger(action_path, trigger_type, properties_json="")` → JSON, `remove_trigger(context_path, mapping_index, trigger_index)`, `get_triggers(context_path, mapping_index)` |

**No longer InputService methods — use Epic's tools** (`vibeue.exec_tool` or `call_tool`):

| Removed | Use instead |
|---|---|
| `input_action_exists`, `mapping_context_exists` | `EditorToolset.AssetToolset.Exists(path)` (also true for a folder path) |
| `list_input_actions`, `list_mapping_contexts` | `EditorToolset.AssetToolset.FindAssets(filter={"assetType": "/Script/EnhancedInput.InputAction", "maxResults": 0})`, or `"/Script/EnhancedInput.InputMappingContext"` for contexts. Sees only saved assets (`create_action` / `create_mapping_context` save on creation); `maxResults=0` drops the default cap of 100 |
| `get_input_action_info` | `EditorToolset.ObjectToolset.GetProperties(instance="/Game/Input/IA_Jump.IA_Jump", properties=["ValueType","bConsumeInput","bTriggerWhenPaused","ActionDescription"])` |
| `configure_action` | `EditorToolset.ObjectToolset.SetProperties(instance="/Game/Input/IA_Jump.IA_Jump", values=json.dumps({"bConsumeInput": True, "bTriggerWhenPaused": False, "ActionDescription": "..."}))` — sets only the keys you pass, and also reaches `ValueType`, `AccumulationBehavior` |

`create_action` value types are the strings `discover_types()` returns: `"Boolean"`
(alias `"Digital"`), `"Axis1D"`, `"Axis2D"`, `"Axis3D"`. The `create_*` folder may be a bare
folder (`"Input"` goes under `/Game`) or start with its mount point (`/Game/...`, a plugin root);
`/Engine`, `/Script` and `/Temp` are refused, and so is an invalid asset name (empty, a space,
`.`), each with the reason in `error_message`. Modifiers and triggers are
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

### Trigger Settings

`properties_json` sets the new trigger's settings, by C++ name or snake_case
(`HoldTimeThreshold` or `hold_time_threshold`, `bIsOneShot` or `is_one_shot`). Only the
settings the trigger's Details panel shows can be set: runtime state such as `HeldDuration`,
`LastValue` or `bShouldAlwaysTick` is refused ("not a trigger setting"), and a number outside
the property's `ClampMin`/`ClampMax` is refused with the allowed range (`hold_time_threshold`
must be >= 0). An unknown name, a refused setting or a bad value fails the call and adds
nothing. `add_action_trigger` puts the trigger on the Input Action itself, so it applies to
every mapping of the action; it returns `PIE_ACTIVE` while a play session runs (stop PIE
first). Neither saves the asset, and a running PIE session keeps its copy of the triggers:
restart PIE to see a change.

```python
import unreal

# Hold 0.4 s before the charged attack fires, once per hold
unreal.InputService.add_trigger("/Game/Input/IMC_Default", 0, "Hold",
    '{"hold_time_threshold": 0.4, "is_one_shot": true}')

# A quick tap on the action itself, for every key bound to it
result = unreal.InputService.add_action_trigger("/Game/Input/IA_Dash", "Tap",
    '{"tap_release_time_threshold": 0.25}')
print(result)   # {"success": true, "trigger_index": 0, ...} or an error_code
unreal.EditorAssetLibrary.save_asset("/Game/Input/IA_Dash")
```

### Get Mappings Info

```python
import unreal

mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Default")
for m in mappings:
    print(f"Action: {m.action_name}, Key: {m.key_name}")

# An Input Action's own settings are Epic's ObjectToolset (no InputService method):
import vibeue
info = vibeue.exec_tool("EditorToolset.ObjectToolset", "GetProperties", {
    "instance": "/Game/Input/IA_Jump.IA_Jump",
    "properties": ["ValueType", "bConsumeInput", "bTriggerWhenPaused", "ActionDescription"]})
print(info)
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

## Sample scripts (run via `execute_python`)

- **`scripts/setup_input.txt`** — create an Input Action + Mapping Context and bind a key.

## PIE input injection (issue #550)

Test input-driven gameplay WITHOUT remapping game assets or OS-level SendKeys — inject directly
into the running PIE session (no OS window focus needed):

```python
import unreal

# Enhanced Input action, QUEUED for the next input tick and released on the one after.
# X/Y/Z map to the value type. Read the result in a later call: nothing ticks while this one runs.
print(unreal.InputService.inject_action("/Game/Input/IA_Fire", 1.0))

# Hold an action INDEFINITELY, and release it in a later call once you have seen what you need
# (hold forward, read where the pawn got to, then let go). Calling it again restarts the hold.
print(unreal.InputService.start_injection("/Game/Input/IA_Block"))
# ... later call ...
print(unreal.InputService.stop_injection("/Game/Input/IA_Block"))
```

A press or hold of **known length** — and any raw key — is Epic's `InputToolset.ApplyInput`
(`inject_action_for` and `inject_key` are gone). It takes `entries` plus `durationSeconds` (`0` = a
one-tick tap, `> 0` = held that long, then released) and can press several actions or keys together:

```
call_tool InputToolset.ApplyInput {"entries": [{"action": {"refPath": "/Game/Input/IA_Block.IA_Block"},
                                               "value": {"x": 1, "y": 0, "z": 0}}], "durationSeconds": 1.5}
call_tool InputToolset.ApplyInput {"entries": [{"key": {"keyName": "SpaceBar"}, "value": {"x": 1, "y": 0, "z": 0}}],
                                   "durationSeconds": 0}
```

The VibeUE calls return JSON with `success`/`error_code` (PIE not running, `NO_LOCAL_PLAYER` /
`NO_PLAYER_CONTROLLER` while PIE or a client's join is still starting — retry on a later call).
`inject_action` reports `queued: true`: it was queued, not yet applied. They take `pie_instance` to
pick a PIE world: -1, the default, is the first PIE world with a local player (the lowest instance
number), and replies report the instance actually used. A `start_injection` hold is one hold per
action and PIE world whatever the path spelling, so `stop_injection` finds it either way; ending PIE
drops every hold. An `ApplyInput` hold is Epic's own and ends on its `durationSeconds`. See the
`pie-testing` skill for the full verification loop.

## Reference pages

Open these with the paths below; UE rewrites them to absolute paths when it serves this skill.

- [scripts/setup_input.txt](scripts/setup_input.txt) - runnable sample

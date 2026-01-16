# Enhanced Input Workflows

---

## Create Input Action

```python
import unreal

# Create Boolean action (simple press)
result = unreal.InputService.create_action("Jump", "/Game/Input", "Boolean")
if result.success:
    print(f"Created: {result.asset_path}")

# Create Axis2D action (movement)
result = unreal.InputService.create_action("Move", "/Game/Input", "Axis2D")

# Save
unreal.EditorAssetLibrary.save_asset(result.asset_path)
```

---

## Create Mapping Context

```python
import unreal

# Create context with priority 0
result = unreal.InputService.create_mapping_context("Default", "/Game/Input", 0)
context_path = result.asset_path

# Save
unreal.EditorAssetLibrary.save_asset(context_path)
```

---

## Add Key Mapping

```python
import unreal

context_path = "/Game/Input/IMC_Default"
action_path = "/Game/Input/IA_Jump"

# Add keyboard binding
unreal.InputService.add_key_mapping(context_path, action_path, "SpaceBar")

# Add gamepad binding
unreal.InputService.add_key_mapping(context_path, action_path, "Gamepad_FaceButton_Bottom")

# Save
unreal.EditorAssetLibrary.save_asset(context_path)
```

---

## Add Triggers and Modifiers

```python
import unreal

context_path = "/Game/Input/IMC_Default"
action_path = "/Game/Input/IA_Fire"

# First add the key mapping
unreal.InputService.add_key_mapping(context_path, action_path, "LeftMouseButton")

# Get mappings to find the index
mappings = unreal.InputService.get_mappings(context_path)
mapping_index = len(mappings) - 1  # Last added mapping

# Add trigger using mapping index
unreal.InputService.add_trigger(context_path, mapping_index, "Pressed")

# Add modifier using mapping index
unreal.InputService.add_modifier(context_path, mapping_index, "DeadZone")

# Save
unreal.EditorAssetLibrary.save_asset(context_path)
```

---

## Get Mappings Info

```python
import unreal

# Get all mappings in context
mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Default")
for m in mappings:
    print(f"Action: {m.action_name}, Key: {m.key_name}")

# Get action info - use get_input_action_info
info = unreal.InputService.get_input_action_info("/Game/Input/IA_Jump")
if info:
    print(f"Action: {info.action_name}, ValueType: {info.value_type}")
```

---

## Discover Available Keys/Triggers/Modifiers

```python
import unreal

# List all available keys
keys = unreal.InputService.get_available_keys()
for k in keys[:20]:
    print(k)

# Discover types to get triggers and modifiers
types = unreal.InputService.discover_types()
print(f"Value Types: {types.action_value_types}")
print(f"Modifiers: {types.modifier_types}")
print(f"Triggers: {types.trigger_types}")

# Alternative: get from direct methods
modifiers = unreal.InputService.get_available_modifier_types()
triggers = unreal.InputService.get_available_trigger_types()

# Get modifiers on a mapping (use type_name or display_name)
mods = unreal.InputService.get_modifiers(context_path, 0)
for mod in mods:
    print(f"Modifier: {mod.type_name} ({mod.display_name})")

# Get triggers on a mapping (use type_name or display_name)
trigs = unreal.InputService.get_triggers(context_path, 0)
for trig in trigs:
    print(f"Trigger: {trig.type_name} ({trig.display_name})")
```

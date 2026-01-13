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

# Add trigger
unreal.InputService.add_trigger(context_path, action_path, "LeftMouseButton", "Pressed")

# Add modifier (for analog inputs)
unreal.InputService.add_modifier(context_path, action_path, "Gamepad_RightTrigger", "DeadZone")

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
    print(f"Action: {m.action_name}, Key: {m.key}")

# Get action info
info = unreal.InputService.get_action_info("/Game/Input/IA_Jump")
if info:
    print(f"Action: {info.name}, ValueType: {info.value_type}")
```

---

## Discover Available Keys/Triggers/Modifiers

```python
import unreal

# List all available keys
keys = unreal.InputService.get_available_keys()
for k in keys[:20]:
    print(k)

# List triggers
triggers = unreal.InputService.get_available_triggers()
for t in triggers:
    print(t)

# List modifiers
modifiers = unreal.InputService.get_available_modifiers()
for m in modifiers:
    print(m)
```

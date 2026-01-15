# Enhanced Input Common Mistakes

Avoid these frequent errors when working with the Enhanced Input API.

---

## ❌ Wrong Method Names

### Don't use these (they don't exist):
```python
# ❌ WRONG - These methods don't exist
unreal.InputService.get_action_info()
unreal.InputService.get_action_properties()
unreal.InputService.action_get_properties()
unreal.InputService.get_available_triggers()
unreal.InputService.get_available_modifiers()
```

### ✅ Use these instead:
```python
# ✅ CORRECT
info = unreal.InputService.get_input_action_info(action_path)
types = unreal.InputService.discover_types()
triggers = unreal.InputService.get_available_trigger_types()
modifiers = unreal.InputService.get_available_modifier_types()
```

---

## ❌ Wrong Property Names

### Don't use:
```python
# ❌ WRONG
types = unreal.InputService.discover_types()
print(types.value_types)  # AttributeError!
```

### ✅ Use correct property:
```python
# ✅ CORRECT
types = unreal.InputService.discover_types()
print(types.action_value_types)  # Correct property name
```

---

## ❌ Wrong Parameter Names

### Don't use:
```python
# ❌ WRONG
unreal.InputService.configure_action(
    action_path,
    action_description="Jump action"  # Wrong parameter name!
)
```

### ✅ Use correct parameter:
```python
# ✅ CORRECT
unreal.InputService.configure_action(
    action_path,
    description="Jump action"  # Correct parameter name
)
```

---

## ❌ Passing Action Path Instead of Mapping Index

### Don't do this:
```python
# ❌ WRONG - add_trigger/add_modifier need mapping_index, not action_path
context_path = "/Game/Input/IMC_Default"
action_path = "/Game/Input/IA_Jump"

unreal.InputService.add_trigger(context_path, action_path, "Pressed")  # ERROR!
unreal.InputService.add_modifier(context_path, action_path, "DeadZone")  # ERROR!
```

### ✅ Do this instead:
```python
# ✅ CORRECT - Get mapping index first
context_path = "/Game/Input/IMC_Default"
action_path = "/Game/Input/IA_Jump"

# First add the key mapping
unreal.InputService.add_key_mapping(context_path, action_path, "SpaceBar")

# Then get the mappings to find the index
mappings = unreal.InputService.get_mappings(context_path)
mapping_index = len(mappings) - 1  # Last added mapping

# Now add trigger/modifier using the index
unreal.InputService.add_trigger(context_path, mapping_index, "Pressed")
unreal.InputService.add_modifier(context_path, mapping_index, "DeadZone")
```

---

## ❌ Not Finding the Correct Mapping Index

### Don't assume:
```python
# ❌ WRONG - Don't assume index 0 is what you want
unreal.InputService.add_trigger("/Game/Input/IMC_Default", 0, "Pressed")
```

### ✅ Find the specific mapping:
```python
# ✅ CORRECT - Find the mapping you want to modify
context_path = "/Game/Input/IMC_Default"
target_action = "/Game/Input/IA_Jump"
target_key = "SpaceBar"

mappings = unreal.InputService.get_mappings(context_path)
for i, mapping in enumerate(mappings):
    if mapping.action_name == target_action and mapping.key == target_key:
        # Found it! Now modify it
        unreal.InputService.add_trigger(context_path, i, "Pressed")
        break
```

---

## ❌ Wrong Value Type Names

### Don't use:
```python
# ❌ WRONG - These value types don't exist
unreal.InputService.create_action("Jump", "/Game/Input", "Bool")  # Wrong!
unreal.InputService.create_action("Jump", "/Game/Input", "Digital")  # Also wrong!
```

### ✅ Use correct value types:
```python
# ✅ CORRECT - Use exact value type strings
unreal.InputService.create_action("Jump", "/Game/Input", "Boolean")  # ✓
unreal.InputService.create_action("Throttle", "/Game/Input", "Axis1D")  # ✓
unreal.InputService.create_action("Move", "/Game/Input", "Axis2D")  # ✓
unreal.InputService.create_action("3D", "/Game/Input", "Axis3D")  # ✓
```

---

## ❌ Wrong Key Names

### Don't use:
```python
# ❌ WRONG - Incorrect key names
unreal.InputService.add_key_mapping(context, action, "Space")  # Wrong!
unreal.InputService.add_key_mapping(context, action, "Shift")  # Wrong!
unreal.InputService.add_key_mapping(context, action, "Mouse1")  # Wrong!
unreal.InputService.add_key_mapping(context, action, "w")  # Wrong case!
```

### ✅ Use correct Unreal key names:
```python
# ✅ CORRECT - Exact Unreal key names
unreal.InputService.add_key_mapping(context, action, "SpaceBar")  # ✓
unreal.InputService.add_key_mapping(context, action, "LeftShift")  # ✓
unreal.InputService.add_key_mapping(context, action, "LeftMouseButton")  # ✓
unreal.InputService.add_key_mapping(context, action, "W")  # Uppercase!
```

**Tip:** Use `get_available_keys()` to see all valid key names:
```python
keys = unreal.InputService.get_available_keys("Mouse")
print(keys)  # Shows all mouse-related keys
```

---

## ❌ Incomplete Asset Paths

### Don't use:
```python
# ❌ WRONG - Missing /Game/ prefix
action_path = "Input/IA_Jump"
unreal.InputService.get_input_action_info(action_path)  # Won't find it!
```

### ✅ Use full asset paths:
```python
# ✅ CORRECT - Full path from /Game/
action_path = "/Game/Input/IA_Jump"
info = unreal.InputService.get_input_action_info(action_path)
```

---

## ❌ Forgetting to Save Assets

### Don't forget:
```python
# ❌ WRONG - Changes made but not saved
unreal.InputService.create_action("Jump", "/Game/Input", "Boolean")
# Asset created but not saved - may be lost!
```

### ✅ Always save after modifications:
```python
# ✅ CORRECT - Save after creating or modifying
result = unreal.InputService.create_action("Jump", "/Game/Input", "Boolean")
unreal.EditorAssetLibrary.save_asset(result.asset_path)  # Save it!

# Also save contexts after adding mappings
unreal.InputService.add_key_mapping(context_path, action_path, "SpaceBar")
unreal.EditorAssetLibrary.save_asset(context_path)  # Save context!
```

---

## ❌ Wrong Modifier/Trigger Type Names

### Don't use:
```python
# ❌ WRONG - These types don't exist
unreal.InputService.add_modifier(context, index, "Invert")  # Wrong!
unreal.InputService.add_trigger(context, index, "Press")  # Wrong!
unreal.InputService.add_trigger(context, index, "Click")  # Wrong!
```

### ✅ Use exact type names:
```python
# ✅ CORRECT - Discover available types first
types = unreal.InputService.discover_types()
print(f"Modifiers: {types.modifier_types}")
print(f"Triggers: {types.trigger_types}")

# Common ones:
unreal.InputService.add_modifier(context, index, "Negate")  # ✓ (not Invert)
unreal.InputService.add_trigger(context, index, "Pressed")  # ✓ (not Press)
unreal.InputService.add_trigger(context, index, "Tap")  # ✓ (not Click)
```

---

## ❌ Trying to Add Modifier/Trigger Before Key Mapping

### Don't do this:
```python
# ❌ WRONG - Trying to add trigger before mapping exists
context_path = "/Game/Input/IMC_Default"
unreal.InputService.add_trigger(context_path, 0, "Pressed")  # No mapping at index 0!
```

### ✅ Add key mapping first:
```python
# ✅ CORRECT - Create mapping, then add trigger
context_path = "/Game/Input/IMC_Default"
action_path = "/Game/Input/IA_Jump"

# 1. Add key mapping first
unreal.InputService.add_key_mapping(context_path, action_path, "SpaceBar")

# 2. Get the new mapping's index
mappings = unreal.InputService.get_mappings(context_path)
mapping_index = len(mappings) - 1

# 3. Now add trigger/modifier
unreal.InputService.add_trigger(context_path, mapping_index, "Pressed")
```

---

## Quick Reference: Correct API Patterns

```python
import unreal

# Discovery
types = unreal.InputService.discover_types()
keys = unreal.InputService.get_available_keys()
modifiers = unreal.InputService.get_available_modifier_types()
triggers = unreal.InputService.get_available_trigger_types()

# Action Management
result = unreal.InputService.create_action("IA_Name", "/Game/Input", "Boolean")
actions = unreal.InputService.list_input_actions()
info = unreal.InputService.get_input_action_info("/Game/Input/IA_Name")
unreal.InputService.configure_action(path, consume_input=True, description="Text")

# Context Management
result = unreal.InputService.create_mapping_context("IMC_Name", "/Game/Input", 0)
contexts = unreal.InputService.list_mapping_contexts()
mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Name")

# Key Mappings
unreal.InputService.add_key_mapping(context_path, action_path, "KeyName")
unreal.InputService.remove_mapping(context_path, mapping_index)

# Modifiers/Triggers (require mapping_index!)
unreal.InputService.add_modifier(context_path, mapping_index, "ModifierType")
unreal.InputService.add_trigger(context_path, mapping_index, "TriggerType")
unreal.InputService.remove_modifier(context_path, mapping_index, modifier_index)
unreal.InputService.remove_trigger(context_path, mapping_index, trigger_index)

# Always save!
unreal.EditorAssetLibrary.save_asset(asset_path)
```

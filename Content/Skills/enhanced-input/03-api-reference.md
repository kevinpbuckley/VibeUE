# Enhanced Input API Reference

Complete API reference for `unreal.InputService` based on actual method signatures.

---

## Discovery Methods

### discover_types()
Discover all available Enhanced Input types.

```python
types = unreal.InputService.discover_types()
# Returns InputTypeDiscoveryResult with properties:
#   - action_value_types: ["Boolean", "Axis1D", "Axis2D", "Axis3D"]
#   - modifier_types: ["Negate", "DeadZone", "Scalar", ...]
#   - trigger_types: ["Pressed", "Released", "Hold", "Tap", ...]

print(f"Value Types: {types.action_value_types}")
print(f"Modifiers: {types.modifier_types}")
print(f"Triggers: {types.trigger_types}")
```

### get_available_keys(filter="")
Get list of all available input keys.

```python
# Get all keys
all_keys = unreal.InputService.get_available_keys()

# Filter by name
gamepad_keys = unreal.InputService.get_available_keys("Gamepad")
mouse_keys = unreal.InputService.get_available_keys("Mouse")
```

### get_available_modifier_types()
Get list of available modifier types.

```python
modifiers = unreal.InputService.get_available_modifier_types()
# Returns: ["SmoothDelta", "DeadZone", "Scalar", "Negate", ...]
```

### get_available_trigger_types()
Get list of available trigger types.

```python
triggers = unreal.InputService.get_available_trigger_types()
# Returns: ["Down", "Pressed", "Released", "Hold", "Tap", ...]
```

---

## Action Management

### create_action(action_name, asset_path, value_type="Axis1D")
Create a new Input Action asset.

**Parameters:**
- `action_name` (str): Name for the action (e.g., "IA_Jump")
- `asset_path` (str): Folder path (e.g., "/Game/Input")
- `value_type` (str): "Boolean", "Axis1D", "Axis2D", or "Axis3D"

**Returns:** `InputCreateResult` with `.asset_path` property

```python
result = unreal.InputService.create_action("IA_Jump", "/Game/Input", "Boolean")
if result.success:
    print(f"Created at: {result.asset_path}")
```

### list_input_actions()
List all Input Action assets in the project.

```python
actions = unreal.InputService.list_input_actions()
for action in actions:
    print(action)  # Full asset path
```

### get_input_action_info(action_path)
Get detailed properties of an Input Action.

**Parameters:**
- `action_path` (str): Full path to action (e.g., "/Game/Input/IA_Jump")

**Returns:** `InputActionDetailedInfo` or `None`

```python
info = unreal.InputService.get_input_action_info("/Game/Input/IA_Jump")
if info:
    print(f"Name: {info.name}")
    print(f"Value Type: {info.value_type}")
    print(f"Consumes Input: {info.consume_input}")
    print(f"Description: {info.description}")
```

### configure_action(action_path, consume_input=True, trigger_when_paused=False, description="")
Configure Input Action properties.

**Parameters:**
- `action_path` (str): Full path to action
- `consume_input` (bool): Whether action consumes input
- `trigger_when_paused` (bool): Trigger when game paused
- `description` (str): Action description text

**Returns:** `bool` (True if successful)

```python
success = unreal.InputService.configure_action(
    "/Game/Input/IA_Jump",
    consume_input=True,
    trigger_when_paused=False,
    description="Jump action for the player"
)
```

---

## Mapping Context Management

### create_mapping_context(context_name, asset_path, priority=0)
Create a new Input Mapping Context.

**Parameters:**
- `context_name` (str): Name for context (e.g., "IMC_Default")
- `asset_path` (str): Folder path (e.g., "/Game/Input")
- `priority` (int32): Context priority (higher = processed first)

**Returns:** `InputCreateResult` with `.asset_path` property

```python
result = unreal.InputService.create_mapping_context("IMC_Default", "/Game/Input", 0)
context_path = result.asset_path
```

### list_mapping_contexts()
List all Mapping Context assets in the project.

```python
contexts = unreal.InputService.list_mapping_contexts()
for context in contexts:
    print(context)  # Full asset path
```

### get_mapping_context_info(context_path)
Get detailed information about a Mapping Context including all mappings.

**Parameters:**
- `context_path` (str): Full path to context

**Returns:** `MappingContextDetailedInfo` or `None`

```python
info = unreal.InputService.get_mapping_context_info("/Game/Input/IMC_Default")
if info:
    print(f"Context: {info.name}")
    print(f"Priority: {info.priority}")
    print(f"Mappings: {len(info.mappings)}")
    for mapping in info.mappings:
        print(f"  {mapping.key} -> {mapping.action_name}")
```

### get_mappings(context_path)
Get all key mappings in a context.

**Parameters:**
- `context_path` (str): Full path to context

**Returns:** `Array[KeyMappingInfo]`

```python
mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Default")
for i, mapping in enumerate(mappings):
    print(f"[{i}] Key: {mapping.key_name}, Action: {mapping.action_name}")
```

---

## Key Mapping Operations

### add_key_mapping(context_path, action_path, key_name)
Add a key mapping to a context.

**Parameters:**
- `context_path` (str): Full path to Mapping Context
- `action_path` (str): Full path to Input Action
- `key_name` (str): Key name (e.g., "SpaceBar", "W", "Gamepad_LeftTrigger")

**Returns:** `bool` (True if successful)

```python
success = unreal.InputService.add_key_mapping(
    "/Game/Input/IMC_Default",
    "/Game/Input/IA_Jump",
    "SpaceBar"
)
```

### remove_mapping(context_path, mapping_index)
Remove a key mapping from a context by index.

**Parameters:**
- `context_path` (str): Full path to Mapping Context
- `mapping_index` (int32): Index of mapping to remove (from `get_mappings()`)

**Returns:** `bool` (True if successful)

```python
# Get mappings first to find index
mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Default")
# Remove first mapping
success = unreal.InputService.remove_mapping("/Game/Input/IMC_Default", 0)
```

---

## Modifier Management

### add_modifier(context_path, mapping_index, modifier_type)
Add a modifier to a key mapping.

**Parameters:**
- `context_path` (str): Full path to Mapping Context
- `mapping_index` (int32): Index of the mapping
- `modifier_type` (str): Modifier type (e.g., "Negate", "DeadZone", "Scalar")

**Returns:** `bool` (True if successful)

```python
# Get mapping index first
mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Default")
mapping_index = 0

success = unreal.InputService.add_modifier(
    "/Game/Input/IMC_Default",
    mapping_index,
    "DeadZone"
)
```

### get_modifiers(context_path, mapping_index)
Get modifiers on a key mapping.

**Parameters:**
- `context_path` (str): Full path to Mapping Context
- `mapping_index` (int32): Index of the mapping

**Returns:** `Array[InputModifierInfo]`

```python
modifiers = unreal.InputService.get_modifiers("/Game/Input/IMC_Default", 0)
for i, mod in enumerate(modifiers):
    print(f"[{i}] Modifier: {mod.type}")
```

### remove_modifier(context_path, mapping_index, modifier_index)
Remove a modifier from a key mapping.

**Parameters:**
- `context_path` (str): Full path to Mapping Context
- `mapping_index` (int32): Index of the mapping
- `modifier_index` (int32): Index of the modifier to remove

**Returns:** `bool` (True if successful)

```python
success = unreal.InputService.remove_modifier(
    "/Game/Input/IMC_Default",
    mapping_index=0,
    modifier_index=0
)
```

---

## Trigger Management

### add_trigger(context_path, mapping_index, trigger_type)
Add a trigger to a key mapping.

**Parameters:**
- `context_path` (str): Full path to Mapping Context
- `mapping_index` (int32): Index of the mapping
- `trigger_type` (str): Trigger type (e.g., "Pressed", "Released", "Hold", "Tap")

**Returns:** `bool` (True if successful)

```python
# Get mapping index first
mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Default")
mapping_index = 0

success = unreal.InputService.add_trigger(
    "/Game/Input/IMC_Default",
    mapping_index,
    "Pressed"
)
```

### get_triggers(context_path, mapping_index)
Get triggers on a key mapping.

**Parameters:**
- `context_path` (str): Full path to Mapping Context
- `mapping_index` (int32): Index of the mapping

**Returns:** `Array[InputTriggerInfo]`

```python
triggers = unreal.InputService.get_triggers("/Game/Input/IMC_Default", 0)
for i, trigger in enumerate(triggers):
    print(f"[{i}] Trigger: {trigger.type}")
```

### remove_trigger(context_path, mapping_index, trigger_index)
Remove a trigger from a key mapping.

**Parameters:**
- `context_path` (str): Full path to Mapping Context
- `mapping_index` (int32): Index of the mapping
- `trigger_index` (int32): Index of the trigger to remove

**Returns:** `bool` (True if successful)

```python
success = unreal.InputService.remove_trigger(
    "/Game/Input/IMC_Default",
    mapping_index=0,
    trigger_index=0
)
```

---

## Common Patterns

### Complete Mapping Setup with Trigger and Modifier

```python
import unreal

# 1. Create action if needed
action_result = unreal.InputService.create_action("IA_Fire", "/Game/Input", "Boolean")

# 2. Create or use existing context
context_path = "/Game/Input/IMC_Combat"

# 3. Add key mapping
unreal.InputService.add_key_mapping(
    context_path,
    "/Game/Input/IA_Fire",
    "LeftMouseButton"
)

# 4. Get mapping index (it's the last one we just added)
mappings = unreal.InputService.get_mappings(context_path)
mapping_index = len(mappings) - 1

# 5. Add trigger
unreal.InputService.add_trigger(context_path, mapping_index, "Pressed")

# 6. Add modifier (if needed for analog inputs)
unreal.InputService.add_modifier(context_path, mapping_index, "DeadZone")

# 7. Save
unreal.EditorAssetLibrary.save_asset(context_path)
```

### Finding a Specific Mapping Index

```python
import unreal

context_path = "/Game/Input/IMC_Default"
target_action = "/Game/Input/IA_Jump"
target_key = "SpaceBar"

mappings = unreal.InputService.get_mappings(context_path)
for i, mapping in enumerate(mappings):
    if mapping.action_name == target_action and mapping.key_name == target_key:
        print(f"Found mapping at index {i}")
        # Now you can add/remove modifiers/triggers using index i
        unreal.InputService.add_trigger(context_path, i, "Pressed")
        break
```

---

## Important Notes

1. **Mapping Index**: Most modifier/trigger operations require a `mapping_index`, not `action_path`. Always call `get_mappings()` first to find the correct index.

2. **Save Assets**: Remember to save assets after modifications:
   ```python
   unreal.EditorAssetLibrary.save_asset(asset_path)
   ```

3. **Asset Paths**: Use full asset paths including folders:
   - ✅ `/Game/Input/IA_Jump`
   - ❌ `IA_Jump`

4. **Value Types**: When creating actions, use exact value type strings:
   - "Boolean" for button presses
   - "Axis1D" for single axis (throttle, zoom)
   - "Axis2D" for dual axis (movement, look)
   - "Axis3D" for 3D manipulation

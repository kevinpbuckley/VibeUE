# InputService API Reference

All methods are called via `unreal.InputService.<method_name>(...)`.

**ALWAYS use `discover_python_class("unreal.InputService")` for parameter details before calling.**

---

## Reflection & Discovery

### discover_types()
Get available value types, modifiers, and triggers.

**Returns:** InputTypeDiscoveryResult

**Example:**
```python
import unreal

types = unreal.InputService.discover_types()
print("Value Types:", types.action_value_types)  # ["Boolean", "Axis1D", "Axis2D", "Axis3D"]
print("Modifiers:", types.modifier_types)
print("Triggers:", types.trigger_types)
```

---

## Input Action Management

### create_action(name, path, value_type)
Create a new Input Action.

**Parameters:**
- `name`: Action name (e.g., "IA_Jump")
- `path`: Destination path (e.g., "/Game/Input")
- `value_type`: "Boolean", "Axis1D", "Axis2D", or "Axis3D"

**Returns:** InputCreateResult with `.success`, `.asset_path`, `.error_message`

**Example:**
```python
import unreal

result = unreal.InputService.create_action("IA_Jump", "/Game/Input", "Boolean")
if result.success:
    print(f"Created: {result.asset_path}")
else:
    print(f"Error: {result.error_message}")
```

### list_input_actions()
Get all Input Action asset paths.

**Returns:** Array[str]

**Example:**
```python
import unreal

actions = unreal.InputService.list_input_actions()
for action_path in actions:
    print(action_path)
```

### get_input_action_info(path)
Get detailed information about an Input Action.

**Returns:** InputActionDetailedInfo or None

**Example:**
```python
import unreal

info = unreal.InputService.get_input_action_info("/Game/Input/IA_Jump")
if info:
    print(f"Name: {info.action_name}")
    print(f"Value Type: {info.value_type}")
    print(f"Consumes Input: {info.consume_input}")
    print(f"Description: {info.description}")
```

### configure_action(path, consume_input, trigger_when_paused, description)
Configure Input Action properties.

**Returns:** bool

**Example:**
```python
import unreal

unreal.InputService.configure_action(
    "/Game/Input/IA_Jump",
    True,              # consume_input
    False,             # trigger_when_paused
    "Player jump action"
)
```

---

## Mapping Context Management

### create_mapping_context(name, path, priority)
Create a new Input Mapping Context.

**Parameters:**
- `name`: Context name (e.g., "IMC_Default")
- `path`: Destination path (e.g., "/Game/Input")
- `priority`: Int (higher = processed first, typically 0)

**Returns:** InputCreateResult

**Example:**
```python
import unreal

result = unreal.InputService.create_mapping_context("IMC_Default", "/Game/Input", 0)
if result.success:
    print(f"Created: {result.asset_path}")
```

### list_mapping_contexts()
Get all Mapping Context asset paths.

**Returns:** Array[str]

### get_mapping_context_info(path)
Get detailed information about a Mapping Context.

**Returns:** MappingContextDetailedInfo or None

**Example:**
```python
import unreal

info = unreal.InputService.get_mapping_context_info("/Game/Input/IMC_Default")
if info:
    print(f"Name: {info.context_name}")
    print(f"Priority: {info.priority}")
    print(f"Mapped Actions: {info.mapped_actions}")
```

### get_mappings(context_path)
Get all key mappings in a Mapping Context.

**Returns:** Array[KeyMappingInfo]

**Example:**
```python
import unreal

mappings = unreal.InputService.get_mappings("/Game/Input/IMC_Default")
for m in mappings:
    print(f"[{m.mapping_index}] {m.action_name} → {m.key_name}")
    print(f"  Modifiers: {m.modifier_count}, Triggers: {m.trigger_count}")
```

### add_key_mapping(context_path, action_path, key_name)
Add a key mapping to a Mapping Context.

**Returns:** bool

**Example:**
```python
import unreal

# Add SpaceBar to Jump action
unreal.InputService.add_key_mapping(
    "/Game/Input/IMC_Default",
    "/Game/Input/IA_Jump",
    "SpaceBar"
)

# Add gamepad button
unreal.InputService.add_key_mapping(
    "/Game/Input/IMC_Default",
    "/Game/Input/IA_Jump",
    "Gamepad_FaceButton_Bottom"  # A button
)
```

### remove_mapping(context_path, mapping_index)
Remove a key mapping by index.

**Returns:** bool

**Example:**
```python
import unreal

# Remove the first mapping
unreal.InputService.remove_mapping("/Game/Input/IMC_Default", 0)
```

### get_available_keys(filter="")
Get available key names, optionally filtered.

**Returns:** Array[str]

**Example:**
```python
import unreal

# Get all keys
all_keys = unreal.InputService.get_available_keys("")

# Get gamepad keys only
gamepad_keys = unreal.InputService.get_available_keys("Gamepad")
for key in gamepad_keys:
    print(key)

# Get mouse keys
mouse_keys = unreal.InputService.get_available_keys("Mouse")
```

---

## Modifier Management

### add_modifier(context_path, mapping_index, modifier_type)
Add a modifier to a key mapping.

**Parameters:**
- `modifier_type`: e.g., "Negate", "DeadZone", "Scalar", "Swizzle"

**Returns:** bool

**Example:**
```python
import unreal

# Add dead zone to first mapping
unreal.InputService.add_modifier(
    "/Game/Input/IMC_Default",
    0,             # mapping_index
    "DeadZone"
)
```

### remove_modifier(context_path, mapping_index, modifier_index)
Remove a modifier by index.

**Returns:** bool

### get_modifiers(context_path, mapping_index)
Get all modifiers on a key mapping.

**Returns:** Array[InputModifierInfo]

**Example:**
```python
import unreal

mods = unreal.InputService.get_modifiers("/Game/Input/IMC_Default", 0)
for m in mods:
    print(f"[{m.modifier_index}] {m.display_name} ({m.type_name})")
```

### get_available_modifier_types()
Get all available modifier type names.

**Returns:** Array[str]

**Example:**
```python
import unreal

modifiers = unreal.InputService.get_available_modifier_types()
for mod in modifiers:
    print(mod)
# Output: "Negate", "DeadZone", "Scalar", "Swizzle", etc.
```

---

## Trigger Management

### add_trigger(context_path, mapping_index, trigger_type)
Add a trigger to a key mapping.

**Parameters:**
- `trigger_type`: e.g., "Pressed", "Released", "Hold", "Tap", "Pulse"

**Returns:** bool

**Example:**
```python
import unreal

# Add Pressed trigger
unreal.InputService.add_trigger(
    "/Game/Input/IMC_Default",
    0,            # mapping_index
    "Pressed"
)

# Add Hold trigger
unreal.InputService.add_trigger(
    "/Game/Input/IMC_Default",
    1,
    "Hold"
)
```

### remove_trigger(context_path, mapping_index, trigger_index)
Remove a trigger by index.

**Returns:** bool

### get_triggers(context_path, mapping_index)
Get all triggers on a key mapping.

**Returns:** Array[InputTriggerInfo]

**Example:**
```python
import unreal

triggers = unreal.InputService.get_triggers("/Game/Input/IMC_Default", 0)
for t in triggers:
    print(f"[{t.trigger_index}] {t.display_name} ({t.type_name})")
```

### get_available_trigger_types()
Get all available trigger type names.

**Returns:** Array[str]

**Example:**
```python
import unreal

triggers = unreal.InputService.get_available_trigger_types()
for trig in triggers:
    print(trig)
# Output: "Pressed", "Released", "Hold", "Tap", "Pulse", "Down", etc.
```

---

## Common Value Types

- **Boolean**: Simple on/off (jump, fire, interact)
- **Axis1D**: Single axis (-1 to 1) (throttle, zoom)
- **Axis2D**: Two axes (movement, look)
- **Axis3D**: Three axes (rarely used)

---

## Common Triggers

- **Pressed**: Triggers once when key pressed
- **Released**: Triggers once when key released
- **Down**: Triggers every frame while held
- **Hold**: Triggers after holding for duration
- **Tap**: Triggers on quick press-release
- **Pulse**: Triggers repeatedly while held

---

## Common Modifiers

- **Negate**: Inverts input value
- **DeadZone**: Ignores small inputs (prevents stick drift)
- **Scalar**: Multiplies input by a value
- **Swizzle**: Rearranges/inverts axes (e.g., swap X/Y)
- **Smooth**: Smooths input over time

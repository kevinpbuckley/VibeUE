# Enhanced Input Workflows

Common patterns for working with Input Actions and Mapping Contexts.

---

## Create Input Action with Configuration

```python
import unreal

# 1. Create Input Action
result = unreal.InputService.create_action("IA_Jump", "/Game/Input", "Boolean")

if result.success:
    action_path = result.asset_path

    # 2. Configure action properties
    unreal.InputService.configure_action(
        action_path,
        True,                    # consume_input
        False,                   # trigger_when_paused
        "Player jump action"     # description
    )

    print(f"Created and configured: {action_path}")
else:
    print(f"Failed: {result.error_message}")
```

---

## Create Mapping Context with Key Bindings

```python
import unreal

# 1. Create Mapping Context
result = unreal.InputService.create_mapping_context("IMC_Default", "/Game/Input", 0)

if result.success:
    context_path = result.asset_path

    # 2. Create actions first
    jump_result = unreal.InputService.create_action("IA_Jump", "/Game/Input", "Boolean")
    move_result = unreal.InputService.create_action("IA_Move", "/Game/Input", "Axis2D")

    # 3. Add key mappings
    unreal.InputService.add_key_mapping(context_path, jump_result.asset_path, "SpaceBar")
    unreal.InputService.add_key_mapping(context_path, jump_result.asset_path, "Gamepad_FaceButton_Bottom")

    # 4. Add movement mappings
    unreal.InputService.add_key_mapping(context_path, move_result.asset_path, "W")
    unreal.InputService.add_key_mapping(context_path, move_result.asset_path, "Gamepad_LeftY")

    print(f"Created mapping context with bindings: {context_path}")
```

---

## Add Triggers and Modifiers to Mappings

```python
import unreal

context_path = "/Game/Input/IMC_Default"

# 1. Get all mappings
mappings = unreal.InputService.get_mappings(context_path)

# 2. Find the Jump action mapping
for m in mappings:
    if m.action_name == "IA_Jump":
        # Add Pressed trigger
        unreal.InputService.add_trigger(context_path, m.mapping_index, "Pressed")
        print(f"Added Pressed trigger to {m.action_name} on {m.key_name}")

# 3. Find movement mapping and add dead zone
for m in mappings:
    if m.action_name == "IA_Move" and "Gamepad" in m.key_name:
        # Add dead zone modifier for gamepad
        unreal.InputService.add_modifier(context_path, m.mapping_index, "DeadZone")
        print(f"Added DeadZone modifier to {m.action_name} on {m.key_name}")
```

---

## Inspect Input Configuration

```python
import unreal

# 1. List all Input Actions
print("=== Input Actions ===")
actions = unreal.InputService.list_input_actions()
for action_path in actions:
    info = unreal.InputService.get_input_action_info(action_path)
    if info:
        print(f"{info.action_name}:")
        print(f"  Type: {info.value_type}")
        print(f"  Consumes: {info.consume_input}")
        print(f"  Description: {info.description}")

# 2. List all Mapping Contexts
print("\n=== Mapping Contexts ===")
contexts = unreal.InputService.list_mapping_contexts()
for context_path in contexts:
    info = unreal.InputService.get_mapping_context_info(context_path)
    if info:
        print(f"{info.context_name} (Priority: {info.priority}):")

        # 3. Get mappings in this context
        mappings = unreal.InputService.get_mappings(context_path)
        for m in mappings:
            print(f"  [{m.mapping_index}] {m.action_name} → {m.key_name}")
            print(f"      Modifiers: {m.modifier_count}, Triggers: {m.trigger_count}")
```

---

## Create Complete Input Setup

```python
import unreal

# 1. Discover available types
types = unreal.InputService.discover_types()
print(f"Available value types: {types.action_value_types}")

# 2. Create Input Actions
actions_to_create = [
    ("IA_Jump", "Boolean", "Player jump"),
    ("IA_Move", "Axis2D", "Player movement"),
    ("IA_Look", "Axis2D", "Camera look"),
    ("IA_Fire", "Boolean", "Fire weapon"),
    ("IA_Reload", "Boolean", "Reload weapon"),
]

created_actions = {}
for name, value_type, desc in actions_to_create:
    result = unreal.InputService.create_action(name, "/Game/Input", value_type)
    if result.success:
        created_actions[name] = result.asset_path
        unreal.InputService.configure_action(result.asset_path, True, False, desc)
        print(f"Created: {name}")

# 3. Create Mapping Context
ctx_result = unreal.InputService.create_mapping_context("IMC_Gameplay", "/Game/Input", 0)
context_path = ctx_result.asset_path

# 4. Add keyboard mappings
keyboard_mappings = [
    ("IA_Jump", "SpaceBar"),
    ("IA_Move", "W"),
    ("IA_Look", "Mouse2D"),
    ("IA_Fire", "LeftMouseButton"),
    ("IA_Reload", "R"),
]

for action_name, key_name in keyboard_mappings:
    if action_name in created_actions:
        unreal.InputService.add_key_mapping(context_path, created_actions[action_name], key_name)
        print(f"Mapped {key_name} to {action_name}")

# 5. Add gamepad mappings
gamepad_mappings = [
    ("IA_Jump", "Gamepad_FaceButton_Bottom"),
    ("IA_Move", "Gamepad_LeftY"),
    ("IA_Look", "Gamepad_RightX"),
    ("IA_Fire", "Gamepad_RightTriggerAxis"),
    ("IA_Reload", "Gamepad_FaceButton_Left"),
]

for action_name, key_name in gamepad_mappings:
    if action_name in created_actions:
        unreal.InputService.add_key_mapping(context_path, created_actions[action_name], key_name)
        print(f"Mapped {key_name} to {action_name}")

# 6. Add triggers to action mappings
mappings = unreal.InputService.get_mappings(context_path)
for m in mappings:
    if m.action_name in ["IA_Jump", "IA_Fire", "IA_Reload"]:
        # Boolean actions should trigger on press
        unreal.InputService.add_trigger(context_path, m.mapping_index, "Pressed")

# 7. Add modifiers for analog inputs
for m in mappings:
    if "Gamepad" in m.key_name and m.action_name in ["IA_Move", "IA_Look"]:
        # Add dead zone for gamepad sticks
        unreal.InputService.add_modifier(context_path, m.mapping_index, "DeadZone")

print(f"\nComplete input setup created at: {context_path}")
```

---

## Discover Available Keys

```python
import unreal

# 1. Get all available keys
all_keys = unreal.InputService.get_available_keys("")
print(f"Total keys available: {len(all_keys)}")

# 2. Get gamepad keys
gamepad_keys = unreal.InputService.get_available_keys("Gamepad")
print(f"\nGamepad keys ({len(gamepad_keys)}):")
for key in sorted(gamepad_keys):
    print(f"  {key}")

# 3. Get mouse keys
mouse_keys = unreal.InputService.get_available_keys("Mouse")
print(f"\nMouse keys ({len(mouse_keys)}):")
for key in sorted(mouse_keys):
    print(f"  {key}")

# 4. Search for specific keys
stick_keys = [k for k in all_keys if "Stick" in k or "stick" in k]
print(f"\nStick-related keys:")
for key in stick_keys:
    print(f"  {key}")
```

---

## Clone and Modify Mapping Context

```python
import unreal

source_path = "/Game/Input/IMC_Gameplay"
dest_path = "/Game/Input/IMC_GameplayAlt"

# 1. Duplicate the mapping context
unreal.AssetDiscoveryService.duplicate_asset(source_path, dest_path)

# 2. Get mappings from the new context
mappings = unreal.InputService.get_mappings(dest_path)

# 3. Remove specific mappings
for m in mappings:
    if m.key_name == "R":  # Remove R key mapping
        unreal.InputService.remove_mapping(dest_path, m.mapping_index)
        print(f"Removed mapping: {m.action_name} → {m.key_name}")
        break

# 4. Add new mapping
reload_action = "/Game/Input/IA_Reload"
unreal.InputService.add_key_mapping(dest_path, reload_action, "X")
print(f"Added new mapping: Reload → X")
```

---

## Add Hold Trigger for Charged Action

```python
import unreal

# 1. Create charged shot action
result = unreal.InputService.create_action("IA_ChargedShot", "/Game/Input", "Boolean")
action_path = result.asset_path

# 2. Configure action
unreal.InputService.configure_action(action_path, True, False, "Charged weapon shot")

# 3. Add to mapping context
context_path = "/Game/Input/IMC_Gameplay"
unreal.InputService.add_key_mapping(context_path, action_path, "LeftMouseButton")

# 4. Find the mapping index
mappings = unreal.InputService.get_mappings(context_path)
mapping_idx = None
for m in mappings:
    if m.action_name == "IA_ChargedShot":
        mapping_idx = m.mapping_index
        break

# 5. Add Hold trigger with duration
if mapping_idx is not None:
    unreal.InputService.add_trigger(context_path, mapping_idx, "Hold")
    print(f"Added Hold trigger to charged shot")
```

---

## Inspect Triggers and Modifiers

```python
import unreal

context_path = "/Game/Input/IMC_Gameplay"
mappings = unreal.InputService.get_mappings(context_path)

for m in mappings:
    print(f"\n{m.action_name} → {m.key_name} (Index: {m.mapping_index})")

    # Get triggers
    triggers = unreal.InputService.get_triggers(context_path, m.mapping_index)
    if triggers:
        print(f"  Triggers:")
        for t in triggers:
            print(f"    [{t.trigger_index}] {t.display_name} ({t.type_name})")

    # Get modifiers
    modifiers = unreal.InputService.get_modifiers(context_path, m.mapping_index)
    if modifiers:
        print(f"  Modifiers:")
        for mod in modifiers:
            print(f"    [{mod.modifier_index}] {mod.display_name} ({mod.type_name})")
```

---

## Remove Specific Trigger or Modifier

```python
import unreal

context_path = "/Game/Input/IMC_Gameplay"

# 1. Find mapping for Jump action
mappings = unreal.InputService.get_mappings(context_path)
jump_idx = None
for m in mappings:
    if m.action_name == "IA_Jump" and m.key_name == "SpaceBar":
        jump_idx = m.mapping_index
        break

if jump_idx is not None:
    # 2. Get current triggers
    triggers = unreal.InputService.get_triggers(context_path, jump_idx)

    # 3. Remove first trigger
    if triggers:
        unreal.InputService.remove_trigger(context_path, jump_idx, triggers[0].trigger_index)
        print(f"Removed trigger: {triggers[0].display_name}")

    # 4. Get current modifiers
    modifiers = unreal.InputService.get_modifiers(context_path, jump_idx)

    # 5. Remove first modifier
    if modifiers:
        unreal.InputService.remove_modifier(context_path, jump_idx, modifiers[0].modifier_index)
        print(f"Removed modifier: {modifiers[0].display_name}")
```

---

## Best Practices

1. **Always check InputCreateResult.success**
   ```python
   result = unreal.InputService.create_action("IA_Test", "/Game/Input", "Boolean")
   if result.success:
       # Use result.asset_path
   else:
       print(f"Error: {result.error_message}")
   ```

2. **Use mapping_index from KeyMappingInfo**
   ```python
   mappings = unreal.InputService.get_mappings(context_path)
   for m in mappings:
       # Use m.mapping_index for triggers/modifiers
       unreal.InputService.add_trigger(context_path, m.mapping_index, "Pressed")
   ```

3. **Verify keys exist before mapping**
   ```python
   available_keys = unreal.InputService.get_available_keys("Gamepad")
   if "Gamepad_LeftX" in available_keys:
       # Safe to map
   ```

4. **Use safe iteration with next()**
   ```python
   idx = next((i for i, m in enumerate(mappings) if m.action_name == "IA_Jump"), None)
   if idx is not None:
       # Safe to use idx
   ```

5. **Add dead zones to analog inputs**
   ```python
   # For gamepad sticks, always add dead zone
   if "Gamepad" in m.key_name and "Axis" in action_value_type:
       unreal.InputService.add_modifier(context_path, m.mapping_index, "DeadZone")
   ```

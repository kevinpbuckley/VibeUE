# Enhanced Input System Guide

## Overview

The `manage_enhanced_input` tool provides comprehensive control over Unreal Engine's Enhanced Input system. It consolidates 24+ actions across reflection, action management, and mapping services.

## Quick Reference

- **Discovery**: `get_help(topic="enhanced-input")`
- **Test Prompts**: See `test_prompts/enhanced_input/enhanced_input_test.md`

## Available Actions by Service

### Reflection Service

| Action | Purpose |
|--------|---------|
| `reflection_discover_types` | Discover all modifier/trigger types |

### Action Service

| Action | Purpose |
|--------|---------|
| `action_create` | Create new Input Action |
| `action_list` | List all Input Actions |
| `action_get_properties` | Get action properties |
| `action_configure` | Modify action properties |

### Mapping Service

| Action | Purpose |
|--------|---------|
| `mapping_create_context` | Create Input Mapping Context |
| `mapping_list_contexts` | List all mapping contexts |
| `mapping_get_properties` | Get context properties |
| `mapping_get_property` | Get single property value |
| `mapping_update_context` | Update context properties |
| `mapping_validate_context` | Validate context configuration |
| `mapping_get_mappings` | List key mappings in context |
| `mapping_add_key_mapping` | Add key binding |
| `mapping_remove_mapping` | Remove key mapping by index |
| `mapping_get_available_keys` | List all bindable keys |
| `mapping_add_modifier` | Add modifier to mapping |
| `mapping_remove_modifier` | Remove modifier from mapping |
| `mapping_get_modifiers` | List modifiers on mapping |
| `mapping_get_available_modifier_types` | List modifier types |
| `mapping_add_trigger` | Add trigger to mapping |
| `mapping_remove_trigger` | Remove trigger from mapping |
| `mapping_get_triggers` | List triggers on mapping |
| `mapping_get_available_trigger_types` | List trigger types |
| `mapping_analyze_usage` | Analyze context usage |
| `mapping_detect_conflicts` | Detect key conflicts |

## Common Workflows

### Create Input Action
```python
manage_enhanced_input(
    action="action_create",
    service="action",
    action_name="IA_Interact",
    asset_path="/Game/Input/Actions/IA_Interact",
    value_type="Digital"  # or "Axis1D", "Axis2D"
)
```

### Create Mapping Context
```python
manage_enhanced_input(
    action="mapping_create_context",
    service="mapping",
    context_name="IMC_Combat",
    asset_path="/Game/Input/IMC_Combat"
)
```

### Add Key Binding
```python
manage_enhanced_input(
    action="mapping_add_key_mapping",
    service="mapping",
    context_path="/Game/Input/IMC_Combat",
    action_path="/Game/Input/Actions/IA_Shoot",
    key="LeftMouseButton"
)
```

### Change Key Binding
```python
# 1. Get mappings to find index
manage_enhanced_input(action="mapping_get_mappings", context_path="/Game/Input/IMC_Combat")

# 2. Remove old mapping
manage_enhanced_input(action="mapping_remove_mapping", context_path="/Game/Input/IMC_Combat", mapping_index=0)

# 3. Add new key
manage_enhanced_input(action="mapping_add_key_mapping", context_path="/Game/Input/IMC_Combat",
                      action_path="/Game/Input/Actions/IA_Shoot", key="Z")
```

### Add Modifier to Mapping
```python
manage_enhanced_input(
    action="mapping_add_modifier",
    service="mapping",
    context_path="/Game/Input/IMC_MouseLook",
    mapping_index=0,
    modifier_type="Negate"
)
```

### Add Trigger to Mapping
```python
manage_enhanced_input(
    action="mapping_add_trigger",
    service="mapping",
    context_path="/Game/Input/IMC_Combat",
    mapping_index=0,
    trigger_type="Hold",
    trigger_config={"HoldTimeThreshold": 0.5}
)
```

### Discover Available Keys
```python
manage_enhanced_input(action="mapping_get_available_keys", service="mapping")
# Returns: All keyboard, mouse, and gamepad keys
```

## Value Types

| Type | Description | Example Actions |
|------|-------------|-----------------|
| `Digital` | Boolean on/off | Jump, Shoot, Interact |
| `Axis1D` | Single float value | Throttle, Zoom |
| `Axis2D` | 2D vector | Movement, Look |

## Common Key Names

### Keyboard
`A`-`Z`, `Zero`-`Nine`, `F1`-`F12`, `SpaceBar`, `LeftShift`, `LeftControl`, `Tab`, `Enter`, `Escape`

### Mouse
`LeftMouseButton`, `RightMouseButton`, `MiddleMouseButton`, `Mouse2D`, `MouseScrollUp`, `MouseScrollDown`

### Gamepad
`Gamepad_FaceButton_Bottom` (A), `Gamepad_FaceButton_Right` (B), `Gamepad_FaceButton_Left` (X), `Gamepad_FaceButton_Top` (Y)
`Gamepad_LeftTrigger`, `Gamepad_RightTrigger`, `Gamepad_LeftShoulder`, `Gamepad_RightShoulder`
`Gamepad_Left2D`, `Gamepad_Right2D`, `Gamepad_DPad_Up/Down/Left/Right`

## Modifier Types

| Modifier | Purpose |
|----------|---------|
| `Negate` | Invert axis values |
| `Swizzle Input Axis Values` | Swap/remap axes |
| `Dead Zone` | Add dead zone to analog input |
| `Scalar` | Scale input values |
| `Scale By Delta Time` | Frame-rate independent scaling |
| `Smooth` | Smooth input over time |
| `Response Curve - Exponential` | Non-linear response |

## Trigger Types

| Trigger | Purpose |
|---------|---------|
| `Down` | While key is held |
| `Pressed` | On key down |
| `Released` | On key up |
| `Hold` | After holding for duration |
| `Hold And Release` | After hold then release |
| `Tap` | Quick press and release |
| `Repeated Tap` | Multiple taps |
| `Pulse` | Repeated firing while held |
| `Chorded Action` | Requires another action held |

## Error Recovery

### Asset Already Exists
The tool handles existing assets gracefully - it returns the existing asset rather than failing.

### Invalid Key Name
Use `mapping_get_available_keys` to see valid key names.

### Missing Parameters
Check required parameters in the action reference table.

## Best Practices

1. **List before modifying**: Use `mapping_get_mappings` to see current state
2. **Use asset paths**: Full paths like `/Game/Input/Actions/IA_Move`
3. **Save after changes**: Use `manage_asset(action="save_all")` to persist changes
4. **Validate contexts**: Use `mapping_validate_context` before deployment

---
name: enhanced-input
display_name: Enhanced Input System
description: Create and configure Input Actions, Mapping Contexts, triggers, and modifiers
services:
  - InputService
keywords:
  - input
  - action
  - mapping
  - context
  - key
  - trigger
  - modifier
  - gamepad
  - keyboard
  - mouse
auto_load_keywords:
  - IA_
  - IMC_
  - input action
  - mapping context
  - key binding
  - input mapping
---

# Enhanced Input System

This skill provides comprehensive documentation for working with Unreal Engine's Enhanced Input system using the InputService.

## What's Included

- **InputService API**: Create Input Actions and Mapping Contexts, manage key bindings
- **Return Types Documentation**: Complex struct return types with exact property names
- **Workflows**: Common patterns for input configuration

## When to Use This Skill

Load this skill when working with:
- Input Actions (IA_* prefix)
- Input Mapping Contexts (IMC_* prefix)
- Key mappings and bindings
- Input triggers (Pressed, Released, Hold, Tap, etc.)
- Input modifiers (Negate, DeadZone, Scalar, etc.)
- Gamepad, keyboard, and mouse input

## Core Services

### InputService
Enhanced Input management:
- Create Input Actions with value types (Boolean, Axis1D, Axis2D, Axis3D)
- Create Mapping Contexts with priority
- Add key mappings to contexts
- Configure triggers and modifiers
- Discover available keys, triggers, and modifiers

## Quick Examples

### Create Input Action
```python
import unreal

result = unreal.InputService.create_action("Jump", "/Game/Input", "Boolean")
if result.success:
    print(f"Created: {result.asset_path}")
```

### Create Mapping Context with Bindings
```python
import unreal

# Create context
result = unreal.InputService.create_mapping_context("Default", "/Game/Input", 0)
context_path = result.asset_path

# Add key mapping
unreal.InputService.add_key_mapping(context_path, "/Game/Input/IA_Jump", "SpaceBar")

# Get mappings
mappings = unreal.InputService.get_mappings(context_path)
for m in mappings:
    print(f"{m.action_name} → {m.key_name}")
```

## Related Skills

- **asset-management**: For finding input assets
- **blueprints**: For using input in Blueprint functions

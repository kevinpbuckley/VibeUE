# Enhanced Input System - Comprehensive Test Prompts

This document contains test prompts for all actions exposed by the `manage_enhanced_input` tool.

---

## Table of Contents

1. [Discovery & Reflection](#discovery--reflection)
2. [Input Actions](#input-actions)
3. [Input Mapping Contexts](#input-mapping-contexts)
4. [Key Mappings](#key-mappings)
5. [Modifiers](#modifiers)
6. [Triggers](#triggers)
7. [Analysis & Validation](#analysis--validation)
8. [Complex Workflows](#complex-workflows)

---

## Discovery & Reflection

### Test: Discover All Types
```
Discover all available Enhanced Input types including modifiers and triggers.
Show me the complete list of what's available in the system.
```
**Expected Action:** `reflection_discover_types`

### Test: Get Modifier Metadata
```
Get detailed metadata for the "Swizzle Input Axis Values" modifier.
What properties can I configure and what are the defaults?
```
**Expected Action:** `reflection_get_metadata` with `input_type="modifier"`

### Test: Get Trigger Metadata
```
Show me the complete metadata for the "Hold" trigger type.
What configuration options does it have?
```
**Expected Action:** `reflection_get_metadata` with `input_type="trigger"`

### Test: List All Available Keys
```
What input keys are available for binding? Show me all keyboard, mouse, and gamepad options.
```
**Expected Action:** `mapping_get_available_keys`

### Test: List Available Modifier Types
```
What modifier types can I add to input mappings?
```
**Expected Action:** `mapping_get_available_modifier_types`

### Test: List Available Trigger Types
```
What trigger types are available for input actions?
```
**Expected Action:** `mapping_get_available_trigger_types`

---

## Input Actions

### Test: List All Input Actions
```
Show me all Input Actions in the project.
```
**Expected Action:** `action_list`

### Test: Create Digital Input Action
```
Create a new Input Action called "IA_Interact" with Digital (boolean) value type
at /Game/Input/Actions/IA_Interact
```
**Expected Action:** `action_create` with `value_type="Digital"`

### Test: Create Axis2D Input Action
```
Create an Input Action for movement called "IA_Movement" with Axis2D value type
at /Game/Input/Actions/IA_Movement
```
**Expected Action:** `action_create` with `value_type="Axis2D"`

### Test: Create Axis1D Input Action
```
Create an Input Action for throttle called "IA_Throttle" with Axis1D value type
at /Game/Input/Actions/IA_Throttle
```
**Expected Action:** `action_create` with `value_type="Axis1D"`

### Test: Get Input Action Properties
```
Get all properties of the IA_Move input action. Show me the current configuration.
```
**Expected Action:** `action_get_properties`

### Test: Configure Input Action - Simple Property
```
Configure IA_Interact to consume input (set bConsumeInput to true).
```
**Expected Action:** `action_configure` with `property_name="bConsumeInput"`, `property_value=true`

### Test: Configure Input Action - Complex Property (Triggers Array)
```
Add a "Pressed" trigger to the IA_Interact action's default triggers array.
```
**Expected Action:** `action_configure` with trigger configuration

### Test: Configure Input Action - Description
```
Set the description of IA_Interact to "Press E to interact with objects"
```
**Expected Action:** `action_configure` with `property_name="ActionDescription"`

---

## Input Mapping Contexts

### Test: List All Mapping Contexts
```
Show me all Input Mapping Contexts in the project.
```
**Expected Action:** `mapping_list_contexts`

### Test: Create Mapping Context
```
Create a new Input Mapping Context called "IMC_Combat" at /Game/Input/IMC_Combat
```
**Expected Action:** `mapping_create_context`

### Test: Create Mapping Context with Priority
```
Create a mapping context "IMC_Vehicle" at /Game/Input/IMC_Vehicle with priority 10
```
**Expected Action:** `mapping_create_context` with `priority=10`

### Test: Get Mapping Context Properties
```
Get all properties of the IMC_Default mapping context.
```
**Expected Action:** `mapping_get_properties`

### Test: Get Single Property
```
Get the ContextDescription property from IMC_Weapons.
```
**Expected Action:** `mapping_get_property`

### Test: Update Mapping Context Property
```
Set the description of IMC_Combat to "Combat-specific input bindings"
```
**Expected Action:** `mapping_update_context` with property settings

### Test: Validate Context Configuration
```
Validate that IMC_Weapons is correctly configured with no errors.
```
**Expected Action:** `mapping_validate_context`

---

## Key Mappings

### Test: Get All Mappings in Context
```
Show me all key mappings in IMC_Weapons. What actions are bound to what keys?
```
**Expected Action:** `mapping_get_mappings`

### Test: Add Keyboard Binding
```
Add a key binding: E key -> IA_Interact in IMC_Default
```
**Expected Action:** `mapping_add_key_mapping` with `key="E"`

### Test: Add Mouse Binding
```
Bind left mouse button to IA_Shoot in IMC_Combat
```
**Expected Action:** `mapping_add_key_mapping` with `key="LeftMouseButton"`

### Test: Add Gamepad Binding
```
Bind gamepad right trigger to IA_Shoot in IMC_Combat
```
**Expected Action:** `mapping_add_key_mapping` with `key="Gamepad_RightTrigger"`

### Test: Add 2D Axis Binding (Mouse)
```
Bind Mouse2D to IA_Look in IMC_Default
```
**Expected Action:** `mapping_add_key_mapping` with `key="Mouse2D"`

### Test: Add 2D Axis Binding (Gamepad)
```
Bind Gamepad_Left2D to IA_Move in IMC_Default
```
**Expected Action:** `mapping_add_key_mapping` with `key="Gamepad_Left2D"`

### Test: Remove Key Mapping
```
Remove the first mapping (index 0) from IMC_Combat
```
**Expected Action:** `mapping_remove_mapping` with `mapping_index=0`

### Test: Replace Key Mapping
```
Change the shoot binding in IMC_Weapons from left mouse button to Z key
```
**Expected Workflow:**
1. `mapping_get_mappings` - find the index
2. `mapping_remove_mapping` - remove old binding
3. `mapping_add_key_mapping` - add new binding

---

## Modifiers

### Test: Get Modifiers on Mapping
```
What modifiers are applied to the first mapping in IMC_MouseLook?
```
**Expected Action:** `mapping_get_modifiers` with `mapping_index=0`

### Test: Add Negate Modifier
```
Add a Negate modifier to the Mouse2D -> IA_Look mapping in IMC_MouseLook
to invert the Y axis
```
**Expected Action:** `mapping_add_modifier` with `modifier_type="Negate"`

### Test: Add Swizzle Modifier
```
Add a Swizzle modifier to swap X and Y axes on the movement input
```
**Expected Action:** `mapping_add_modifier` with `modifier_type="Swizzle Input Axis Values"`

### Test: Add Dead Zone Modifier
```
Add a Dead Zone modifier with 0.2 threshold to the gamepad stick mapping
```
**Expected Action:** `mapping_add_modifier` with `modifier_type="Dead Zone"` and config

### Test: Add Scalar Modifier
```
Add a Scalar modifier to multiply the mouse sensitivity by 2.0
```
**Expected Action:** `mapping_add_modifier` with `modifier_type="Scalar"` and config

### Test: Add Scale By Delta Time Modifier
```
Add Scale By Delta Time modifier to make input frame-rate independent
```
**Expected Action:** `mapping_add_modifier` with `modifier_type="Scale By Delta Time"`

### Test: Remove Modifier
```
Remove the first modifier from the mouse look mapping
```
**Expected Action:** `mapping_remove_modifier` with `modifier_index=0`

### Test: Configure Modifier with Complex Properties
```
Add a Response Curve - Exponential modifier with exponent 2.0 to create
acceleration on stick input
```
**Expected Action:** `mapping_add_modifier` with `modifier_config={"Exponent": 2.0}`

---

## Triggers

### Test: Get Triggers on Mapping
```
What triggers are configured on the IA_Shoot mapping?
```
**Expected Action:** `mapping_get_triggers` with `mapping_index`

### Test: Add Pressed Trigger
```
Add a Pressed trigger to IA_Interact so it fires on key down
```
**Expected Action:** `mapping_add_trigger` with `trigger_type="Pressed"`

### Test: Add Released Trigger
```
Add a Released trigger to IA_Crouch so it fires on key up
```
**Expected Action:** `mapping_add_trigger` with `trigger_type="Released"`

### Test: Add Hold Trigger
```
Add a Hold trigger with 0.5 second threshold to IA_Sprint
```
**Expected Action:** `mapping_add_trigger` with `trigger_type="Hold"` and config

### Test: Add Tap Trigger
```
Add a Tap trigger with 0.2 second max tap time for quick interactions
```
**Expected Action:** `mapping_add_trigger` with `trigger_type="Tap"` and config

### Test: Add Pulse Trigger
```
Add a Pulse trigger that fires every 0.1 seconds while held for auto-fire
```
**Expected Action:** `mapping_add_trigger` with `trigger_type="Pulse"` and config

### Test: Add Chorded Action Trigger
```
Add a Chorded Action trigger that requires IA_Modifier to be held
```
**Expected Action:** `mapping_add_trigger` with `trigger_type="Chorded Action"` and config

### Test: Remove Trigger
```
Remove the first trigger from the IA_Shoot mapping
```
**Expected Action:** `mapping_remove_trigger` with `trigger_index=0`

---

## Analysis & Validation

### Test: Analyze Context Usage
```
Analyze how IMC_Weapons is being used. Show me all actions and their bindings.
```
**Expected Action:** `mapping_analyze_usage`

### Test: Detect Key Conflicts
```
Check if there are any conflicting key bindings in IMC_Default
```
**Expected Action:** `mapping_detect_conflicts`

### Test: Validate Before Deploy
```
Validate IMC_Combat is correctly configured before I deploy to production
```
**Expected Action:** `mapping_validate_context`

---

## Complex Workflows

### Test: Complete FPS Input Setup
```
Create a complete FPS input setup:
1. Create actions: IA_Aim, IA_Reload, IA_Crouch
2. Create mapping context: IMC_FPS
3. Bind: Right mouse -> IA_Aim, R -> IA_Reload, C/LeftControl -> IA_Crouch
4. Add Hold trigger to IA_Aim
5. Add Pressed trigger to IA_Reload
```
**Expected Workflow:** Multiple create, bind, and configure actions

### Test: Gamepad Configuration with Modifiers
```
Set up gamepad controls for movement:
1. Bind Gamepad_Left2D to IA_Move
2. Add Dead Zone modifier (0.15)
3. Add Response Curve - Exponential modifier
4. Verify the configuration
```
**Expected Workflow:** Add mapping then multiple modifiers

### Test: Mouse Look with Sensitivity
```
Configure mouse look with proper sensitivity:
1. Bind Mouse2D to IA_Look in IMC_MouseLook
2. Add Scalar modifier for X axis (2.5)
3. Add Negate modifier for Y axis (invert)
4. Add Smooth modifier for smoothing
```
**Expected Workflow:** Add mapping then chain modifiers

### Test: Combo Input Setup
```
Set up a combo system:
1. Create IA_ComboAttack action
2. Add Repeated Tap trigger
3. Configure for 3 taps within 0.5 seconds
```
**Expected Workflow:** Create action with complex trigger config

### Test: Modifier Key Combinations
```
Set up Shift+E for alternate interaction:
1. Create IA_AlternateInteract
2. Bind E key
3. Add Chorded Action trigger requiring Left Shift
```
**Expected Workflow:** Mapping with chorded trigger

### Test: Racing Game Throttle
```
Set up throttle for a racing game:
1. Create IA_Throttle with Axis1D
2. Bind Gamepad_RightTriggerAxis
3. Add Response Curve for progressive feel
4. Bind W key with digital-to-analog conversion
```
**Expected Workflow:** Mixed analog/digital input handling

---

## Property Configuration Examples

### Simple Property Types

| Property | Type | Example Value |
|----------|------|---------------|
| `bConsumeInput` | Boolean | `true` |
| `ActionDescription` | String | `"Move character"` |
| `Priority` | Integer | `10` |

### Modifier Configuration

```json
// Dead Zone
{"LowerThreshold": 0.15, "UpperThreshold": 0.9, "Type": "Axial"}

// Scalar
{"Scalar": {"X": 2.0, "Y": 2.0, "Z": 1.0}}

// Swizzle
{"Order": "YXZ"}

// Negate
{"bX": false, "bY": true, "bZ": false}
```

### Trigger Configuration

```json
// Hold
{"HoldTimeThreshold": 0.5, "bIsOneShot": false}

// Tap
{"TapReleaseTimeThreshold": 0.2}

// Pulse
{"bTriggerOnStart": true, "Interval": 0.1}

// Chorded Action
{"ChordAction": "/Game/Input/Actions/IA_Modifier"}
```

---

## Error Handling Tests

### Test: Create Duplicate Action
```
Try to create IA_Move when it already exists
```
**Expected:** Error with existing asset handling or graceful return

### Test: Invalid Key Name
```
Try to bind "InvalidKey" to an action
```
**Expected:** Error with available keys suggestion

### Test: Invalid Modifier Type
```
Try to add "FakeModifier" to a mapping
```
**Expected:** Error with available modifier types

### Test: Missing Required Parameter
```
Try to add a key mapping without specifying the context_path
```
**Expected:** Error indicating missing required parameter

### Test: Non-Existent Asset
```
Try to get properties of /Game/Input/IA_DoesNotExist
```
**Expected:** Asset not found error

---

## Complete Action Reference

| Service | Action | Required Parameters |
|---------|--------|---------------------|
| reflection | `reflection_discover_types` | - |
| reflection | `reflection_get_metadata` | `input_type`, `property_name` |
| action | `action_create` | `action_name`, `asset_path`, `value_type` |
| action | `action_list` | - |
| action | `action_get_properties` | `action_name` |
| action | `action_configure` | `action_name`, `property_name`, `property_value` |
| mapping | `mapping_create_context` | `context_name`, `asset_path` |
| mapping | `mapping_list_contexts` | - |
| mapping | `mapping_get_properties` | `context_path` |
| mapping | `mapping_get_property` | `context_path`, `property_name` |
| mapping | `mapping_update_context` | `context_path`, `property_name`, `property_value` |
| mapping | `mapping_validate_context` | `context_path` |
| mapping | `mapping_get_mappings` | `context_path` |
| mapping | `mapping_add_key_mapping` | `context_path`, `action_path`, `key` |
| mapping | `mapping_remove_mapping` | `context_path`, `mapping_index` |
| mapping | `mapping_get_available_keys` | - |
| mapping | `mapping_add_modifier` | `context_path`, `mapping_index`, `modifier_type` |
| mapping | `mapping_remove_modifier` | `context_path`, `mapping_index`, `modifier_index` |
| mapping | `mapping_get_modifiers` | `context_path`, `mapping_index` |
| mapping | `mapping_get_available_modifier_types` | - |
| mapping | `mapping_add_trigger` | `context_path`, `mapping_index`, `trigger_type` |
| mapping | `mapping_remove_trigger` | `context_path`, `mapping_index`, `trigger_index` |
| mapping | `mapping_get_triggers` | `context_path`, `mapping_index` |
| mapping | `mapping_get_available_trigger_types` | - |
| mapping | `mapping_analyze_usage` | `context_path` |
| mapping | `mapping_detect_conflicts` | `context_path` |

# Enhanced Input Test Prompts

These prompts test all manage_enhanced_input actions. Run as a single conversation - assets are created, tested, and cleaned up.

---

## Phase 1: Setup - Create Test Assets

### Setup: Create test contexts and actions
Create these test assets for our Enhanced Input testing:
1. Input Mapping Context: IMC_TestContext at /Game/Input/TestAssets
2. Input Mapping Context: IMC_TestCombat at /Game/Input/TestAssets  
3. Input Mapping Context: IMC_TestVehicle at /Game/Input/TestAssets with priority 10
4. Digital Input Action: IA_TestAction at /Game/Input/TestAssets
5. Digital Input Action: IA_TestInteract at /Game/Input/TestAssets
6. Digital Input Action: IA_TestShoot at /Game/Input/TestAssets
7. Axis2D Input Action: IA_TestMovement at /Game/Input/TestAssets
8. Axis2D Input Action: IA_TestLook at /Game/Input/TestAssets
9. Axis1D Input Action: IA_TestThrottle at /Game/Input/TestAssets

---

## Phase 2: Discovery & Reflection Tests

### Test: reflection_discover_types
Show me all the Enhanced Input types available in Unreal Engine, including modifiers and triggers.

### Test: reflection_get_metadata (modifier)
Get the metadata for the "Negate" modifier type.

### Test: reflection_get_metadata (trigger)
What are the configurable properties for the "Hold" trigger type?

### Test: mapping_get_available_keys
List all the available input keys I can use for key bindings.

### Test: mapping_get_available_modifier_types
What modifier types are available for Enhanced Input?

### Test: mapping_get_available_trigger_types
List all the trigger types I can add to input mappings.

---

## Phase 3: Action Tests

### Test: action_list
Show me all the Input Actions that exist in /Game/Input/TestAssets.

### Test: action_get_properties
Get all the properties of the IA_TestMovement input action at /Game/Input/TestAssets.

### Test: action_configure (boolean)
Configure the IA_TestInteract action at /Game/Input/TestAssets to consume input by setting bConsumeInput to true.

### Test: action_configure (description)
Set the description of IA_TestInteract at /Game/Input/TestAssets to "Test interaction action".

---

## Phase 4: Mapping Context Tests

### Test: mapping_list_contexts
List all the Input Mapping Contexts in the project.

### Test: mapping_get_mappings (empty)
Show me all the key mappings in IMC_TestContext at /Game/Input/TestAssets (should be empty initially).

---

## Phase 5: Key Mapping Tests

### Test: mapping_add_key_mapping (keyboard)
Bind the E key to IA_TestInteract in IMC_TestContext (both at /Game/Input/TestAssets).

### Test: mapping_add_key_mapping (shift key)
Bind LeftShift to IA_TestAction in IMC_TestContext (both at /Game/Input/TestAssets).

### Test: mapping_add_key_mapping (mouse)
Add a binding for LeftMouseButton to IA_TestShoot in IMC_TestCombat (both at /Game/Input/TestAssets).

### Test: mapping_add_key_mapping (gamepad trigger)
Bind Gamepad_RightTrigger to IA_TestShoot in IMC_TestCombat (both at /Game/Input/TestAssets).

### Test: mapping_add_key_mapping (2D axis)
Map Mouse2D to IA_TestLook in IMC_TestContext (both at /Game/Input/TestAssets).

### Test: mapping_add_key_mapping (gamepad stick)
Bind Gamepad_Left2D to IA_TestMovement in IMC_TestContext (both at /Game/Input/TestAssets).

### Test: mapping_add_key_mapping (vehicle throttle)
Bind Gamepad_RightTrigger to IA_TestThrottle in IMC_TestVehicle (both at /Game/Input/TestAssets).

### Test: mapping_get_mappings (after adding)
Show me all the key mappings in IMC_TestContext at /Game/Input/TestAssets to verify our bindings.

### Test: mapping_remove_mapping
Remove the key mapping at index 0 from IMC_TestCombat at /Game/Input/TestAssets.

---

## Phase 6: Modifier Tests

### Test: mapping_add_modifier (Negate)
Add a Negate modifier to mapping index 0 in IMC_TestContext at /Game/Input/TestAssets.

### Test: mapping_add_modifier (DeadZone)
Add a DeadZone modifier to mapping index 2 in IMC_TestContext at /Game/Input/TestAssets (the Mouse2D mapping).

### Test: mapping_add_modifier (Scalar)
Add a Scalar modifier to mapping index 0 in IMC_TestVehicle at /Game/Input/TestAssets.

### Test: mapping_get_modifiers
Get all the modifiers on mapping index 0 in IMC_TestContext at /Game/Input/TestAssets.

### Test: mapping_remove_modifier
Remove the modifier at index 0 from mapping 0 in IMC_TestContext at /Game/Input/TestAssets.

---

## Phase 7: Trigger Tests

### Test: mapping_add_trigger (Pressed)
Add a Pressed trigger to mapping index 0 in IMC_TestContext at /Game/Input/TestAssets.

### Test: mapping_add_trigger (Hold)
Add a Hold trigger to mapping index 0 in IMC_TestCombat at /Game/Input/TestAssets.

### Test: mapping_add_trigger (Tap)
Add a Tap trigger to mapping index 0 in IMC_TestCombat at /Game/Input/TestAssets.

### Test: mapping_get_triggers
Show me all the triggers on mapping index 0 in IMC_TestCombat at /Game/Input/TestAssets.

### Test: mapping_remove_trigger
Remove the trigger at index 0 from mapping 0 in IMC_TestContext at /Game/Input/TestAssets.

---

## Phase 8: Complex Multi-Step Tests

### Test: Full input setup
Create a new Digital Input Action called IA_TestReload at /Game/Input/TestAssets, then add it to IMC_TestCombat at /Game/Input/TestAssets bound to the R key, and add a Pressed trigger to that mapping.

### Test: Movement with modifiers
Create an Axis2D input action called IA_TestStrafe at /Game/Input/TestAssets, bind it to Gamepad_Right2D in IMC_TestContext at /Game/Input/TestAssets, add a DeadZone modifier, then add a SwizzleAxis modifier.

### Test: Context with multiple bindings
Create a new mapping context called IMC_TestMenu at /Game/Input/TestAssets, create Digital actions IA_TestPause, IA_TestConfirm, and IA_TestBack at /Game/Input/TestAssets, then bind Escape to IA_TestPause, Enter to IA_TestConfirm, and BackSpace to IA_TestBack in IMC_TestMenu.

---

## Phase 9: Cleanup - Delete All Test Assets

### Cleanup: Remove all test assets
Delete all the test assets we created. Use force delete and don't show confirmation dialogs. Delete these: if you can't delete an asset move on to the next one.

Input Actions:
- /Game/Input/TestAssets/IA_TestAction
- /Game/Input/TestAssets/IA_TestInteract
- /Game/Input/TestAssets/IA_TestShoot
- /Game/Input/TestAssets/IA_TestMovement
- /Game/Input/TestAssets/IA_TestLook
- /Game/Input/TestAssets/IA_TestThrottle
- /Game/Input/TestAssets/IA_TestReload
- /Game/Input/TestAssets/IA_TestStrafe
- /Game/Input/TestAssets/IA_TestPause
- /Game/Input/TestAssets/IA_TestConfirm
- /Game/Input/TestAssets/IA_TestBack

Mapping Contexts:
- /Game/Input/TestAssets/IMC_TestContext
- /Game/Input/TestAssets/IMC_TestCombat
- /Game/Input/TestAssets/IMC_TestVehicle
- /Game/Input/TestAssets/IMC_TestMenu

Search for any remaining assets in /Game/Input/TestAssets and delete them too.

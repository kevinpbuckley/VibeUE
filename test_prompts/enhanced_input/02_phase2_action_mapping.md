# Phase 2: Input Action & Mapping Management Test Prompts

These prompts test creating, managing, and configuring input actions and mapping contexts.

## Input Action Lifecycle Tests

### Test: Create Simple Input Action
I need to create a basic input action for player movement. Create an action called "IA_Move" that handles 2D input for forward/back and left/right movement.

### Test: Create Axis Input Action
Create an input action called "IA_Look" that will be used for camera look around. This should handle continuous axis input with values like mouse movement or controller stick.

### Test: Create Digital Input Action
I want a simple yes/no input action for "IA_Jump". Create this as a digital (button press) input action.

### Test: List All Input Actions
Show me all the input actions that currently exist in the project. I want to see what's already been set up.

### Test: Get Input Action Properties
Tell me all the properties and settings for the "IA_Move" action. What's currently configured for it?

### Test: Modify Input Action Display Name
Update the "IA_Look" action to have a more descriptive display name like "Camera Look".

### Test: Configure Input Action Behavior
Set up the "IA_Move" action to consume input (so it doesn't pass through to the game world when the UI has focus).

### Test: Delete Old Input Action
I have an old input action called "IA_Sprint_Legacy" that's no longer used. Please remove it from the project.

## Input Mapping Context Tests

### Test: Create Default Mapping Context
Create the default keyboard and mouse input mapping context called "IMC_Default" with priority 0. This will map basic actions to WASD and mouse.

### Test: Create Combat Context
Create a combat-specific mapping context called "IMC_Combat" with priority 5. Combat controls will override default controls when active.

### Test: Create Mobile Context
Set up a mobile input mapping context called "IMC_Mobile" with priority 1 for touchscreen controls.

### Test: List All Mapping Contexts
Show me all the input mapping contexts that are available in the project. What priorities are they set to?

### Test: Delete Mapping Context
I need to remove an old mapping context called "IMC_Deprecated_Controls". Please delete it.

## Key Mapping Tests

### Test: Map Keyboard Key
Add a mapping to the "IMC_Default" context that binds the W key to the "IA_Move" action for forward movement.

### Test: Map Multiple Keys to Same Action
In the "IMC_Default" context, map both W and UpArrow keys to "IA_Move" action for forward movement options.

### Test: Map Gamepad Button
Create a mapping in "IMC_Default" where the gamepad's right trigger (RT) activates the "IA_Attack" action.

### Test: Map Mouse Button
In the "IMC_Default" context, bind the left mouse button to "IA_Attack" action.

### Test: View All Mappings in Context
Show me every key-to-action mapping in the "IMC_Default" context. I want to see all the bindings.

### Test: Map Gamepad Stick
In a game controller context, map the right analog stick to "IA_Look" for camera control.

### Test: Map with Modifiers
Create a mapping where Shift+W triggers "IA_Sprint" (shift key used as a modifier with W key).

## Complex Workflow Tests

### Test: Complete Input Setup Workflow
Walk me through setting up a complete input system: Create "IA_Move", "IA_Look", "IA_Jump", and "IA_Attack" actions, then create "IMC_Default" and map all the keyboard bindings.

### Test: Multi-Context System
Set up two mapping contexts - "IMC_Exploration" for exploration mode and "IMC_Combat" for combat mode, both mapping to the same actions but with different keys.

### Test: Create RPG Input System
Set up a typical RPG input system with actions for: Move, Look, Interact, Cast Spell 1-4, Inventory, Map. Then create keyboard mappings.

### Test: Create FPS Input System
Build an FPS-style input system with: Move (WASD), Look (Mouse), Jump (Space), Attack (Mouse Click), Sprint (Shift), Crouch (Ctrl), Reload (R).

### Test: Gamepad Mapping Scheme
Create a complete gamepad mapping for console players: Sticks for movement and look, buttons for jump/attack, triggers for secondary actions.

### Test: Context Switching Setup
Create a system where you have a default context for exploration, but when entering combat, a higher priority combat context takes over specific input bindings.

### Test: Access Key Remapping
Set up the base mappings, then show how a user could view and understand what keys are bound to what actions for a key remapping UI.

### Test: Input Action Organization
Create a well-organized input system for a game that supports: Player movement, Camera control, UI navigation, and Combat actions. Show me the structure.

## Edge Case Tests

### Test: Create Action with Same Name
Try to create an input action with a name that already exists. What happens?

### Test: Priority Conflicts
Create two mapping contexts with the same priority. How does the system handle conflicts?

### Test: Invalid Value Types
Try to create an input action with an invalid value type. What validation error do we get?

### Test: Delete Active Context
Try to delete a mapping context that's currently active. What's the behavior?

### Test: Map to Non-Existent Action
Attempt to create a key mapping to an input action that doesn't exist. What error occurs?

## Property Management Tests

### Test: Update Context Description
Set the description for "IMC_Default" to explain what it's for: "Default input mappings for basic movement, looking, and jumping".

### Test: Read Context Description  
What is the current description set on the "IMC_Combat" context?

### Test: Set Registration Tracking Mode
Configure "IMC_Combat" to count registrations so we can track when it's active. Set RegistrationTrackingMode to "CountRegistrations".

### Test: Set Input Mode Filter
Configure "IMC_Default" to use a custom input mode query filter instead of the project default.

### Test: Get All Context Properties
Show me all available properties and their current values for "IMC_Default" so I know what can be configured.

## Advanced Management Tests

### Test: Validate Context Configuration
Check if the "IMC_Default" mapping context has valid configuration - are all actions valid and all keys mapped correctly?

### Test: Duplicate Mapping Context
Create a copy of "IMC_Default" called "IMC_Default_Backup" in "/Game/Input/Backups/" to preserve the current configuration.

### Test: Get Available Input Keys
I'm building a key remapping UI. Show me all the valid key names that players can bind to actions (filter for keyboard keys).

### Test: Analyze Context Usage
Analyze the "IMC_Default" context and tell me statistics: how many mappings, how many unique actions, how many unique keys?

### Test: Detect Key Conflicts
Check if there are any key conflicts between "IMC_Default" and "IMC_Combat" contexts - are any keys mapped to different actions in both contexts?

### Test: Find All Contexts
Search for all Input Mapping Contexts in the entire project. I want to see everything that exists.

### Test: Search Contexts by Name
Find all mapping contexts that have "Combat" in their name.

### Test: Get Gamepad Keys
I need to build a gamepad configuration UI. Get all available input keys but filter to only show gamepad buttons and sticks.

### Test: Remove Specific Mapping
In "IMC_Default", I want to remove the third key mapping (index 2). Delete that specific mapping without affecting others.

### Test: Update Multiple Properties
Update both the description and registration tracking mode for "IMC_Combat" in one operation.

## Integration & Workflow Tests

### Test: Complete Property Configuration Workflow
1. Create "IMC_Advanced"
2. Set its description
3. Enable registration tracking
4. Add several key mappings
5. Validate the configuration
6. Get all properties to verify settings

### Test: Conflict Resolution Workflow
1. Create two contexts with overlapping key bindings
2. Detect the conflicts
3. Analyze usage of both contexts
4. Show how to resolve by adjusting priorities

### Test: Backup and Restore Workflow
1. Duplicate an existing context as backup
2. Make changes to the original
3. Validate both configurations
4. Compare properties between original and backup


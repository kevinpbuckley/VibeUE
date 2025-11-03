# manage_blueprint Test Prompts

## Prerequisites
- Unreal Engine 5.6+ running
- VibeUE plugin loaded and enabled
- MCP connection active (verify with `check_unreal_connection`)
- A test project with /Game/Blueprints folder

## Overview
This document provides comprehensive test prompts for the `manage_blueprint` tool, covering all 8 actions in a logical Blueprint lifecycle workflow. The tests follow the natural sequence of creating, configuring, and managing a Blueprint.

---

## Test 1: Complete Blueprint Lifecycle

**Purpose**: Test all 8 actions in natural workflow order to verify complete Blueprint lifecycle management.

### Step 1: Create Blueprint
**Action**: Create a new Actor Blueprint

**Command**:
```python
manage_blueprint(
    action="create",
    name="BP_TestActor",
    parent_class="Actor"
)
```

**Expected Outcome**:
- Success: true
- Blueprint created at path (e.g., `/Game/Blueprints/BP_TestActor`)
- Reminder message about dependency order: Variables → Components → Functions → Event Graph
- Blueprint appears in Content Browser

**Validation**: Check Content Browser for BP_TestActor in /Game/Blueprints/

---

### Step 2: Get Blueprint Info
**Action**: Verify Blueprint creation and inspect initial state

**Command**:
```python
manage_blueprint(
    action="get_info",
    blueprint_name="/Game/Blueprints/BP_TestActor",
    include_class_defaults=True
)
```

**Expected Outcome**:
- Success: true
- parent_class: "Actor"
- Variables list (may be empty for new Blueprint)
- Components list (should include DefaultSceneRoot)
- Functions list (inherited functions present)
- Event graphs list
- Class defaults section with inherited properties

**Validation**: Verify parent_class is "Actor" and DefaultSceneRoot component exists

---

### Step 3: Get Property Value
**Action**: Read a default property value from the Blueprint

**Command**:
```python
manage_blueprint(
    action="get_property",
    blueprint_name="/Game/Blueprints/BP_TestActor",
    property_name="bCanBeDamaged"
)
```

**Expected Outcome**:
- Success: true
- current_value: true (or false, depending on Actor default)
- type: "bool"
- Property metadata including category, tooltip
- Min/max values if applicable

**Validation**: Property value matches Actor class default

---

### Step 4: Set Property Value
**Action**: Modify a class default property

**Command**:
```python
manage_blueprint(
    action="set_property",
    blueprint_name="/Game/Blueprints/BP_TestActor",
    property_name="bCanBeDamaged",
    property_value=False
)
```

**Expected Outcome**:
- Success: true
- Property set confirmation message
- Value persisted to Blueprint

**Validation**: Run get_property again to verify value is now False

---

### Step 5: Compile Blueprint
**Action**: Compile the Blueprint after property changes

**Command**:
```python
manage_blueprint(
    action="compile",
    blueprint_name="/Game/Blueprints/BP_TestActor"
)
```

**Expected Outcome**:
- Success: true
- Compilation successful message
- No compilation errors
- Blueprint status changes to "Up to date"

**Validation**: Check Blueprint editor - should show green checkmark (compiled successfully)

---

### Step 6: Get Info Again (Verify Compiled State)
**Action**: Verify Blueprint is compiled and property changes are applied

**Command**:
```python
manage_blueprint(
    action="get_info",
    blueprint_name="/Game/Blueprints/BP_TestActor",
    include_class_defaults=True
)
```

**Expected Outcome**:
- Success: true
- Compiled status: true (or equivalent indicator)
- Class defaults reflect the changed property (bCanBeDamaged = false)
- All previous structure intact

**Validation**: Confirm compiled state and property change persisted

---

### Step 7: Reparent Blueprint
**Action**: Change the parent class of the Blueprint

**Command**:
```python
manage_blueprint(
    action="reparent",
    blueprint_name="/Game/Blueprints/BP_TestActor",
    new_parent_class="Pawn"
)
```

**Expected Outcome**:
- Success: true
- Previous parent: "Actor"
- New parent: "Pawn"
- Component hierarchy may be automatically fixed
- Compilation may be required

**Validation**: 
- Run get_info to verify parent_class is now "Pawn"
- Check for new Pawn-specific properties (e.g., AutoPossessPlayer)

---

### Step 8: List Custom Events
**Action**: List any custom events in the Blueprint

**Command**:
```python
manage_blueprint(
    action="list_custom_events",
    blueprint_name="/Game/Blueprints/BP_TestActor"
)
```

**Expected Outcome**:
- Success: true
- events: [] (empty array for new Blueprint)
- Or events: ["EventName1", "EventName2"] if events exist

**Validation**: Empty list expected for newly created Blueprint

---

### Step 9: Summarize Event Graph
**Action**: Get a readable outline of the Event Graph

**Command**:
```python
manage_blueprint(
    action="summarize_event_graph",
    blueprint_name="/Game/Blueprints/BP_TestActor",
    max_nodes=200
)
```

**Expected Outcome**:
- Success: true
- summary: Text outline of Event Graph
- Lists nodes like "Event BeginPlay", "Event Tick", etc.
- Shows node connections and types
- Node count indicated

**Validation**: Summary should include standard Actor/Pawn events (BeginPlay, Tick)

---

### Cleanup
**Action**: Delete the test Blueprint

```python
# Note: manage_blueprint does not have a delete action
# Use Unreal Editor to delete /Game/Blueprints/BP_TestActor manually
# Or use asset management tools if available
```

**Manual Steps**:
1. Open Content Browser in Unreal Editor
2. Navigate to /Game/Blueprints/
3. Right-click BP_TestActor
4. Select "Delete"
5. Confirm deletion

---

## Test 2: Property Management Deep Dive

**Purpose**: Test property get/set operations with various data types

### Prerequisites
Create a Character Blueprint for testing:
```python
manage_blueprint(
    action="create",
    name="BP_TestCharacter",
    parent_class="Character"
)
```

### Test 2.1: Boolean Property
```python
# Get current value
manage_blueprint(
    action="get_property",
    blueprint_name="/Game/Blueprints/BP_TestCharacter",
    property_name="bUseControllerRotationYaw"
)

# Set to true
manage_blueprint(
    action="set_property",
    blueprint_name="/Game/Blueprints/BP_TestCharacter",
    property_name="bUseControllerRotationYaw",
    property_value=True
)

# Verify change
manage_blueprint(
    action="get_property",
    blueprint_name="/Game/Blueprints/BP_TestCharacter",
    property_name="bUseControllerRotationYaw"
)
```

**Expected**: Value changes from default to True

---

### Test 2.2: Float Property
```python
# Get default
manage_blueprint(
    action="get_property",
    blueprint_name="/Game/Blueprints/BP_TestCharacter",
    property_name="CapsuleHalfHeight"
)

# Set new value
manage_blueprint(
    action="set_property",
    blueprint_name="/Game/Blueprints/BP_TestCharacter",
    property_name="CapsuleHalfHeight",
    property_value=100.0
)

# Verify
manage_blueprint(
    action="get_property",
    blueprint_name="/Game/Blueprints/BP_TestCharacter",
    property_name="CapsuleHalfHeight"
)
```

**Expected**: Value changes to 100.0

---

### Test 2.3: String Property
```python
# Get property that accepts strings (may need custom variable)
# Example with a display name or tag
manage_blueprint(
    action="get_property",
    blueprint_name="/Game/Blueprints/BP_TestCharacter",
    property_name="AIControllerClass"
)

# Note: Some properties may be read-only or require specific format
```

---

### Cleanup Test 2
Delete BP_TestCharacter manually from Content Browser

---

## Test 3: Blueprint Type Variations

**Purpose**: Test creation of different Blueprint types

### Test 3.1: Widget Blueprint (UMG)
```python
manage_blueprint(
    action="create",
    name="WBP_TestWidget",
    parent_class="UserWidget"
)

# Verify creation
manage_blueprint(
    action="get_info",
    blueprint_name="/Game/Blueprints/WBP_TestWidget"
)
```

**Expected**: Widget Blueprint created with UserWidget parent

---

### Test 3.2: ActorComponent Blueprint
```python
manage_blueprint(
    action="create",
    name="BP_TestComponent",
    parent_class="ActorComponent"
)

# Verify creation
manage_blueprint(
    action="get_info",
    blueprint_name="/Game/Blueprints/BP_TestComponent"
)
```

**Expected**: Component Blueprint created with ActorComponent parent

---

### Test 3.3: GameMode Blueprint
```python
manage_blueprint(
    action="create",
    name="BP_TestGameMode",
    parent_class="GameMode"
)

# Verify creation and check for GameMode-specific properties
manage_blueprint(
    action="get_info",
    blueprint_name="/Game/Blueprints/BP_TestGameMode",
    include_class_defaults=True
)
```

**Expected**: GameMode Blueprint with framework properties

---

### Cleanup Test 3
Delete WBP_TestWidget, BP_TestComponent, and BP_TestGameMode manually

---

## Test 4: Reparenting Scenarios

**Purpose**: Test reparenting with different class hierarchies

### Test 4.1: Actor → Pawn → Character
```python
# Create as Actor
manage_blueprint(
    action="create",
    name="BP_ReparentTest",
    parent_class="Actor"
)

# Verify initial parent
manage_blueprint(
    action="get_info",
    blueprint_name="/Game/Blueprints/BP_ReparentTest"
)

# Reparent to Pawn
manage_blueprint(
    action="reparent",
    blueprint_name="/Game/Blueprints/BP_ReparentTest",
    new_parent_class="Pawn"
)

# Verify change
manage_blueprint(
    action="get_info",
    blueprint_name="/Game/Blueprints/BP_ReparentTest"
)

# Reparent to Character
manage_blueprint(
    action="reparent",
    blueprint_name="/Game/Blueprints/BP_ReparentTest",
    new_parent_class="Character"
)

# Verify final state
manage_blueprint(
    action="get_info",
    blueprint_name="/Game/Blueprints/BP_ReparentTest"
)
```

**Expected**: 
- Each reparent operation succeeds
- Parent class changes at each step
- Component hierarchy updates automatically
- New parent's properties become available

---

### Test 4.2: Reparent to Custom Parent Class
```python
# Assumes you have a custom C++ or Blueprint parent class
# Example: Reparent to a project-specific base class
manage_blueprint(
    action="reparent",
    blueprint_name="/Game/Blueprints/BP_ReparentTest",
    new_parent_class="YourCustomBaseClass"
)
```

**Expected**: Reparenting to custom classes works if they exist

---

### Cleanup Test 4
Delete BP_ReparentTest manually

---

## Test 5: Event Graph Analysis

**Purpose**: Test event graph inspection on Blueprints with actual logic

### Prerequisites
Create a Blueprint with some event graph nodes (manually add nodes in Unreal Editor):
1. Create BP_EventTest (Actor)
2. Add Event BeginPlay node
3. Add Print String node connected to BeginPlay
4. Create a custom event "OnTestEvent"

### Test 5.1: List Custom Events
```python
manage_blueprint(
    action="list_custom_events",
    blueprint_name="/Game/Blueprints/BP_EventTest"
)
```

**Expected**: 
- events: ["OnTestEvent"]
- Success: true

---

### Test 5.2: Summarize Event Graph
```python
manage_blueprint(
    action="summarize_event_graph",
    blueprint_name="/Game/Blueprints/BP_EventTest",
    max_nodes=200
)
```

**Expected**:
- Summary includes Event BeginPlay
- Summary includes Print String node
- Summary includes OnTestEvent
- Node connections shown
- Readable format

---

### Test 5.3: Limit Node Count
```python
manage_blueprint(
    action="summarize_event_graph",
    blueprint_name="/Game/Blueprints/BP_EventTest",
    max_nodes=5
)
```

**Expected**: Summary shows maximum 5 nodes

---

### Cleanup Test 5
Delete BP_EventTest manually

---

## Test 6: Compilation States

**Purpose**: Test compilation with various Blueprint states

### Test 6.1: Compile Fresh Blueprint
```python
# Create new Blueprint
manage_blueprint(
    action="create",
    name="BP_CompileTest",
    parent_class="Actor"
)

# Compile immediately
manage_blueprint(
    action="compile",
    blueprint_name="/Game/Blueprints/BP_CompileTest"
)
```

**Expected**: Compiles successfully even with no changes

---

### Test 6.2: Compile After Property Changes
```python
# Set a property
manage_blueprint(
    action="set_property",
    blueprint_name="/Game/Blueprints/BP_CompileTest",
    property_name="bCanBeDamaged",
    property_value=False
)

# Compile
manage_blueprint(
    action="compile",
    blueprint_name="/Game/Blueprints/BP_CompileTest"
)

# Verify property persisted
manage_blueprint(
    action="get_property",
    blueprint_name="/Game/Blueprints/BP_CompileTest",
    property_name="bCanBeDamaged"
)
```

**Expected**: Compilation applies property changes

---

### Test 6.3: Compile After Reparenting
```python
# Reparent
manage_blueprint(
    action="reparent",
    blueprint_name="/Game/Blueprints/BP_CompileTest",
    new_parent_class="Pawn"
)

# Compile
manage_blueprint(
    action="compile",
    blueprint_name="/Game/Blueprints/BP_CompileTest"
)

# Get info to verify compiled state
manage_blueprint(
    action="get_info",
    blueprint_name="/Game/Blueprints/BP_CompileTest"
)
```

**Expected**: Blueprint compiles successfully after reparenting

---

### Cleanup Test 6
Delete BP_CompileTest manually

---

## Test 7: Error Handling

**Purpose**: Test error conditions and edge cases

### Test 7.1: Invalid Action
```python
manage_blueprint(
    action="invalid_action",
    blueprint_name="/Game/Blueprints/BP_Test"
)
```

**Expected**: 
- Success: false
- Error message listing valid actions

---

### Test 7.2: Missing Required Parameters
```python
# Missing name for create
manage_blueprint(
    action="create",
    parent_class="Actor"
)

# Missing blueprint_name for compile
manage_blueprint(
    action="compile"
)

# Missing property_name for get_property
manage_blueprint(
    action="get_property",
    blueprint_name="/Game/Blueprints/BP_Test"
)
```

**Expected**: Error messages indicating required parameters

---

### Test 7.3: Non-existent Blueprint
```python
manage_blueprint(
    action="get_info",
    blueprint_name="/Game/Blueprints/NonExistentBP"
)
```

**Expected**: Error message about Blueprint not found

---

### Test 7.4: Invalid Property Name
```python
manage_blueprint(
    action="get_property",
    blueprint_name="/Game/Blueprints/BP_TestActor",
    property_name="NonExistentProperty"
)
```

**Expected**: Error message about property not found

---

### Test 7.5: Invalid Parent Class
```python
manage_blueprint(
    action="create",
    name="BP_InvalidParent",
    parent_class="NonExistentClass"
)
```

**Expected**: Error about invalid parent class

---

## Test 8: Integration Workflow

**Purpose**: Test realistic workflow combining multiple tools

### Scenario: Create and Configure a Player Character

```python
# 1. Create Character Blueprint
manage_blueprint(
    action="create",
    name="BP_MyPlayer",
    parent_class="Character"
)

# 2. Configure movement properties
manage_blueprint(
    action="set_property",
    blueprint_name="/Game/Blueprints/BP_MyPlayer",
    property_name="bUseControllerRotationYaw",
    property_value=False
)

manage_blueprint(
    action="set_property",
    blueprint_name="/Game/Blueprints/BP_MyPlayer",
    property_name="JumpZVelocity",
    property_value=600.0
)

# 3. Compile
manage_blueprint(
    action="compile",
    blueprint_name="/Game/Blueprints/BP_MyPlayer"
)

# 4. Verify configuration
manage_blueprint(
    action="get_info",
    blueprint_name="/Game/Blueprints/BP_MyPlayer",
    include_class_defaults=True
)

# 5. Check event graph (should have default Character events)
manage_blueprint(
    action="summarize_event_graph",
    blueprint_name="/Game/Blueprints/BP_MyPlayer",
    max_nodes=100
)

# Now you would use other tools to:
# - Add components (manage_blueprint_component)
# - Add variables (manage_blueprint_variable)
# - Add functions (manage_blueprint_function)
# - Add event graph nodes (manage_blueprint_node)
```

**Expected**: Complete workflow executes successfully

---

### Cleanup Test 8
Delete BP_MyPlayer manually

---

## Summary of Test Coverage

### ✅ All 8 Actions Tested:
1. **create** - Multiple Blueprint types tested
2. **get_info** - Verified at various lifecycle stages
3. **get_property** - Various property types tested
4. **set_property** - Boolean, float, string properties tested
5. **compile** - Multiple compilation scenarios tested
6. **reparent** - Different class hierarchies tested
7. **list_custom_events** - Empty and populated event lists tested
8. **summarize_event_graph** - Various node counts and complexity tested

### Test Matrix

| Test # | create | compile | get_info | get_property | set_property | reparent | list_custom_events | summarize_event_graph |
|--------|--------|---------|----------|--------------|--------------|----------|-------------------|---------------------|
| Test 1 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Test 2 | ✅ | - | - | ✅ | ✅ | - | - | - |
| Test 3 | ✅ | - | ✅ | - | - | - | - | - |
| Test 4 | ✅ | - | ✅ | - | - | ✅ | - | - |
| Test 5 | - | - | - | - | - | - | ✅ | ✅ |
| Test 6 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | - | - |
| Test 7 | - | - | - | - | - | - | - | - |
| Test 8 | ✅ | ✅ | ✅ | - | ✅ | - | - | ✅ |

---

## Best Practices

### 1. Always Follow Dependency Order After Creation
After creating a Blueprint with `action="create"`, add elements in this order:
1. Variables FIRST
2. Components SECOND  
3. Functions THIRD
4. Event Graph nodes LAST

### 2. Compile Frequently
- Compile after making property changes
- Compile after reparenting
- Compile after adding components or variables
- Check compilation status with get_info

### 3. Use Full Blueprint Paths
- Always use full paths: `/Game/Blueprints/BP_Name`
- Avoid partial names for better performance

### 4. Verify Changes
- Use get_info after major changes
- Use get_property to verify set_property operations
- Check event graph after node additions

### 5. Handle Errors Gracefully
- Check success field in all responses
- Read error messages for troubleshooting
- Validate Blueprint existence before operations

---

## Common Property Names by Blueprint Type

### Actor/Pawn/Character
- `bCanBeDamaged` (bool)
- `InitialLifeSpan` (float)
- `Tags` (array)
- `bUseControllerRotationYaw` (bool) - Pawn/Character
- `JumpZVelocity` (float) - Character
- `MaxWalkSpeed` (float) - Character

### UserWidget (UMG)
- `bIsFocusable` (bool)
- `Visibility` (enum)
- `RenderOpacity` (float)

### ActorComponent
- `bAutoActivate` (bool)
- `PrimaryComponentTick` (struct)

### GameMode
- `DefaultPawnClass` (class)
- `PlayerControllerClass` (class)
- `GameStateClass` (class)

---

## Related Tools

After using `manage_blueprint` for basic Blueprint setup, use these tools for detailed configuration:

- **manage_blueprint_component** - Add and configure Blueprint components
- **manage_blueprint_variable** - Create and manage Blueprint variables
- **manage_blueprint_function** - Create and manage Blueprint functions
- **manage_blueprint_node** - Add and connect nodes in Event Graph
- **search_items** - Find Blueprints in your project
- **compile_blueprint** - Standalone compilation (also available as manage_blueprint action)

---

## Troubleshooting

### Issue: Blueprint not found
**Solution**: 
- Verify Blueprint exists with search_items
- Use full path: `/Game/Blueprints/BP_Name`
- Check spelling and case sensitivity

### Issue: Property not found
**Solution**:
- Use get_info with include_class_defaults=True to see available properties
- Check property name spelling
- Verify property exists for that Blueprint type

### Issue: Compilation fails
**Solution**:
- Check error message in response
- Verify all required properties are set
- Check for missing connections in Event Graph
- Try reparenting if component hierarchy issues

### Issue: Reparenting fails
**Solution**:
- Verify new parent class exists
- Check that new parent is compatible
- Some reparenting operations may require manual fixes in Unreal Editor

---

## Notes

- These tests assume a clean Unreal Engine project with VibeUE plugin installed
- Some tests require manual Blueprint setup in Unreal Editor (e.g., adding event graph nodes)
- Always verify MCP connection before running tests
- Blueprint deletion is not available through manage_blueprint - use Unreal Editor
- Property values and available properties depend on the parent class
- Event graph summarization requires the Blueprint to have been opened in the editor at least once

---

**Last Updated**: Test prompts created for Phase 5, Task 2  
**Tool Version**: manage_blueprint with 8 actions  
**Status**: Ready for testing ✅

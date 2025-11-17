# manage_blueprint Test Prompts

## Prerequisites
- ✅ Unreal Engine 5.7+ running
- ✅ VibeUE plugin loaded and active
- ✅ MCP server connection established
- ✅ Working in test project (not production)

## Setup: Create Test Assets

**Run these commands BEFORE starting tests:**

```
Create Blueprint "BP_TestActor" with parent "Actor"
Create Blueprint "BP_TestCharacter" with parent "Character"
Create Blueprint "WBP_TestWidget" with parent "UserWidget"
```

## Overview
This document tests all 8 actions of the `manage_blueprint` MCP tool following natural Blueprint lifecycle workflows.

## Test 1: Complete Blueprint Lifecycle

**Purpose**: Test create, get_info, compile, and property management in a complete workflow

### Steps

1. **Create Blueprint**
   ```
   Create a new Actor Blueprint named BP_TestActor with Actor as parent class
   ```

2. **Get Blueprint Info**
   ```
   Get comprehensive information about /Game/Blueprints/BP_TestActor including variables, components, and functions
   ```

3. **Get Property Value**
   ```
   Get the property value for a default Actor property (like bReplicates) from BP_TestActor
   ```

4. **Set Property Value**
   ```
   Set a property value on BP_TestActor (e.g., set bReplicates to true)
   ```

5. **Compile Blueprint**
   ```
   Compile the BP_TestActor Blueprint to ensure it's valid
   ```

6. **Verify Changes**
   ```
   Get info again to verify the property was set and Blueprint compiled successfully
   ```

### Expected Outcomes
- ✅ Blueprint created at /Game/Blueprints/BP_TestActor
- ✅ get_info returns Blueprint structure with empty variable/function lists
- ✅ get_property returns current property value
- ✅ set_property successfully updates the property
- ✅ compile succeeds with no errors
- ✅ Second get_info shows updated property value

### Cleanup
```
Delete BP_TestActor Blueprint from Content Browser or leave for additional tests
```

---

## Test 2: Blueprint Reparenting

**Purpose**: Test changing Blueprint parent class

### Steps

1. **Create Character Blueprint**
   ```
   Create Blueprint named BP_TestCharacter with Character parent class
   ```

2. **Verify Initial Parent**
   ```
   Get info to confirm parent class is Character
   ```

3. **Reparent to Pawn**
   ```
   Reparent BP_TestCharacter to Pawn class
   ```

4. **Compile After Reparent**
   ```
   Compile the Blueprint after reparenting
   ```

5. **Verify New Parent**
   ```
   Get info to confirm parent class changed to Pawn
   ```

### Expected Outcomes
- ✅ Blueprint created with Character parent
- ✅ Initial get_info shows parent_class: Character
- ✅ Reparent succeeds
- ✅ Compilation succeeds
- ✅ Final get_info shows parent_class: Pawn

### Cleanup
```
Delete BP_TestCharacter Blueprint
```

---

## Test 3: Event Graph Analysis

**Purpose**: Test list_custom_events and summarize_event_graph actions

### Steps

1. **Use Existing Blueprint**
   ```
   Use BP_TestActor from Test 1 or create new Blueprint
   ```

2. **List Custom Events**
   ```
   List all custom events in the Blueprint's event graph
   ```

3. **Summarize Event Graph**
   ```
   Get a readable summary/outline of the event graph (max 200 nodes)
   ```

4. **Test with Limit**
   ```
   Summarize event graph with max_nodes=50 to test truncation
   ```

### Expected Outcomes
- ✅ list_custom_events returns empty array for new Blueprint
- ✅ summarize_event_graph returns formatted outline
- ✅ Graph summary includes node types and connections
- ✅ max_nodes parameter limits output correctly

### Notes
- Custom events would only appear if manually created in Blueprint
- Event graph summary useful for understanding Blueprint logic
- For complex Blueprints, limit max_nodes to avoid overwhelming output

---

## Test 4: Property Type Testing

**Purpose**: Test get/set with different property types

### Steps

1. **Test Boolean Property**
   ```
   Set bReplicates property (boolean) on BP_TestActor and verify
   ```

2. **Test Float Property**
   ```
   Set InitialLifeSpan property (float) and verify value
   ```

3. **Test Integer Property**
   ```
   Set any integer property and verify
   ```

4. **Test Invalid Property**
   ```
   Try to get/set a non-existent property to test error handling
   ```

### Expected Outcomes
- ✅ Boolean properties accept true/false values
- ✅ Float properties accept decimal values
- ✅ Integer properties accept whole numbers
- ✅ Invalid property names return proper error messages
- ✅ Type mismatches are handled gracefully

---

## Test 5: Compilation Error Handling

**Purpose**: Test Blueprint compilation with various states

### Steps

1. **Compile Valid Blueprint**
   ```
   Compile a clean Blueprint - should succeed
   ```

2. **Get Compilation State**
   ```
   Use get_info to check if Blueprint shows as compiled
   ```

3. **Test Recompilation**
   ```
   Compile already-compiled Blueprint - should still succeed
   ```

### Expected Outcomes
- ✅ Valid Blueprint compiles successfully
- ✅ get_info reflects compilation state
- ✅ Recompiling compiled Blueprint works
- ✅ Compilation status visible in Blueprint info

---

## Integration Tests

### Blueprint → Component → Property Workflow
```
1. Create Blueprint
2. Get info to verify structure
3. Set class default properties
4. Compile Blueprint
5. Verify all changes persisted
```

### Discovery → Modification → Validation Workflow
```
1. List all Blueprints (use asset search)
2. Get info on specific Blueprint
3. Modify properties
4. Compile
5. Summarize event graph to validate
```

---

## Common Issues & Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| "Blueprint not found" | Wrong path or name | Use exact package path from search |
| Compilation fails | Invalid property values | Check property types match |
| Property not found | Typo in property name | Use exact property name (case-sensitive) |
| Can't set property | Property is read-only | Verify property is editable |

---

## Cleanup: Delete Test Assets

**Run these commands AFTER completing all tests:**

```
Delete all test Blueprints created during testing:
- Delete /Game/Blueprints/BP_TestActor with force_delete=True and show_confirmation=False
- Delete /Game/Blueprints/BP_TestCharacter with force_delete=True and show_confirmation=False
- Delete /Game/Blueprints/WBP_TestWidget with force_delete=True and show_confirmation=False
```

**Note**: Using `force_delete=True` and `show_confirmation=False` allows automated cleanup without user clicks.

---

## Reference: All Actions Summary

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| **create** | Create new Blueprint | name, parent_class |
| **get_info** | Get Blueprint structure | blueprint_name, include_class_defaults |
| **get_property** | Get class default property | blueprint_name, property_name |
| **set_property** | Set class default property | blueprint_name, property_name, property_value |
| **compile** | Compile Blueprint | blueprint_name |
| **reparent** | Change parent class | blueprint_name, new_parent_class |
| **list_custom_events** | List custom events | blueprint_name |
| **summarize_event_graph** | Get graph outline | blueprint_name, max_nodes |

---

**Test Coverage**: 8/8 actions tested ✅  
**Last Updated**: November 3, 2025  
**Related Issues**: #69 (structure), #70 (this test)

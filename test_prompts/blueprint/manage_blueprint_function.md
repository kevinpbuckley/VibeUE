# manage_blueprint_function Test Prompts

## Prerequisites
- ✅ Unreal Engine 5.7+ running
- ✅ VibeUE plugin loaded
- ✅ MCP connection active
- ✅ Test Blueprint available

## Setup: Create Test Assets

**Run these commands BEFORE starting tests:**

```
Create Blueprint "BP_FunctionTest" with parent "Actor"
```

## Overview
Tests all 13 actions of `manage_blueprint_function` including function creation, parameter management, local variables, and function metadata.

## Test 1: Function Lifecycle

**Purpose**: Create, inspect, and delete functions

### Steps

1. **Create Test Blueprint**
   ```
   Create BP_FunctionTest Blueprint with Actor parent
   ```

2. **List Existing Functions**
   ```
   List all functions in BP_FunctionTest (should show defaults like UserConstructionScript)
   ```

3. **Create Function**
   ```
   Create new function named "CalculateHealth"
   ```

4. **Get Function Details**
   ```
   Get detailed information about CalculateHealth function
   ```

5. **List Functions Again**
   ```
   Verify CalculateHealth appears in function list
   ```

### Expected Outcomes
- ✅ New Blueprint has default functions
- ✅ create adds CalculateHealth function
- ✅ get returns function details with node count
- ✅ list shows CalculateHealth in results

---

## Test 2: Parameter Management

**Purpose**: Add, update, and remove function parameters

### Steps

1. **List Initial Parameters**
   ```
   List parameters of CalculateHealth (should be empty initially)
   ```

2. **Add Input Parameter**
   ```
   Add input parameter "BaseHealth" of type float to CalculateHealth
   ```

3. **Add Second Input**
   ```
   Add input parameter "Modifier" of type float
   ```

4. **Add Output Parameter**
   ```
   Add output parameter "ResultHealth" of type float with direction "out"
   ```

5. **List All Parameters**
   ```
   List params to verify all three were added
   ```

6. **Update Parameter Type**
   ```
   Update BaseHealth parameter type from float to int
   ```

7. **Remove Parameter**
   ```
   Remove the Modifier parameter
   ```

8. **Verify Final State**
   ```
   List params to confirm Modifier removed and BaseHealth is now int
   ```

### Expected Outcomes
- ✅ list_params initially empty (except auto-generated exec pin)
- ✅ add_param creates BaseHealth input
- ✅ Multiple input params can be added
- ✅ direction "out" creates output parameter (NOT "output")
- ✅ list_params shows execute, BaseHealth, Modifier, ResultHealth
- ✅ update_param changes BaseHealth to int
- ✅ remove_param deletes Modifier
- ✅ Final list shows only BaseHealth (int) and ResultHealth (float)

### Critical Notes
⚠️ **Direction Values**:
- ✅ Use `"input"` for input parameters
- ✅ Use `"out"` for output parameters (NOT "output"!)
- ❌ "output" will cause "Invalid direction" error

---

## Test 3: Local Variables

**Purpose**: Manage function-scoped local variables

### Steps

1. **List Local Variables**
   ```
   List local variables in CalculateHealth (initially empty)
   ```

2. **Add Local Variable**
   ```
   Add local variable "TempResult" of type float
   ```

3. **Add Second Local**
   ```
   Add local variable "Multiplier" of type float
   ```

4. **List Locals**
   ```
   Verify both local variables exist
   ```

5. **Update Local Type**
   ```
   Update TempResult from float to int
   ```

6. **Remove Local**
   ```
   Remove the Multiplier local variable
   ```

7. **Verify Final Locals**
   ```
   List locals - should show only TempResult (int)
   ```

### Expected Outcomes
- ✅ list_locals initially empty
- ✅ add_local creates TempResult
- ✅ Multiple locals can be added
- ✅ list_locals shows all locals
- ✅ update_local changes type
- ✅ remove_local deletes variable
- ✅ Locals persist within function scope

---

## Test 4: Function Properties

**Purpose**: Update function metadata and properties

### Steps

1. **Update to Pure Function**
   ```
   Update CalculateHealth properties: BlueprintPure = true
   ```

2. **Set Category**
   ```
   Update properties: Category = "Combat|Health"
   ```

3. **Enable Call in Editor**
   ```
   Update properties: CallInEditor = true
   ```

4. **Get Function Info**
   ```
   Verify function properties were applied
   ```

### Expected Outcomes
- ✅ BlueprintPure makes function pure (no exec pins)
- ✅ Category organizes function in palette
- ✅ CallInEditor allows editor-time execution
- ✅ Properties persist after setting

---

## Test 5: Function Recreation Workflow

**Purpose**: Complete workflow to recreate function from another Blueprint

### Steps

1. **Discover Original Function**
   ```
   Use list_params on existing function in BP_Original
   ```

2. **Create New Function**
   ```
   Create same function name in BP_FunctionTest
   ```

3. **Add All Parameters**
   ```
   For each param in original (except execute):
   - Add param with exact name, type, and direction from original
   ```

4. **Verify Match**
   ```
   List params on both functions and compare
   ```

### Expected Outcomes
- ✅ list_params captures full function signature
- ✅ Parameters recreated with correct types
- ✅ Directions match (input vs out)
- ✅ Function signatures identical

---

## Test 6: Parameter Type Testing

**Purpose**: Test various parameter types

### Steps

1. **Create Test Function**
   ```
   Create function "TypeTest"
   ```

2. **Add Primitive Types**
   ```
   Add params: int, float, bool, string, name
   ```

3. **Add Object Reference**
   ```
   Add param type "object:AActor" for actor reference
   ```

4. **Add Struct Type**
   ```
   Add param type "struct:FVector" for vector
   ```

5. **List Parameters**
   ```
   Verify all parameter types created correctly
   ```

### Expected Outcomes
- ✅ Primitive types (int, float, bool, string, name) work
- ✅ Object references use "object:ClassName" format
- ✅ Structs use "struct:StructName" format
- ✅ All types display correctly in list_params

### Type Format Reference
- **Primitives**: `"int"`, `"float"`, `"bool"`, `"string"`, `"byte"`, `"name"`
- **Objects**: `"object:AActor"`, `"object:UUserWidget"`, `"object:ABP_Enemy_C"`
- **Structs**: `"struct:FVector"`, `"struct:FRotator"`
- ⚠️ **Note**: "real" from list_params → use "float" when adding params

---

## Test 7: Function Deletion

**Purpose**: Clean up and remove functions

### Steps

1. **Delete Function**
   ```
   Delete the CalculateHealth function
   ```

2. **Verify Removal**
   ```
   List functions - CalculateHealth should be gone
   ```

3. **Delete TypeTest**
   ```
   Delete TypeTest function
   ```

### Expected Outcomes
- ✅ delete removes function from Blueprint
- ✅ Deleted function no longer in list
- ✅ Blueprint remains valid after deletion

---

## Common Issues & Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| "Invalid direction" error | Used "output" instead of "out" | Use direction="out" for output params |
| Parameter type mismatch | Used "real" instead of "float" | Use "float" when adding params |
| Can't add parameter | Function doesn't exist | Create function first |
| Local variable error | Wrong parameter name | Use param_name for local var name |

---

## Reference: All Actions Summary

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| **list** | List all functions | blueprint_name |
| **get** | Get function details | blueprint_name, function_name |
| **list_params** | List parameters | blueprint_name, function_name |
| **create** | Create function | blueprint_name, function_name |
| **add_param** | Add parameter | blueprint_name, function_name, param_name, type, direction |
| **update_param** | Update parameter | blueprint_name, function_name, param_name, direction, new_type, new_name |
| **remove_param** | Remove parameter | blueprint_name, function_name, param_name, direction |
| **list_locals** | List local variables | blueprint_name, function_name |
| **add_local** | Add local variable | blueprint_name, function_name, param_name, type |
| **update_local** | Update local type | blueprint_name, function_name, param_name, new_type |
| **remove_local** | Remove local | blueprint_name, function_name, param_name |
| **update_properties** | Update metadata | blueprint_name, function_name, properties dict |
| **delete** | Delete function | blueprint_name, function_name |

---

## Cleanup: Delete Test Assets

**Run these commands AFTER completing all tests:**

```
Delete test Blueprint:
- Delete /Game/Blueprints/BP_FunctionTest with force_delete=True and show_confirmation=False
```

---

**Test Coverage**: 13/13 actions tested ✅  
**Last Updated**: November 4, 2025  
**Related Issues**: #69, #72


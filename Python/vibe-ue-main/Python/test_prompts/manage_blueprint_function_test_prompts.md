# Test Prompts for manage_blueprint_function

This document contains test prompts for all 13 actions of the `manage_blueprint_function` tool.

## Overview

The test workflow demonstrates:
1. Creating a test Blueprint
2. Listing existing functions
3. Creating a new function "CalculateHealth"
4. Adding input parameters (BaseHealth, Modifier)
5. Adding output parameter (ResultHealth)
6. Listing parameters to verify
7. Adding local variable (TempValue)
8. Updating function properties (pure, category)
9. Updating parameter types
10. Removing local variable
11. Deleting the function

## Prerequisites

Before running these test prompts, ensure:
- Unreal Engine is running with the VibeUE plugin enabled
- A test project is loaded
- The MCP server is connected and ready

## Test Workflow

### Step 1: Create Test Blueprint

**Prompt:**
```
Create a new Blueprint Actor called BP_TestFunctions in the /Game/Tests/ directory.
```

**Expected Result:**
- Blueprint created successfully at `/Game/Tests/BP_TestFunctions`

---

### Step 2: List Existing Functions (Action: list)

**Prompt:**
```
List all functions in the BP_TestFunctions Blueprint at /Game/Tests/BP_TestFunctions.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="list"
)
```

**Expected Result:**
```json
{
    "functions": [
        {"name": "UserConstructionScript", "node_count": 1}
    ]
}
```

---

### Step 3: Create New Function (Action: create)

**Prompt:**
```
Create a new function called "CalculateHealth" in the BP_TestFunctions Blueprint.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="create",
    function_name="CalculateHealth"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "graph_guid": "..."
}
```

---

### Step 4: Get Function Details (Action: get)

**Prompt:**
```
Get the details of the CalculateHealth function in BP_TestFunctions.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="get",
    function_name="CalculateHealth"
)
```

**Expected Result:**
```json
{
    "name": "CalculateHealth",
    "node_count": 2,
    "graph_guid": "..."
}
```

---

### Step 5: Add Input Parameter - BaseHealth (Action: add_param)

**Prompt:**
```
Add an input parameter called "BaseHealth" of type float to the CalculateHealth function.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="add_param",
    function_name="CalculateHealth",
    param_name="BaseHealth",
    direction="input",
    type="float"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "param_name": "BaseHealth"
}
```

---

### Step 6: Add Input Parameter - Modifier (Action: add_param)

**Prompt:**
```
Add another input parameter called "Modifier" of type float to the CalculateHealth function.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="add_param",
    function_name="CalculateHealth",
    param_name="Modifier",
    direction="input",
    type="float"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "param_name": "Modifier"
}
```

---

### Step 7: Add Output Parameter - ResultHealth (Action: add_param)

**Prompt:**
```
Add an output parameter called "ResultHealth" of type float to the CalculateHealth function.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="add_param",
    function_name="CalculateHealth",
    param_name="ResultHealth",
    direction="out",
    type="float"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "param_name": "ResultHealth"
}
```

**Note:** Direction must be "out" not "output"!

---

### Step 8: List Function Parameters (Action: list_params)

**Prompt:**
```
List all parameters of the CalculateHealth function to verify they were added correctly.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="list_params",
    function_name="CalculateHealth"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "parameters": [
        {"name": "BaseHealth", "direction": "input", "type": "float"},
        {"name": "Modifier", "direction": "input", "type": "float"},
        {"name": "execute", "direction": "out", "type": "exec"},
        {"name": "ResultHealth", "direction": "out", "type": "float"}
    ],
    "count": 4
}
```

**Validation:**
- Should see 2 input parameters: BaseHealth, Modifier
- Should see 2 output parameters: execute (auto-generated), ResultHealth
- All types should be correct

---

### Step 9: Add Local Variable (Action: add_local)

**Prompt:**
```
Add a local variable called "TempValue" of type float to the CalculateHealth function.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="add_local",
    function_name="CalculateHealth",
    param_name="TempValue",
    type="float"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "param_name": "TempValue"
}
```

---

### Step 10: List Local Variables (Action: list_locals)

**Prompt:**
```
List all local variables in the CalculateHealth function.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="list_locals",
    function_name="CalculateHealth"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "locals": [
        {"name": "TempValue", "type": "float"}
    ],
    "count": 1
}
```

---

### Step 11: Update Function Properties (Action: update_properties)

**Prompt:**
```
Update the CalculateHealth function to make it pure and set its category to "Combat|Health".
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="update_properties",
    function_name="CalculateHealth",
    properties={
        "BlueprintPure": True,
        "Category": "Combat|Health"
    }
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "properties_updated": ["BlueprintPure", "Category"]
}
```

---

### Step 12: Update Parameter Type (Action: update_param)

**Prompt:**
```
Update the Modifier parameter to be an integer instead of a float.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="update_param",
    function_name="CalculateHealth",
    param_name="Modifier",
    direction="input",
    new_type="int"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "param_name": "Modifier",
    "new_type": "int"
}
```

---

### Step 13: Update Parameter Name (Action: update_param)

**Prompt:**
```
Rename the Modifier parameter to HealthModifier.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="update_param",
    function_name="CalculateHealth",
    param_name="Modifier",
    direction="input",
    new_name="HealthModifier"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "param_name": "Modifier",
    "new_name": "HealthModifier"
}
```

---

### Step 14: Update Local Variable Type (Action: update_local)

**Prompt:**
```
Update the TempValue local variable to be an integer.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="update_local",
    function_name="CalculateHealth",
    param_name="TempValue",
    new_type="int"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "param_name": "TempValue",
    "new_type": "int"
}
```

---

### Step 15: Remove Local Variable (Action: remove_local)

**Prompt:**
```
Remove the TempValue local variable from the CalculateHealth function.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="remove_local",
    function_name="CalculateHealth",
    param_name="TempValue"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "param_name": "TempValue"
}
```

---

### Step 16: Remove Parameter (Action: remove_param)

**Prompt:**
```
Remove the ResultHealth output parameter from the CalculateHealth function.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="remove_param",
    function_name="CalculateHealth",
    param_name="ResultHealth",
    direction="out"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth",
    "param_name": "ResultHealth"
}
```

---

### Step 17: Delete Function (Action: delete)

**Prompt:**
```
Delete the CalculateHealth function from the BP_TestFunctions Blueprint.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="delete",
    function_name="CalculateHealth"
)
```

**Expected Result:**
```json
{
    "success": true,
    "function_name": "CalculateHealth"
}
```

---

### Step 18: Verify Function Deletion (Action: list)

**Prompt:**
```
List all functions again to verify CalculateHealth was deleted.
```

**Tool Call:**
```python
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="list"
)
```

**Expected Result:**
```json
{
    "functions": [
        {"name": "UserConstructionScript", "node_count": 1}
    ]
}
```

**Validation:** CalculateHealth should no longer be in the list.

---

## Additional Test Cases

### Test Case: Object Reference Parameter

**Prompt:**
```
Create a function ProcessActor and add an input parameter TargetActor of type AActor object reference.
```

**Tool Calls:**
```python
# Create function
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="create",
    function_name="ProcessActor"
)

# Add object reference parameter
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="add_param",
    function_name="ProcessActor",
    param_name="TargetActor",
    direction="input",
    type="object:AActor"
)
```

---

### Test Case: Multiple Local Variables

**Prompt:**
```
Add three local variables to ProcessActor: Counter (int), IsValid (bool), and Result (float).
```

**Tool Calls:**
```python
# Add Counter
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="add_local",
    function_name="ProcessActor",
    param_name="Counter",
    type="int"
)

# Add IsValid
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="add_local",
    function_name="ProcessActor",
    param_name="IsValid",
    type="bool"
)

# Add Result
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="add_local",
    function_name="ProcessActor",
    param_name="Result",
    type="float"
)
```

---

### Test Case: Function with Multiple Outputs

**Prompt:**
```
Create a function GetPlayerInfo with three output parameters: PlayerName (string), PlayerScore (int), and IsAlive (bool).
```

**Tool Calls:**
```python
# Create function
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="create",
    function_name="GetPlayerInfo"
)

# Add PlayerName output
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="add_param",
    function_name="GetPlayerInfo",
    param_name="PlayerName",
    direction="out",
    type="string"
)

# Add PlayerScore output
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="add_param",
    function_name="GetPlayerInfo",
    param_name="PlayerScore",
    direction="out",
    type="int"
)

# Add IsAlive output
manage_blueprint_function(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="add_param",
    function_name="GetPlayerInfo",
    param_name="IsAlive",
    direction="out",
    type="bool"
)
```

---

## Summary of Actions Tested

✅ **All 13 Actions Covered:**

1. ✅ **list** - List all functions (Steps 2, 18)
2. ✅ **get** - Get function details (Step 4)
3. ✅ **list_params** - List function parameters (Step 8)
4. ✅ **create** - Create new function (Step 3, Additional tests)
5. ✅ **add_param** - Add input/output parameter (Steps 5, 6, 7, Additional tests)
6. ✅ **update_param** - Update parameter type/name (Steps 12, 13)
7. ✅ **remove_param** - Remove parameter (Step 16)
8. ✅ **list_locals** - List local variables (Step 10)
9. ✅ **add_local** - Add local variable (Step 9, Additional tests)
10. ✅ **update_local** - Update local variable (Step 14)
11. ✅ **remove_local** - Remove local variable (Step 15)
12. ✅ **update_properties** - Update function metadata (Step 11)
13. ✅ **delete** - Delete function (Step 17)

## Critical Notes

### Parameter Direction Values
- ✅ **Use "input"** for input parameters (left side of function node)
- ✅ **Use "out"** for output parameters (right side of function node)
- ❌ **Do NOT use "output"** - This will cause an "Invalid direction" error
- The "execute" pin is auto-generated when you add your first output parameter

### Type Format Reference
- **Primitives**: `"int"`, `"float"`, `"bool"`, `"string"`, `"byte"`, `"name"`
- **Objects**: `"object:ClassName"` (e.g., `"object:AActor"`)
- **Structs**: `"struct:StructName"` (e.g., `"struct:FVector"`)
- **Note**: `"real"` from list_params should be `"float"` when adding params

### Best Practices
1. Always use `list_params` to verify parameter additions
2. Use `list_locals` to verify local variable additions
3. The execute output pin is automatically created
4. Use full Blueprint paths: `/Game/Blueprints/BP_Name`
5. Test both input and output parameters
6. Test parameter and local variable updates and removals
7. Verify function deletion with a final list action

## Integration Testing

After completing these tests, the function should be ready for node management:
```python
# Use manage_blueprint_node with graph_scope="function"
manage_blueprint_node(
    blueprint_name="/Game/Tests/BP_TestFunctions",
    action="create",
    graph_scope="function",
    function_name="CalculateHealth",
    node_params={...}
)
```

## Acceptance Criteria Verification

- ✅ All 13 actions tested with specific prompts
- ✅ Parameter direction handling (input/out) validated in Steps 5-7
- ✅ Local variables tested in Steps 9-10, 14-15
- ✅ Function metadata updates verified in Step 11
- ✅ Comprehensive workflow from creation to deletion
- ✅ Additional test cases for edge scenarios

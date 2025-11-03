# manage_blueprint_function - Test Prompt

## Purpose

This test validates Blueprint function management including creating functions, adding parameters, setting return types, and inspecting function details through the VibeUE MCP tools.

## Prerequisites

- Unreal Engine is running with your project loaded
- VibeUE plugin is enabled
- MCP client is connected to the VibeUE server
- Connection verified with `check_unreal_connection`
- A test Blueprint exists (create one with `create_blueprint` or use an existing one)

## Test Steps

### Test 1: Create a Simple Function

1. Ask your AI assistant: "Create a function named 'CalculateHealth' in the Blueprint 'BP_TestActor'"

2. Verify the function was created by asking: "List all functions in 'BP_TestActor'"

### Test 2: Create Function with Parameters

3. Ask your AI assistant: "Create a function named 'AddDamage' in 'BP_TestActor' with a float input parameter named 'DamageAmount'"

4. Get information about the function to verify the parameter was added

### Test 3: Create Function with Return Value

5. Ask your AI assistant: "Create a function named 'GetPlayerScore' in 'BP_TestActor' with an integer return value"

6. Verify the return type is set correctly

### Test 4: Create Function with Multiple Parameters and Return

7. Ask your AI assistant: "Create a function named 'CalculateDamage' in 'BP_TestActor' with float input 'BaseDamage', float input 'Multiplier', and float return value"

8. Get detailed information about the function

### Test 5: Inspect Function Details

9. Ask your AI assistant: "Get detailed information about the 'CalculateDamage' function in 'BP_TestActor'"

10. Review the function signature, parameters, and return type

### Test 6: Modify Function (if supported)

11. Ask your AI assistant: "Add a boolean input parameter named 'IsCritical' to the 'CalculateDamage' function"

12. Verify the parameter was added

### Test 7: Error Handling - Duplicate Function

13. Ask your AI assistant: "Create a function named 'CalculateHealth' in 'BP_TestActor'" (same name as Test 1)

14. Verify appropriate error handling for duplicate function names

## Expected Outcomes

### Test 1 - Create Simple Function
- ✅ Function 'CalculateHealth' is created successfully
- ✅ Function appears in the Blueprint's function list
- ✅ Function has no parameters by default
- ✅ Function has no return value by default
- ✅ Confirmation message includes function name

### Test 2 - Function with Parameters
- ✅ Function 'AddDamage' is created successfully
- ✅ Input parameter 'DamageAmount' of type float is added
- ✅ Parameter appears in function signature
- ✅ Confirmation message includes parameter details

### Test 3 - Function with Return Value
- ✅ Function 'GetPlayerScore' is created successfully
- ✅ Return type is set to integer
- ✅ Return value appears in function signature
- ✅ Confirmation message includes return type

### Test 4 - Function with Multiple Parameters
- ✅ Function 'CalculateDamage' is created successfully
- ✅ Both input parameters are added correctly
- ✅ Return type is set to float
- ✅ Function signature shows all parameters and return type
- ✅ Parameters are in the correct order

### Test 5 - Inspect Function Details
- ✅ Returns complete function information
- ✅ Shows function name and access level
- ✅ Lists all input parameters with types
- ✅ Shows return value type
- ✅ May include function category or other metadata

### Test 6 - Modify Function
- ✅ New parameter is added to existing function
- ✅ Existing parameters remain unchanged
- ✅ Function signature is updated correctly
- ✅ Confirmation message indicates successful modification

### Test 7 - Error Handling
- ✅ Clear error message indicating function already exists
- ✅ No crash or unexpected behavior
- ✅ Existing function remains unchanged
- ✅ Helpful suggestion (e.g., use a different name or modify existing function)

## Notes

- Function names must be unique within a Blueprint
- Parameter types include: Boolean, Integer, Float, String, Vector, Rotator, Transform, and Object types
- Functions can have multiple input parameters but only one return value
- Pure functions (functions without side effects) can be marked as such
- Functions created through MCP start with empty graphs - nodes must be added separately
- Always compile the Blueprint after creating or modifying functions
- Functions can be categorized for organization in the Blueprint editor

## Cleanup

After testing, the test functions remain in the Blueprint. You can:
- Delete functions manually in the Blueprint editor
- Delete the entire test Blueprint
- Keep them for testing with `manage_blueprint_node` tool

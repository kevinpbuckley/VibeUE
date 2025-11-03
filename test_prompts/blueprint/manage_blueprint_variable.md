# manage_blueprint_variable - Test Prompt

## Purpose

This test validates Blueprint variable management including creating variables, setting types, configuring properties, and inspecting variable details through the VibeUE MCP tools.

## Prerequisites

- Unreal Engine is running with your project loaded
- VibeUE plugin is enabled
- MCP client is connected to the VibeUE server
- Connection verified with `check_unreal_connection`
- A test Blueprint exists (create one with `create_blueprint` or use an existing one)

## Test Steps

### Test 1: Create a Simple Variable

1. Ask your AI assistant: "Create a variable named 'Health' of type Float in the Blueprint 'BP_TestActor'"

2. Verify the variable was created by asking: "List all variables in 'BP_TestActor'"

### Test 2: Create Variable with Default Value

3. Ask your AI assistant: "Create an integer variable named 'MaxAmmo' in 'BP_TestActor' with a default value of 100"

4. Get information about the variable to verify the default value

### Test 3: Create Boolean Variable

5. Ask your AI assistant: "Create a boolean variable named 'IsAlive' in 'BP_TestActor' with a default value of true"

6. Verify the variable type and default value

### Test 4: Create String Variable

7. Ask your AI assistant: "Create a string variable named 'PlayerName' in 'BP_TestActor' with a default value of 'Player1'"

8. Confirm the variable was created with the correct value

### Test 5: Create Array Variable

9. Ask your AI assistant: "Create an array variable named 'Inventory' of type String in 'BP_TestActor'"

10. Verify the variable is created as an array type

### Test 6: Set Variable Property

11. Ask your AI assistant: "Make the 'Health' variable in 'BP_TestActor' editable in the instance details panel (Instance Editable)"

12. Verify the variable property was set

### Test 7: Set Variable Category

13. Ask your AI assistant: "Set the category of the 'Health' variable to 'Player Stats' in 'BP_TestActor'"

14. Confirm the category was applied

### Test 8: Get Variable Information

15. Ask your AI assistant: "Get detailed information about the 'Health' variable in 'BP_TestActor'"

16. Review variable type, default value, and properties

### Test 9: Error Handling - Duplicate Variable

17. Ask your AI assistant: "Create a variable named 'Health' in 'BP_TestActor'" (same name as Test 1)

18. Verify appropriate error handling

## Expected Outcomes

### Test 1 - Create Simple Variable
- ✅ Variable 'Health' is created successfully
- ✅ Variable type is set to Float
- ✅ Variable appears in the Blueprint's variable list
- ✅ Default value is 0.0 for Float type
- ✅ Confirmation message includes variable name and type

### Test 2 - Variable with Default Value
- ✅ Variable 'MaxAmmo' is created successfully
- ✅ Variable type is Integer
- ✅ Default value is set to 100
- ✅ Confirmation includes the default value

### Test 3 - Boolean Variable
- ✅ Variable 'IsAlive' is created successfully
- ✅ Variable type is Boolean
- ✅ Default value is set to true
- ✅ Boolean value is properly formatted

### Test 4 - String Variable
- ✅ Variable 'PlayerName' is created successfully
- ✅ Variable type is String
- ✅ Default value is set to "Player1"
- ✅ String value is properly quoted/formatted

### Test 5 - Array Variable
- ✅ Variable 'Inventory' is created successfully
- ✅ Variable is marked as an array type
- ✅ Array element type is String
- ✅ Default value is an empty array
- ✅ Confirmation indicates array type

### Test 6 - Set Variable Property
- ✅ 'Health' variable is marked as Instance Editable
- ✅ Property change is confirmed
- ✅ Variable will be editable in placed instances
- ✅ Other variable properties remain unchanged

### Test 7 - Set Variable Category
- ✅ Variable category is set to 'Player Stats'
- ✅ Category change is confirmed
- ✅ Variable will appear under 'Player Stats' in the editor
- ✅ Variable retains all other properties

### Test 8 - Get Variable Information
- ✅ Returns complete variable information
- ✅ Shows variable name, type, and default value
- ✅ Shows variable properties (Instance Editable, Blueprint Read Only, etc.)
- ✅ Shows category if set
- ✅ Shows tooltip if set

### Test 9 - Error Handling
- ✅ Clear error message indicating variable already exists
- ✅ No crash or unexpected behavior
- ✅ Existing variable remains unchanged
- ✅ Helpful suggestion (e.g., use a different name or modify existing variable)

## Notes

- Variable names must be unique within a Blueprint
- Supported types include: Boolean, Integer, Float, String, Name, Text, Vector, Rotator, Transform, and Object types
- Variables can be arrays, sets, or maps of any supported type
- Variable properties include:
  - Instance Editable: Can be edited on placed instances
  - Blueprint Read Only: Cannot be set, only read in Blueprint graphs
  - Expose on Spawn: Appears as parameter when spawning the Blueprint
  - Private: Only accessible within this Blueprint
  - Category: Organizes variables in the editor
  - Tooltip: Provides description in the editor
- Always compile the Blueprint after creating or modifying variables
- Variables can be promoted from literal values in the graph

## Cleanup

After testing, the test variables remain in the Blueprint. You can:
- Delete variables manually in the Blueprint editor
- Delete the entire test Blueprint
- Keep them for testing with `manage_blueprint_node` tool

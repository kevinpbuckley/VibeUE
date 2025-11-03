# manage_blueprint - Test Prompt

## Purpose

This test validates the Blueprint lifecycle management tools including creation, compilation, reparenting, and property setting. These are fundamental operations for working with Blueprints through the VibeUE MCP server.

## Prerequisites

- Unreal Engine is running with your project loaded
- VibeUE plugin is enabled
- MCP client is connected to the VibeUE server
- Connection verified with `check_unreal_connection`

## Test Steps

### Test 1: Create a New Blueprint

1. Ask your AI assistant: "Create a new Blueprint called 'BP_TestActor' based on Actor class at '/Game/Blueprints/BP_TestActor'"

2. Verify the Blueprint was created by asking: "Search for blueprints named 'BP_TestActor'"

### Test 2: Get Blueprint Information

3. Ask your AI assistant: "Get information about the Blueprint 'BP_TestActor'"

4. Review the returned information including parent class, components, and properties

### Test 3: Set Blueprint Property

5. Ask your AI assistant: "Set the 'bReplicates' property to true on 'BP_TestActor'"

6. Verify the property was set by getting blueprint info again

### Test 4: Compile Blueprint

7. Ask your AI assistant: "Compile the Blueprint 'BP_TestActor'"

8. Check that compilation succeeded without errors

### Test 5: Reparent Blueprint

9. Ask your AI assistant: "Reparent 'BP_TestActor' to use 'Pawn' as its parent class"

10. Verify the parent class changed by getting blueprint info

### Test 6: Error Handling - Invalid Blueprint

11. Ask your AI assistant: "Get information about a Blueprint named 'BP_NonExistent'"

12. Verify that an appropriate error message is returned

## Expected Outcomes

### Test 1 - Create Blueprint
- ✅ Blueprint 'BP_TestActor' is created successfully
- ✅ Blueprint appears in the Content Browser at the specified path
- ✅ Blueprint is based on Actor class
- ✅ Confirmation message includes the full asset path

### Test 2 - Get Blueprint Info
- ✅ Returns blueprint name and full path
- ✅ Shows parent class as 'Actor'
- ✅ Lists components (should have DefaultSceneRoot at minimum)
- ✅ Shows available properties

### Test 3 - Set Property
- ✅ Property is set successfully
- ✅ Confirmation message indicates the property was changed
- ✅ Getting blueprint info shows updated property value

### Test 4 - Compile
- ✅ Compilation completes successfully
- ✅ No compilation errors or warnings
- ✅ Confirmation message indicates successful compilation

### Test 5 - Reparent
- ✅ Parent class changes from 'Actor' to 'Pawn'
- ✅ Blueprint retains its existing components and settings
- ✅ Confirmation message indicates reparenting succeeded
- ✅ Getting blueprint info shows new parent class

### Test 6 - Error Handling
- ✅ Clear error message indicating Blueprint not found
- ✅ No crash or unexpected behavior
- ✅ Helpful suggestion on how to proceed (e.g., search for blueprints)

## Notes

- Blueprint creation requires a valid parent class name (Actor, Pawn, Character, etc.)
- Full asset paths should follow the format `/Game/FolderName/AssetName`
- Compilation is required for changes to take effect in the editor
- Reparenting may reset some properties depending on the parent class change
- Always compile after making significant changes to a Blueprint

## Cleanup

After testing, you may want to delete the test Blueprint:
- In Unreal Engine Content Browser, locate 'BP_TestActor'
- Right-click and select "Delete"
- Or keep it for further testing with other tools

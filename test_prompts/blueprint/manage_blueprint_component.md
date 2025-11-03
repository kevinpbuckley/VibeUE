# manage_blueprint_component - Test Prompt

## Purpose

This test validates Blueprint component management including adding components, removing components, reordering components, setting component properties, and inspecting component hierarchies.

## Prerequisites

- Unreal Engine is running with your project loaded
- VibeUE plugin is enabled
- MCP client is connected to the VibeUE server
- Connection verified with `check_unreal_connection`
- A test Blueprint exists (you can create one with `create_blueprint` or use an existing one)

## Test Steps

### Test 1: Add Component to Blueprint

1. Ask your AI assistant: "Add a Static Mesh Component named 'MeshComponent' to the Blueprint 'BP_TestActor'"

2. Verify the component was added by asking: "Get the component hierarchy for 'BP_TestActor'"

### Test 2: Add Another Component

3. Ask your AI assistant: "Add a Box Component named 'CollisionBox' to 'BP_TestActor'"

4. Check the hierarchy again to see both components

### Test 3: Get Available Components

5. Ask your AI assistant: "What components can I add to a Blueprint?"

6. Review the list of available component types

### Test 4: Get Component Information

7. Ask your AI assistant: "Get information about the 'MeshComponent' component in 'BP_TestActor'"

8. Review the component's properties and settings

### Test 5: Set Component Property

9. Ask your AI assistant: "Set the 'Mobility' property to 'Movable' on the 'MeshComponent' in 'BP_TestActor'"

10. Verify the property was set by getting component info again

### Test 6: Reorder Components

11. Ask your AI assistant: "Reorder components in 'BP_TestActor' so 'CollisionBox' comes before 'MeshComponent'"

12. Get the hierarchy to confirm the new order

### Test 7: Remove Component

13. Ask your AI assistant: "Remove the 'CollisionBox' component from 'BP_TestActor'"

14. Verify the component was removed by checking the hierarchy

### Test 8: Error Handling - Invalid Component

15. Ask your AI assistant: "Remove a component named 'NonExistentComponent' from 'BP_TestActor'"

16. Verify appropriate error handling

## Expected Outcomes

### Test 1 - Add Component
- ✅ Static Mesh Component 'MeshComponent' is added successfully
- ✅ Component appears in the component hierarchy
- ✅ Component is attached to the root component
- ✅ Confirmation message includes component name and type

### Test 2 - Add Another Component
- ✅ Box Component 'CollisionBox' is added successfully
- ✅ Both components appear in hierarchy
- ✅ Components are properly attached to the hierarchy

### Test 3 - Get Available Components
- ✅ Returns a list of available component types
- ✅ Includes common components like Static Mesh, Box, Sphere, Capsule
- ✅ Includes specialized components like Camera, Light, Audio
- ✅ List is categorized or well-organized

### Test 4 - Get Component Info
- ✅ Returns component name and type
- ✅ Shows parent component (attachment)
- ✅ Lists available properties
- ✅ Shows current property values

### Test 5 - Set Component Property
- ✅ Property is set successfully
- ✅ Confirmation message indicates the property was changed
- ✅ Getting component info shows updated property value
- ✅ Property value uses correct Unreal Engine enum or format

### Test 6 - Reorder Components
- ✅ Components are reordered successfully
- ✅ New hierarchy shows correct order
- ✅ Components maintain their properties and settings
- ✅ Confirmation message indicates successful reordering

### Test 7 - Remove Component
- ✅ Component is removed successfully
- ✅ Component no longer appears in hierarchy
- ✅ Remaining components are unaffected
- ✅ Confirmation message indicates successful removal

### Test 8 - Error Handling
- ✅ Clear error message indicating component not found
- ✅ No crash or unexpected behavior
- ✅ Helpful suggestion on how to proceed

## Notes

- The DefaultSceneRoot component cannot be removed
- Components are typically attached to existing components in the hierarchy
- Some component types may require specific parent classes
- Component properties vary by component type
- Always compile the Blueprint after making component changes
- Component reordering affects the visual hierarchy in the Blueprint editor

## Cleanup

After testing, the test components remain in the Blueprint. You can:
- Remove them manually through the test steps
- Delete the entire test Blueprint
- Keep them for further testing

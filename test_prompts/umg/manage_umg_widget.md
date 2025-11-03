# manage_umg_widget - Test Prompt

## Purpose

This test validates UMG widget creation and manipulation including creating widget blueprints, adding widget components, setting properties, styling, and inspecting widget hierarchies through the VibeUE MCP tools.

## Prerequisites

- Unreal Engine is running with your project loaded
- VibeUE plugin is enabled
- MCP client is connected to the VibeUE server
- Connection verified with `check_unreal_connection`
- Familiarity with UMG widget concepts (Canvas Panel, Buttons, Text Blocks, etc.)

## Test Steps

### Test 1: Create a Widget Blueprint

1. Ask your AI assistant: "Create a UMG widget blueprint named 'WBP_TestMenu' at '/Game/UI/WBP_TestMenu'"

2. Verify the widget was created by searching for it

### Test 2: Get Widget Information

3. Ask your AI assistant: "Get information about the widget 'WBP_TestMenu'"

4. Review the widget's components and hierarchy

### Test 3: Get Available Widget Types

5. Ask your AI assistant: "What widget component types can I add to a UMG widget?"

6. Review the list of available widget types

### Test 4: Add Canvas Panel

7. Ask your AI assistant: "Add a Canvas Panel to 'WBP_TestMenu' as the root widget"

8. Verify the Canvas Panel was added

### Test 5: Add Text Block

9. Ask your AI assistant: "Add a Text Block named 'TitleText' to the Canvas Panel in 'WBP_TestMenu'"

10. Confirm the Text Block was added to the hierarchy

### Test 6: Add Button

11. Ask your AI assistant: "Add a Button named 'PlayButton' to the Canvas Panel in 'WBP_TestMenu'"

12. Verify the button was added

### Test 7: Set Widget Text Property

13. Ask your AI assistant: "Set the text of 'TitleText' to 'Main Menu' in 'WBP_TestMenu'"

14. Confirm the text was updated

### Test 8: Set Slot Properties (Position and Size)

15. Ask your AI assistant: "Position 'TitleText' at (100, 50) with size (300, 60) in 'WBP_TestMenu'"

16. Verify the slot properties were set

### Test 9: Set Widget Style Property

17. Ask your AI assistant: "Set the font size of 'TitleText' to 36 in 'WBP_TestMenu'"

18. Confirm the style property was updated

### Test 10: Set Alignment

19. Ask your AI assistant: "Set the horizontal alignment of 'PlayButton' to HAlign_Center in 'WBP_TestMenu'"

20. Verify the alignment was set

### Test 11: Add Nested Widget

21. Ask your AI assistant: "Add a Vertical Box named 'ButtonContainer' to the Canvas Panel in 'WBP_TestMenu'"

22. Then: "Add a Button named 'OptionsButton' to 'ButtonContainer' in 'WBP_TestMenu'"

23. Confirm the nested hierarchy

### Test 12: Get Widget Component Properties

25. Ask your AI assistant: "List all properties available for the 'TitleText' component in 'WBP_TestMenu'"

26. Review the available properties

### Test 13: Compile Widget

27. Ask your AI assistant: "Compile the widget 'WBP_TestMenu'"

28. Verify compilation succeeded

### Test 14: Validate Widget Hierarchy

29. Ask your AI assistant: "Validate the widget hierarchy of 'WBP_TestMenu'"

30. Review any validation messages or warnings

### Test 15: Error Handling - Invalid Parent

31. Ask your AI assistant: "Add a Canvas Panel to 'TitleText' in 'WBP_TestMenu'" (Text Block cannot have children)

32. Verify appropriate error handling

## Expected Outcomes

### Test 1 - Create Widget Blueprint
- ✅ Widget 'WBP_TestMenu' is created successfully
- ✅ Widget appears in the Content Browser at specified path
- ✅ Widget can be opened in the UMG Designer
- ✅ Confirmation message includes full asset path

### Test 2 - Get Widget Information
- ✅ Returns widget name and path
- ✅ Shows root widget (if any)
- ✅ Lists components in the hierarchy
- ✅ Shows widget structure

### Test 3 - Get Available Widget Types
- ✅ Returns comprehensive list of widget types
- ✅ Includes containers (Canvas Panel, Vertical Box, Horizontal Box, etc.)
- ✅ Includes common widgets (Button, Text Block, Image, etc.)
- ✅ Includes input widgets (Slider, Checkbox, ComboBox, etc.)
- ✅ List is categorized or well-organized

### Test 4 - Add Canvas Panel
- ✅ Canvas Panel is added as root widget
- ✅ Canvas Panel appears in hierarchy
- ✅ Confirmation includes widget type
- ✅ Canvas Panel can contain child widgets

### Test 5 - Add Text Block
- ✅ Text Block 'TitleText' is added successfully
- ✅ Text Block is child of Canvas Panel
- ✅ Text Block has default text value
- ✅ Confirmation includes component name and parent

### Test 6 - Add Button
- ✅ Button 'PlayButton' is added successfully
- ✅ Button is child of Canvas Panel
- ✅ Button has default appearance
- ✅ Button automatically includes child Text Block

### Test 7 - Set Text Property
- ✅ Text is set to "Main Menu"
- ✅ Property change is confirmed
- ✅ Text Block retains other properties
- ✅ Text value is properly formatted

### Test 8 - Set Slot Properties
- ✅ Position is set to (100, 50)
- ✅ Size is set to (300, 60)
- ✅ Slot properties are confirmed
- ✅ Widget appears at correct position in designer

### Test 9 - Set Style Property
- ✅ Font size is set to 36
- ✅ Property change is confirmed
- ✅ Other style properties remain unchanged
- ✅ Font size value is appropriate for the property type

### Test 10 - Set Alignment
- ✅ Horizontal alignment is set to HAlign_Center
- ✅ Alignment uses correct Unreal Engine enum format
- ✅ Confirmation includes alignment value
- ✅ Widget alignment updates accordingly

### Test 11 - Add Nested Widget
- ✅ Vertical Box 'ButtonContainer' is added to Canvas Panel
- ✅ Button 'OptionsButton' is added to Vertical Box
- ✅ Hierarchy shows correct nesting
- ✅ Parent-child relationships are maintained

### Test 12 - Get Widget Properties
- ✅ Returns list of available properties
- ✅ Includes content properties (Text, Image, etc.)
- ✅ Includes style properties (Font, Color, etc.)
- ✅ Includes slot properties (Position, Size, Alignment, etc.)
- ✅ Properties are categorized

### Test 13 - Compile Widget
- ✅ Widget compiles successfully
- ✅ No compilation errors or warnings
- ✅ Confirmation message indicates success
- ✅ Widget is ready for use in game

### Test 14 - Validate Hierarchy
- ✅ Validation completes successfully
- ✅ Reports any issues with hierarchy
- ✅ Confirms proper parent-child relationships
- ✅ Checks for common mistakes

### Test 15 - Error Handling
- ✅ Clear error message indicating Text Block cannot have children
- ✅ No crash or unexpected behavior
- ✅ Widget hierarchy remains unchanged
- ✅ Helpful suggestion about valid parent types

## Notes

- Widget creation requires a valid asset path starting with '/Game/'
- Widget component hierarchy must follow UMG rules (containers can have children, text blocks cannot)
- Slot properties (Position, Size, Alignment) are prefixed with "Slot." when using `set_widget_property`
- Alignment properties use Unreal Engine enum format (HAlign_Fill, VAlign_Center, etc.)
- Canvas Panel uses absolute positioning with Position and Size
- Box containers (Vertical/Horizontal Box) use slot-based layout
- Always compile widgets after making changes for them to take effect
- Use `get_help(topic="umg-guide")` for comprehensive UMG styling guidance
- Widget properties vary by widget type
- Color properties use LinearColor format: (R, G, B, A) with values 0.0 to 1.0

## Cleanup

After testing, the test widget remains in the project. You can:
- Delete it manually through the Content Browser
- Keep it for further testing or as a reference
- Modify it to create a functional UI

# 🧪 manage_umg_widget Test Prompts - Comprehensive Testing Guide

## Overview
This document provides test prompts for the `manage_umg_widget` tool, covering all 11 actions with a complete workflow that demonstrates widget creation, property management (both widget and slot properties), event binding, and hierarchy validation.

**Test Date**: November 3, 2025  
**Tool**: `manage_umg_widget`  
**Total Actions**: 11  

---

## 📋 Test Workflow Summary

The test workflow follows this sequence:
1. Search for Button widget types (**search_types**)
2. Use existing or create new widget blueprint
3. List current components (**list_components**)
4. Add Button component (**add_component**)
5. Get component properties (**get_component_properties**)
6. Set widget properties (**set_property** - Text, ColorAndOpacity)
7. Set slot properties (**set_property** - Slot.HorizontalAlignment)
8. Get available events (**get_available_events**)
9. Bind OnClicked event (**bind_events**)
10. Validate hierarchy (**validate**)
11. Get single property value (**get_property**)
12. List all available properties (**list_properties**)
13. Remove component (**remove_component**)

---

## 🎯 Test Prompts by Action

### 1. Action: search_types
**Purpose**: Discover available Button widget types

**Test Prompt**:
```
Search for Button widget types in the UMG widget library. 
Use manage_umg_widget with action="search_types", category="Common", and search_text="Button".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="search_types",
    category="Common",
    search_text="Button",
    include_custom=True,
    include_engine=True
)
```

**Expected Result**:
- Success: true
- Returns list of Button widget types
- Should include "Button" class
- Contains widget metadata (name, display_name, category)

---

### 2. Action: list_components
**Purpose**: List all components in existing widget

**Test Prompt**:
```
List all components in the widget blueprint named "WBP_TestWidget" (or create it if it doesn't exist first).
Use manage_umg_widget with action="list_components".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="list_components",
    widget_name="WBP_TestWidget"
)
```

**Expected Result**:
- Success: true
- Returns component hierarchy
- Shows root component
- Lists any existing child components with their properties

---

### 3. Action: add_component
**Purpose**: Add a new Button component to the widget

**Test Prompt**:
```
Add a new Button component named "TestButton" to "WBP_TestWidget". 
Set its parent to the root canvas panel. 
Use manage_umg_widget with action="add_component".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="add_component",
    widget_name="WBP_TestWidget",
    component_type="Button",
    component_name="TestButton",
    parent_name="root",
    is_variable=True,
    properties={
        "Text": "Click Me"
    }
)
```

**Expected Result**:
- Success: true
- Button component created
- Component is a Blueprint variable
- Initial text property set
- Component added to hierarchy

---

### 4. Action: get_component_properties
**Purpose**: Retrieve all properties of a specific component

**Test Prompt**:
```
Get all properties of the "TestButton" component in "WBP_TestWidget".
Use manage_umg_widget with action="get_component_properties".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="get_component_properties",
    widget_name="WBP_TestWidget",
    component_name="TestButton"
)
```

**Expected Result**:
- Success: true
- Returns complete property list
- Includes appearance properties (ColorAndOpacity, etc.)
- Includes slot properties
- Shows current property values

---

### 5. Action: set_property (Widget Property - Text)
**Purpose**: Set the Text property on the Button

**Test Prompt**:
```
Set the Text property of "TestButton" in "WBP_TestWidget" to "Start Game".
Use manage_umg_widget with action="set_property".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="set_property",
    widget_name="WBP_TestWidget",
    component_name="TestButton",
    property_name="Text",
    property_value="Start Game"
)
```

**Expected Result**:
- Success: true
- Text property updated
- Button displays "Start Game"

---

### 6. Action: set_property (Widget Property - ColorAndOpacity)
**Purpose**: Set the color property on the Button

**Test Prompt**:
```
Set the ColorAndOpacity property of "TestButton" in "WBP_TestWidget" to a cyan color [0.0, 1.0, 1.0, 1.0].
Use manage_umg_widget with action="set_property".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="set_property",
    widget_name="WBP_TestWidget",
    component_name="TestButton",
    property_name="ColorAndOpacity",
    property_value=[0.0, 1.0, 1.0, 1.0]
)
```

**Expected Result**:
- Success: true
- Color property updated to cyan
- Button appears in cyan color

---

### 7. Action: set_property (Slot Property - HorizontalAlignment)
**Purpose**: Demonstrate slot property access and modification

**Test Prompt**:
```
Set the slot horizontal alignment of "TestButton" in "WBP_TestWidget" to fill the entire width.
Use manage_umg_widget with action="set_property" and property_name="Slot.HorizontalAlignment".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="set_property",
    widget_name="WBP_TestWidget",
    component_name="TestButton",
    property_name="Slot.HorizontalAlignment",
    property_value="HAlign_Fill"
)
```

**Expected Result**:
- Success: true
- Slot property updated
- Button fills horizontally in its container
- **Critical**: Demonstrates slot vs. widget property differentiation

---

### 8. Action: get_available_events
**Purpose**: Get list of available events for the Button component

**Test Prompt**:
```
Get all available events for the "TestButton" component in "WBP_TestWidget".
Use manage_umg_widget with action="get_available_events".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="get_available_events",
    widget_name="WBP_TestWidget",
    component_name="TestButton"
)
```

**Expected Result**:
- Success: true
- Returns list of available events
- Should include "OnClicked"
- Should include "OnPressed", "OnReleased", "OnHovered", "OnUnhovered"

---

### 9. Action: bind_events
**Purpose**: Bind OnClicked event to a function

**Test Prompt**:
```
Bind the OnClicked event of "TestButton" in "WBP_TestWidget" to a function named "HandleStartGameClicked".
Use manage_umg_widget with action="bind_events".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="bind_events",
    widget_name="WBP_TestWidget",
    component_name="TestButton",
    input_events={
        "OnClicked": "HandleStartGameClicked"
    }
)
```

**Expected Result**:
- Success: true
- OnClicked event bound to function
- Event graph node created
- Function signature matches event signature

---

### 10. Action: validate
**Purpose**: Validate widget hierarchy and structure

**Test Prompt**:
```
Validate the widget hierarchy and structure of "WBP_TestWidget".
Use manage_umg_widget with action="validate".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="validate",
    widget_name="WBP_TestWidget"
)
```

**Expected Result**:
- Success: true
- Returns validation status
- Reports any hierarchy issues
- Confirms widget structure is valid

---

### 11. Action: get_property
**Purpose**: Get a single property value

**Test Prompt**:
```
Get the current Text property value of "TestButton" in "WBP_TestWidget".
Use manage_umg_widget with action="get_property".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="get_property",
    widget_name="WBP_TestWidget",
    component_name="TestButton",
    property_name="Text"
)
```

**Expected Result**:
- Success: true
- Returns current Text property value ("Start Game")
- Property value matches what was set earlier

---

### 12. Action: list_properties
**Purpose**: List all available properties for a component

**Test Prompt**:
```
List all available properties for "TestButton" in "WBP_TestWidget", including inherited properties.
Use manage_umg_widget with action="list_properties".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="list_properties",
    widget_name="WBP_TestWidget",
    component_name="TestButton",
    include_inherited=True,
    category_filter=""
)
```

**Expected Result**:
- Success: true
- Returns comprehensive property list
- Includes property names and types
- Shows both widget and slot properties
- Organized by category

---

### 13. Action: remove_component
**Purpose**: Remove a component from the widget hierarchy

**Test Prompt**:
```
Remove the "TestButton" component from "WBP_TestWidget", including its children and Blueprint variable.
Use manage_umg_widget with action="remove_component".
```

**Expected MCP Call**:
```python
manage_umg_widget(
    action="remove_component",
    widget_name="WBP_TestWidget",
    component_name="TestButton",
    remove_children=True,
    remove_from_variables=True
)
```

**Expected Result**:
- Success: true
- Component removed from hierarchy
- Blueprint variable removed
- Child components removed (if any)

---

## 🔄 Complete Test Workflow Example

Below is a complete workflow that an AI assistant could execute to test all actions in sequence:

```markdown
## Test Workflow: Create and Configure a Button Widget

### Step 1: Search for Button widget types
Use manage_umg_widget to search for Button widget types:
- action: "search_types"
- category: "Common"
- search_text: "Button"

### Step 2: List existing components
Use manage_umg_widget to list components in "WBP_TestWidget":
- action: "list_components"
- widget_name: "WBP_TestWidget"

### Step 3: Add Button component
Use manage_umg_widget to add a new Button:
- action: "add_component"
- widget_name: "WBP_TestWidget"
- component_type: "Button"
- component_name: "TestButton"
- parent_name: "root"
- is_variable: True

### Step 4: Get component properties
Use manage_umg_widget to get all properties of TestButton:
- action: "get_component_properties"
- widget_name: "WBP_TestWidget"
- component_name: "TestButton"

### Step 5: Set Text property
Use manage_umg_widget to set button text:
- action: "set_property"
- widget_name: "WBP_TestWidget"
- component_name: "TestButton"
- property_name: "Text"
- property_value: "Start Game"

### Step 6: Set ColorAndOpacity property
Use manage_umg_widget to set button color to cyan:
- action: "set_property"
- widget_name: "WBP_TestWidget"
- component_name: "TestButton"
- property_name: "ColorAndOpacity"
- property_value: [0.0, 1.0, 1.0, 1.0]

### Step 7: Set Slot.HorizontalAlignment property
Use manage_umg_widget to make button fill horizontally:
- action: "set_property"
- widget_name: "WBP_TestWidget"
- component_name: "TestButton"
- property_name: "Slot.HorizontalAlignment"
- property_value: "HAlign_Fill"

### Step 8: Get available events
Use manage_umg_widget to get events for TestButton:
- action: "get_available_events"
- widget_name: "WBP_TestWidget"
- component_name: "TestButton"

### Step 9: Bind OnClicked event
Use manage_umg_widget to bind the OnClicked event:
- action: "bind_events"
- widget_name: "WBP_TestWidget"
- component_name: "TestButton"
- input_events: {"OnClicked": "HandleStartGameClicked"}

### Step 10: Validate hierarchy
Use manage_umg_widget to validate the widget structure:
- action: "validate"
- widget_name: "WBP_TestWidget"

### Step 11: Get specific property
Use manage_umg_widget to get the Text property value:
- action: "get_property"
- widget_name: "WBP_TestWidget"
- component_name: "TestButton"
- property_name: "Text"

### Step 12: List all properties
Use manage_umg_widget to list all available properties:
- action: "list_properties"
- widget_name: "WBP_TestWidget"
- component_name: "TestButton"
- include_inherited: True

### Step 13: Remove component (cleanup)
Use manage_umg_widget to remove the TestButton:
- action: "remove_component"
- widget_name: "WBP_TestWidget"
- component_name: "TestButton"
- remove_children: True
- remove_from_variables: True
```

---

## 🎓 Advanced Test Cases

### Test Case 1: Slot Property vs Widget Property Differentiation

**Objective**: Demonstrate the difference between widget properties and slot properties

**Widget Property Example**:
```python
# Set button's inherent color (widget property)
manage_umg_widget(
    action="set_property",
    widget_name="WBP_TestWidget",
    component_name="Background",
    property_name="ColorAndOpacity",
    property_value=[0.1, 0.1, 0.1, 1.0]
)
```

**Slot Property Example**:
```python
# Set how button fills its container (slot property)
manage_umg_widget(
    action="set_property",
    widget_name="WBP_TestWidget",
    component_name="Background",
    property_name="Slot.HorizontalAlignment",
    property_value="HAlign_Fill"
)
```

**Key Differences**:
- Widget properties belong to the widget itself (color, text, visibility)
- Slot properties control layout behavior within parent container
- Slot properties use "Slot." prefix
- Common slot properties: HorizontalAlignment, VerticalAlignment, Padding, Size

---

### Test Case 2: Multiple Event Bindings

**Objective**: Bind multiple events to different functions

```python
manage_umg_widget(
    action="bind_events",
    widget_name="WBP_TestWidget",
    component_name="TestButton",
    input_events={
        "OnClicked": "HandleButtonClicked",
        "OnHovered": "HandleButtonHovered",
        "OnUnhovered": "HandleButtonUnhovered",
        "OnPressed": "HandleButtonPressed",
        "OnReleased": "HandleButtonReleased"
    }
)
```

---

### Test Case 3: Complex Widget Creation

**Objective**: Create a complete button with background in one action

```python
manage_umg_widget(
    action="add_component",
    widget_name="WBP_MainMenu",
    component_type="Button",
    component_name="PlayButton",
    parent_name="CanvasPanel_Root",
    is_variable=True,
    properties={
        "Text": "Play Game",
        "ColorAndOpacity": [0.0, 1.0, 0.0, 1.0],
        "Slot.HorizontalAlignment": "HAlign_Fill",
        "Slot.VerticalAlignment": "VAlign_Top"
    }
)
```

---

## ✅ Acceptance Criteria Checklist

- [x] **All 11 actions tested**: search_types, list_components, add_component, remove_component, validate, get_component_properties, get_property, set_property, list_properties, get_available_events, bind_events
- [x] **Widget and slot properties differentiated**: Demonstrated with ColorAndOpacity (widget) vs Slot.HorizontalAlignment (slot)
- [x] **Event binding tested**: OnClicked event bound to HandleStartGameClicked function
- [x] **Hierarchy validation demonstrated**: validate action included in workflow
- [x] **Complete workflow provided**: Step-by-step test sequence covering all actions
- [x] **Slot property examples**: HorizontalAlignment set to HAlign_Fill

---

## 📊 Testing Results Template

Use this template to record test results:

| Action | Status | Notes |
|--------|--------|-------|
| search_types | ⏳ Pending | |
| list_components | ⏳ Pending | |
| add_component | ⏳ Pending | |
| get_component_properties | ⏳ Pending | |
| set_property (Text) | ⏳ Pending | |
| set_property (ColorAndOpacity) | ⏳ Pending | |
| set_property (Slot.HorizontalAlignment) | ⏳ Pending | |
| get_available_events | ⏳ Pending | |
| bind_events | ⏳ Pending | |
| validate | ⏳ Pending | |
| get_property | ⏳ Pending | |
| list_properties | ⏳ Pending | |
| remove_component | ⏳ Pending | |

**Legend**: ✅ Pass | ❌ Fail | ⏳ Pending | ⚠️ Partial

---

## 🔍 Common Issues and Solutions

### Issue 1: Component Not Found
**Symptom**: "Component 'TestButton' not found in widget"
**Solution**: Verify component was created successfully with list_components action

### Issue 2: Slot Property Not Recognized
**Symptom**: "Property 'Slot.HorizontalAlignment' not found"
**Solution**: Ensure component is child of a panel that supports slot properties

### Issue 3: Event Binding Fails
**Symptom**: "Event 'OnClicked' not available"
**Solution**: Use get_available_events to verify event name and availability

### Issue 4: Property Type Mismatch
**Symptom**: "Invalid value type for property"
**Solution**: 
- Colors: Use array [R, G, B, A] with 0.0-1.0 values
- Text: Use string values
- Enums: Use exact enum value strings (e.g., "HAlign_Fill")

---

## 💡 Best Practices

1. **Always list components first** to understand current hierarchy
2. **Use get_component_properties** before setting properties to know what's available
3. **Test slot properties separately** from widget properties to understand differences
4. **Validate hierarchy** after making structural changes
5. **Use get_property** to verify property changes were applied correctly
6. **Bind events last** after all component setup is complete

---

## 📚 Related Documentation

- **manage_umg_widget.py**: Tool implementation with complete docstrings
- **UMG-Guide.md**: Widget styling best practices and container patterns
- **help.md**: Complete reference for all MCP tools
- **node-tools-testing-report.md**: Example testing approach for multi-action tools

---

**Test Document Created**: November 3, 2025  
**Last Updated**: November 3, 2025  
**Tool Version**: manage_umg_widget v1.0  
**Dependencies**: Issue #69 (test_prompts structure)

---

## 🎯 Success Metrics

- **Total Actions**: 11
- **Workflow Steps**: 13
- **Test Cases**: 3
- **Slot Property Examples**: 4
- **Event Binding Examples**: 2
- **Complete Integration Test**: ✅

**Ready for Testing**: This comprehensive test prompt document is ready for AI assistant testing and validation of the manage_umg_widget tool.

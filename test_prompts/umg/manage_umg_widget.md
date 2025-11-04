# manage_umg_widget Test Prompts

## Prerequisites
- ✅ Unreal Engine 5.6+ running
- ✅ VibeUE plugin loaded
- ✅ MCP connection active

## Setup: Create Test Assets

**Run these commands BEFORE starting tests:**

```
Create Blueprint "WBP_TestWidget" with parent "UserWidget"
```

## Overview
Tests all 11 actions of `manage_umg_widget` covering component lifecycle, property management (including slot properties), and event binding.

## Test 1: Widget Creation and Component Listing

**Purpose**: Create widget and list initial components

### Steps

1. **Create Widget Blueprint**
   ```
   Use manage_blueprint to create WBP_TestWidget with UserWidget parent
   ```

2. **List Initial Components**
   ```
   List all components in WBP_TestWidget (should have root CanvasPanel)
   ```

3. **Verify Hierarchy**
   ```
   Check that root component exists and hierarchy is valid
   ```

### Expected Outcomes
- ✅ Widget created successfully
- ✅ list_components shows root CanvasPanel
- ✅ Hierarchy structure visible
- ✅ Component names and types listed

---

## Test 2: Widget Type Discovery

**Purpose**: Discover available widget components before adding

### Steps

1. **Search for Button Types**
   ```
   Use search_types with search_text="Button", category="Common"
   ```

2. **Search for Layout Types**
   ```
   Search for types with category filter for panels
   ```

3. **Search for Text Types**
   ```
   Search for TextBlock and other text widgets
   ```

### Expected Outcomes
- ✅ search_types returns Button component info
- ✅ Category filtering works
- ✅ Multiple widget types discoverable
- ✅ Type metadata includes properties

---

## Test 3: Add Widget Components

**Purpose**: Add various widget components to build UI

### Steps

1. **Add Button**
   ```
   Add Button component named "PlayButton" to CanvasPanel_Root
   ```

2. **Add TextBlock**
   ```
   Add TextBlock named "TitleText" to CanvasPanel_Root
   ```

3. **Add Image**
   ```
   Add Image component named "Background" to CanvasPanel_Root
   ```

4. **Add Vertical Box**
   ```
   Add VerticalBox named "MenuContainer" for layout
   ```

5. **Verify Components**
   ```
   List components to see all additions
   ```

### Expected Outcomes
- ✅ add_component creates each widget
- ✅ Components appear in hierarchy
- ✅ Parent-child relationships correct
- ✅ Different component types coexist

---

## Test 4: Widget Properties (Non-Slot)

**Purpose**: Get and set widget component properties

### Steps

1. **Get Component Properties**
   ```
   Get all properties of TitleText component
   ```

2. **Set Text Property**
   ```
   Set TitleText property "Text" to "Main Menu"
   ```

3. **Set Color**
   ```
   Set TitleText ColorAndOpacity to [1.0, 1.0, 1.0, 1.0] (white)
   ```

4. **Set Font Size**
   ```
   Set font size on TitleText (if available in properties)
   ```

5. **Verify Changes**
   ```
   Get properties again to confirm updates
   ```

### Expected Outcomes
- ✅ get_component_properties lists all properties
- ✅ Text property accepts string values
- ✅ ColorAndOpacity accepts RGBA array [R, G, B, A] (0.0-1.0)
- ✅ Properties persist after setting

---

## Test 5: Slot Properties (CRITICAL)

**Purpose**: Test slot property access for layout control

### Steps

1. **Set Background Horizontal Fill**
   ```
   Set Background component property:
   - property_name: "Slot.HorizontalAlignment"
   - property_value: "HAlign_Fill"
   ```

2. **Set Vertical Fill**
   ```
   Set Background property:
   - property_name: "Slot.VerticalAlignment"  
   - property_value: "VAlign_Fill"
   ```

3. **Set Button Alignment**
   ```
   Set PlayButton slot alignment to center
   ```

4. **Verify Slot Properties**
   ```
   Get properties to see slot property values
   ```

### Expected Outcomes
- ✅ Slot.HorizontalAlignment accepts HAlign_ values
- ✅ Slot.VerticalAlignment accepts VAlign_ values
- ✅ Background fills entire canvas
- ✅ Slot properties control layout

### Slot Property Values
**Horizontal:**
- `"HAlign_Fill"`, `"HAlign_Left"`, `"HAlign_Center"`, `"HAlign_Right"`

**Vertical:**
- `"VAlign_Fill"`, `"VAlign_Top"`, `"VAlign_Center"`, `"VAlign_Bottom"`

---

## Test 6: List Properties with Filters

**Purpose**: Use list_properties with filtering

### Steps

1. **List All Properties**
   ```
   List all properties of PlayButton with include_inherited=true
   ```

2. **Filter by Category**
   ```
   List properties with category_filter="Appearance"
   ```

3. **Compare Widget Types**
   ```
   List properties of Button vs TextBlock to see differences
   ```

### Expected Outcomes
- ✅ list_properties shows available properties
- ✅ include_inherited shows base class properties
- ✅ category_filter narrows results
- ✅ Different widgets have different properties

---

## Test 7: Event Discovery and Binding

**Purpose**: Get available events and bind handlers

### Steps

1. **Get Button Events**
   ```
   Get available events for PlayButton component
   ```

2. **Bind OnClicked Event**
   ```
   Use bind_events to bind OnClicked to "HandlePlayClicked" function
   ```

3. **Bind OnHovered Event**
   ```
   Bind OnHovered event to "HandlePlayHovered"
   ```

4. **Verify Bindings**
   ```
   Check that events are bound (may need to inspect Blueprint)
   ```

### Expected Outcomes
- ✅ get_available_events lists Button events
- ✅ OnClicked, OnPressed, OnReleased available
- ✅ bind_events creates event bindings
- ✅ Multiple events can bind to same component

---

## Test 8: Widget Hierarchy Validation

**Purpose**: Test validate action

### Steps

1. **Validate Clean Widget**
   ```
   Validate WBP_TestWidget hierarchy
   ```

2. **Add Invalid Configuration**
   ```
   Try to create invalid parent-child relationship (if possible)
   ```

3. **Validate Again**
   ```
   Validation should report issues
   ```

4. **Fix Issues**
   ```
   Correct hierarchy problems
   ```

5. **Final Validation**
   ```
   Should pass validation
   ```

### Expected Outcomes
- ✅ validate checks widget hierarchy
- ✅ Reports structural issues
- ✅ Valid widgets pass validation
- ✅ Useful for debugging complex UIs

---

## Test 9: Remove Components

**Purpose**: Test component removal with cleanup options

### Steps

1. **Add Component with Children**
   ```
   Add VerticalBox with several child widgets
   ```

2. **Remove with Children**
   ```
   Remove VerticalBox with remove_children=true
   ```

3. **Verify Cascade Delete**
   ```
   All children should be removed
   ```

4. **Remove Single Component**
   ```
   Remove PlayButton (no children)
   ```

5. **Verify Selective Removal**
   ```
   Only PlayButton removed, others remain
   ```

### Expected Outcomes
- ✅ remove_component deletes widget
- ✅ remove_children=true cascades delete
- ✅ remove_from_variables=true cleans up variables
- ✅ Hierarchy updates correctly

---

## Test 10: Complex Layout Example

**Purpose**: Build complete UI with multiple components

### Steps

1. **Create Base Layout**
   ```
   Add CanvasPanel root, Image background (fill screen)
   ```

2. **Add Menu Container**
   ```
   Add VerticalBox for menu items (centered)
   ```

3. **Add Menu Buttons**
   ```
   Add 3 Button components to VerticalBox
   ```

4. **Add Text Labels**
   ```
   Add TextBlock to each button
   ```

5. **Style Components**
   ```
   Set colors, sizes, alignments for professional look
   ```

6. **Validate Final Widget**
   ```
   Validate complete hierarchy
   ```

### Expected Outcomes
- ✅ Multi-level hierarchy created
- ✅ Background fills screen
- ✅ Buttons properly contained
- ✅ Text labels attached to buttons
- ✅ Validation passes
- ✅ Widget ready for use

---

## Test 11: Property Value Formats

**Purpose**: Test different property value types

### Steps

1. **Test Color Values**
   ```
   Set ColorAndOpacity with [R, G, B, A] where values are 0.0-1.0
   Example: [0.2, 0.6, 1.0, 1.0] for blue
   ```

2. **Test Padding Values**
   ```
   Set Padding with {"Left": 10, "Top": 10, "Right": 10, "Bottom": 10}
   ```

3. **Test Visibility Values**
   ```
   Set Visibility to "Visible", "Hidden", "Collapsed", or "HitTestInvisible"
   ```

4. **Test Font Values**
   ```
   Set Font with {"Size": 24, "TypefaceFontName": "Bold"}
   ```

### Expected Outcomes
- ✅ Colors accept RGBA arrays
- ✅ Padding accepts dict with edge values
- ✅ Visibility accepts enum strings
- ✅ Fonts accept structured objects

---

## Modern UI Color Reference

### Professional Color Palettes
```python
MODERN_COLORS = {
    "primary_blue": [0.2, 0.6, 1.0, 1.0],
    "dark_background": [0.08, 0.08, 0.08, 1.0],
    "text_light": [0.95, 0.95, 0.95, 1.0],
    "success_green": [0.3, 0.69, 0.31, 1.0],
    "warning_orange": [1.0, 0.6, 0.0, 1.0],
    "error_red": [0.9, 0.2, 0.2, 1.0]
}
```

---

## Reference: All Actions Summary

| Action | Purpose | Key Parameters |
|--------|---------|----------------|
| **list_components** | List widget components | widget_name |
| **add_component** | Add component | widget_name, component_type, component_name, parent_name, properties |
| **remove_component** | Remove component | widget_name, component_name, remove_children, remove_from_variables |
| **validate** | Validate hierarchy | widget_name |
| **search_types** | Find widget types | category, search_text, include_custom, include_engine |
| **get_component_properties** | Get all properties | widget_name, component_name |
| **get_property** | Get single property | widget_name, component_name, property_name |
| **set_property** | Set property | widget_name, component_name, property_name, property_value |
| **list_properties** | List available properties | widget_name, component_name, include_inherited, category_filter |
| **get_available_events** | Get component events | widget_name, component_name |
| **bind_events** | Bind event handlers | widget_name, component_name, input_events dict |

---

## Cleanup: Delete Test Assets

**Run these commands AFTER completing all tests:**

```
Delete test Widget Blueprint:
- Delete /Game/Blueprints/WBP_TestWidget with force_delete=True and show_confirmation=False
```

---

**Test Coverage**: 11/11 actions tested ✅  
**Last Updated**: November 4, 2025  
**Related Issues**: #69, #75


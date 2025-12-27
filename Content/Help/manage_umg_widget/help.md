# manage_umg_widget

Manage UMG (Unreal Motion Graphics) Widget Blueprint components - add, configure, and style UI components inside Widget Blueprints.

## Getting Action-Specific Help

For detailed help on any action, use the `help_action` parameter:
```python
# Get help for a specific action
manage_umg_widget(action="help", help_action="set_property")
manage_umg_widget(action="help", help_action="add_component")
manage_umg_widget(action="help", help_action="bind_events")
```

**Available help_action values:** list_components, add_component, remove_component, validate, search_types, get_component_properties, get_property, set_property, list_properties, get_available_events, bind_events

## Summary

The `manage_umg_widget` tool provides comprehensive management of UMG Widget components INSIDE existing Widget Blueprints. It allows you to add and configure UI components, bind events, and manage widget properties.

## Creating Widget Blueprints

**Use `manage_blueprint` with `parent_class="UserWidget"` to create Widget Blueprints:**
```python
# Step 1: CREATE the Widget Blueprint
manage_blueprint(action="create", name="/Game/UI/WBP_MainMenu", parent_class="UserWidget")

# Step 2: Add components using manage_umg_widget  
manage_umg_widget(action="add_component", widget_name="/Game/UI/WBP_MainMenu",
                  component_type="CanvasPanel", component_name="RootCanvas")
```

## Actions

| Action | Description |
|--------|-------------|
| list_components | List all components in a Widget Blueprint |
| add_component | Add a widget component to a Widget Blueprint |
| remove_component | Remove a widget component from a Widget Blueprint |
| validate | Validate widget structure and bindings |
| search_types | Search for available widget types |
| get_component_properties | Get properties of a widget component (**requires BOTH widget_name AND component_name**) |
| get_property | Get a specific property value |
| set_property | Set a property value on a widget component |
| list_properties | List all properties for a widget component |
| get_available_events | Get events available for binding |
| bind_events | Bind events to functions |

## Usage Examples

### List Components in Existing Widget
```python
manage_umg_widget(action="list_components", widget_name="/Game/UI/WBP_MainMenu")
```

### Add Button Widget
```python
manage_umg_widget(action="add_component", widget_name="/Game/UI/WBP_MainMenu",
                  component_type="Button", component_name="PlayButton")
```

### Set Widget Property
```python
manage_umg_widget(action="set_property", widget_name="/Game/UI/WBP_MainMenu",
                  component_name="TitleText", property_name="Text", property_value="My Game")
```

### Get Component Properties (BOTH params required!)
```python
# ✅ CORRECT - Both widget_name AND component_name
manage_umg_widget(action="get_component_properties", widget_name="/Game/UI/WBP_MainMenu",
                  component_name="TitleText")

# ❌ WRONG - Missing component_name
manage_umg_widget(action="get_component_properties", widget_name="/Game/UI/WBP_MainMenu")
```

## Empty Widget Blueprints

**A freshly created UserWidget blueprint has NO components at all - not even a root!**

When `list_components` returns `[]` (empty array):
- The widget has zero components
- There is NO "RootWidget", "RootCanvas", or any implicit root
- **DO NOT** try to find or query a non-existent root
- **DO NOT** call `get_component_properties` for components that don't exist

**Correct workflow for empty widgets:**
```python
# Step 1: Check components
result = manage_umg_widget(action="list_components", widget_name="/Game/UI/MyWidget")
# Result: {"components": [], "count": 0}  ← EMPTY!

# Step 2: Accept it's empty, add a root container
manage_umg_widget(action="add_component", widget_name="/Game/UI/MyWidget", 
                  component_type="CanvasPanel", component_name="RootCanvas")

# Step 3: Now add children to the root
manage_umg_widget(action="add_component", widget_name="/Game/UI/MyWidget",
                  component_type="Button", component_name="PlayButton", parent_name="RootCanvas")
```

# Fill the whole screen (anchors 0,0 to 1,1)
manage_umg_widget(action="set_property", widget_name="/Game/UI/MyWidget",
                  component_name="Background", property_name="Slot.anchors", 
                  property_value="fill")  # Or {min_x:0, min_y:0, max_x:1, max_y:1}

# Set position and size
manage_umg_widget(action="set_property", ..., property_name="Slot.position", property_value=[100, 200])
manage_umg_widget(action="set_property", ..., property_name="Slot.size", property_value=[300, 150])
```

**For Box/Overlay panels, use these slot properties:**
```python
manage_umg_widget(action="set_property", ..., property_name="Slot.horizontal_alignment", property_value="Center")
manage_umg_widget(action="set_property", ..., property_name="Slot.vertical_alignment", property_value="Fill")
```

## UMG Color Property Formats (FLinearColor)

**UMG widgets use FLinearColor with 0.0-1.0 normalized values (NOT 0-255!):**

```python
# ✅ CORRECT - UMG ColorAndOpacity (use dict with 0-1 normalized values)
manage_umg_widget(action="set_property", widget_name="MyWidget", 
                 component_name="MyText", property_name="ColorAndOpacity",
                 property_value={"R": 1.0, "G": 0.5, "B": 0.0, "A": 1.0})  # Orange

# ✅ CORRECT - Array format also works (0-1 normalized)
manage_umg_widget(action="set_property", widget_name="MyWidget",
                 component_name="MyText", property_name="ColorAndOpacity",
                 property_value=[0.0, 1.0, 1.0, 1.0])  # Cyan

# ❌ WRONG - String format doesn't work for UMG
property_value="(R=1.0,G=0.5,B=0.0,A=1.0)"  # WRONG!

# ❌ WRONG - Using 0-255 byte values instead of 0-1 normalized
property_value={"R": 255, "G": 128, "B": 0, "A": 255}  # WRONG! Use 0-1 range
```

## UMG Alignment Enum Values

```python
# ✅ CORRECT horizontal alignment values
"HAlign_Fill", "HAlign_Left", "HAlign_Center", "HAlign_Right"

# ✅ CORRECT vertical alignment values  
"VAlign_Fill", "VAlign_Top", "VAlign_Center", "VAlign_Bottom"

# ❌ WRONG - Missing prefix
"Fill", "Center", "Left"  # WRONG! Use HAlign_/VAlign_ prefix
```

## Button Events - Use component_name Parameter

When querying events for a specific component (like a Button), always pass `component_name`:
```python
# Correct - returns Button-specific events like OnClicked, OnHovered
manage_umg_widget(action="get_available_events", widget_name="...", component_name="PlayButton")

# Then bind the events
manage_umg_widget(action="bind_events", widget_name="...", component_name="PlayButton", 
                  input_events={"OnClicked": "HandlePlayButtonClicked"})
```

**Note:** Event delegate properties like `OnClicked`, `OnHovered` cannot be set via `set_property`. Use `bind_events` action instead.

## Notes

- Widget Blueprints define reusable UI layouts
- Components are arranged in a hierarchy
- Use canvas panels for absolute positioning
- Use other panels (Vertical/Horizontal Box, Grid) for automatic layout

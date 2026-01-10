# WidgetService API Reference

All methods are called via `unreal.WidgetService.<method_name>(...)`.

**ALWAYS use `discover_python_class("unreal.WidgetService")` for parameter details before calling.**

---

## IMPORTANT: Creating Widget Blueprints

WidgetService does **NOT** have a `create_widget()` method. Widget Blueprints must be created using **BlueprintService**:

```python
import unreal

# Create Widget Blueprint using BlueprintService (NOT WidgetService)
widget_path = unreal.BlueprintService.create_blueprint(
    "MainMenu",       # Name
    "UserWidget",     # Parent class - this makes it a Widget Blueprint
    "/Game/UI/"       # Destination folder
)

# Verify it's a widget
is_widget = unreal.BlueprintService.is_widget_blueprint(widget_path)
print(f"Is Widget: {is_widget}")  # True

# Now you can use WidgetService to add components
unreal.WidgetService.add_component(widget_path, "CanvasPanel", "RootCanvas", "", True)
```

---

## Discovery Methods

### list_widgets(path_filter="")
List all Widget Blueprints in the project.

**Returns:** Array[WidgetInfo] with properties: `.name`, `.path`, `.parent_class`, `.component_count`

**Example:**
```python
import unreal

# List all widgets
widgets = unreal.WidgetService.list_widgets()
for w in widgets:
    print(f"{w.name}: {w.component_count} components")
    print(f"  Path: {w.path}")

# Filter by path
ui_widgets = unreal.WidgetService.list_widgets("/Game/UI")
```

### search_types(search_filter="", max_results=50)
Search for available widget component types (Button, Text, Image, etc.).

**NOTE:** This method is `search_types`, NOT `search_widget_types`.

**Returns:** Array[WidgetTypeInfo] with properties: `.name`, `.category`, `.description`

**Example:**
```python
import unreal

# Search for button types
buttons = unreal.WidgetService.search_types("Button")
for b in buttons:
    print(f"{b.name} ({b.category})")
    print(f"  {b.description}")

# Search for panel types
panels = unreal.WidgetService.search_types("Panel")
```

---

## Widget Info

### get_info(widget_path)
Get detailed Widget Blueprint information.

**Returns:** WidgetDetailedInfo or None with properties: `.name`, `.path`, `.parent_class`, `.root_widget`, `.component_count`, `.components_json`

**Example:**
```python
import unreal

info = unreal.WidgetService.get_info("/Game/UI/WBP_MainMenu")
if info:
    print(f"Widget: {info.name}")
    print(f"Root: {info.root_widget}")
    print(f"Components: {info.component_count}")
    print(f"Hierarchy: {info.components_json}")
```

### get_hierarchy(widget_path)
Get widget component hierarchy as formatted text.

**Returns:** str (hierarchy tree)

**Example:**
```python
import unreal

hierarchy = unreal.WidgetService.get_hierarchy("/Game/UI/WBP_MainMenu")
print(hierarchy)
# Output:
# CanvasPanel "RootCanvas"
#   ├─ Button "PlayButton"
#   │   └─ TextBlock "PlayText"
#   └─ Button "QuitButton"
#       └─ TextBlock "QuitText"
```

### list_components(widget_path)
List all components in a widget.

**Returns:** Array[WidgetComponentInfo] with properties: `.name`, `.type`, `.parent_name`, `.slot_index`

**Example:**
```python
import unreal

components = unreal.WidgetService.list_components("/Game/UI/WBP_MainMenu")
for comp in components:
    print(f"{comp.name}: {comp.type}")
    if comp.parent_name:
        print(f"  Parent: {comp.parent_name}")
```

---

## Component Management

### add_component(widget_path, component_type, component_name, parent_name="", set_as_root=False)
Add a widget component to the Widget Blueprint.

**Returns:** bool

**Example:**
```python
import unreal

widget_path = "/Game/UI/WBP_Menu"

# Add root canvas panel
unreal.WidgetService.add_component(
    widget_path,
    "CanvasPanel",
    "RootCanvas",
    "",          # No parent (root)
    True         # Set as root
)

# Add button as child
unreal.WidgetService.add_component(
    widget_path,
    "Button",
    "PlayButton",
    "RootCanvas",  # Parent
    False
)

# Add text to button
unreal.WidgetService.add_component(
    widget_path,
    "TextBlock",
    "PlayText",
    "PlayButton",
    False
)

# Save
unreal.EditorAssetLibrary.save_asset(widget_path)
```

### remove_component(widget_path, component_name)
Remove a component from the widget.

**Returns:** bool

**Example:**
```python
import unreal

unreal.WidgetService.remove_component("/Game/UI/WBP_Menu", "OldButton")
unreal.EditorAssetLibrary.save_asset("/Game/UI/WBP_Menu")
```

### rename_component(widget_path, old_name, new_name)
Rename a widget component.

**Returns:** bool

**Example:**
```python
import unreal

unreal.WidgetService.rename_component(
    "/Game/UI/WBP_Menu",
    "Button_1",
    "PlayButton"
)
```

### reparent_component(widget_path, component_name, new_parent_name)
Move a component to a different parent in the hierarchy.

**Returns:** bool

**Example:**
```python
import unreal

# Move button from one panel to another
unreal.WidgetService.reparent_component(
    "/Game/UI/WBP_Menu",
    "PlayButton",
    "BottomPanel"  # New parent
)
```

---

## Component Properties

### get_property(widget_path, component_name, property_name)
Get a component property value.

**Returns:** str or None

**Example:**
```python
import unreal

# Get button text
text = unreal.WidgetService.get_property(
    "/Game/UI/WBP_Menu",
    "PlayButton",
    "Text"
)
print(f"Button text: {text}")

# Get visibility
visibility = unreal.WidgetService.get_property(
    "/Game/UI/WBP_Menu",
    "PlayButton",
    "Visibility"
)
```

### set_property(widget_path, component_name, property_name, value)
Set a component property value.

**Returns:** bool

**Example:**
```python
import unreal

widget_path = "/Game/UI/WBP_Menu"

# Set text
unreal.WidgetService.set_property(
    widget_path,
    "PlayText",
    "Text",
    "PLAY GAME"
)

# Set color
unreal.WidgetService.set_property(
    widget_path,
    "PlayText",
    "ColorAndOpacity",
    "(R=1.0,G=1.0,B=1.0,A=1.0)"
)

# Set font size
unreal.WidgetService.set_property(
    widget_path,
    "PlayText",
    "Font.Size",
    "24"
)

# Set position (Canvas Panel slot)
unreal.WidgetService.set_property(
    widget_path,
    "PlayButton",
    "Position X",
    "100"
)

unreal.WidgetService.set_property(
    widget_path,
    "PlayButton",
    "Position Y",
    "200"
)

# Set size
unreal.WidgetService.set_property(
    widget_path,
    "PlayButton",
    "Size X",
    "200"
)

unreal.WidgetService.set_property(
    widget_path,
    "PlayButton",
    "Size Y",
    "50"
)

# Set visibility
unreal.WidgetService.set_property(
    widget_path,
    "PlayButton",
    "Visibility",
    "Visible"  # Or: Hidden, Collapsed, HitTestInvisible
)

# Save
unreal.EditorAssetLibrary.save_asset(widget_path)
```

### list_properties(widget_path, component_name)
List all available properties for a component.

**Returns:** Array[PropertyInfo] with properties: `.name`, `.type`, `.category`

**Example:**
```python
import unreal

props = unreal.WidgetService.list_properties("/Game/UI/WBP_Menu", "PlayButton")
for prop in props:
    print(f"{prop.name}: {prop.type}")
    print(f"  Category: {prop.category}")
```

---

## Slot Properties (Layout)

Slot properties control how a widget is positioned within its parent container.

### Canvas Panel Slots
```python
import unreal

widget_path = "/Game/UI/WBP_Menu"

# Position
unreal.WidgetService.set_property(widget_path, "PlayButton", "Position X", "100")
unreal.WidgetService.set_property(widget_path, "PlayButton", "Position Y", "200")

# Size
unreal.WidgetService.set_property(widget_path, "PlayButton", "Size X", "200")
unreal.WidgetService.set_property(widget_path, "PlayButton", "Size Y", "50")

# Anchors (0-1 range)
unreal.WidgetService.set_property(widget_path, "PlayButton", "Anchor Min X", "0.5")
unreal.WidgetService.set_property(widget_path, "PlayButton", "Anchor Min Y", "0.5")
unreal.WidgetService.set_property(widget_path, "PlayButton", "Anchor Max X", "0.5")
unreal.WidgetService.set_property(widget_path, "PlayButton", "Anchor Max Y", "0.5")

# Alignment (0-1 range, pivot point)
unreal.WidgetService.set_property(widget_path, "PlayButton", "Alignment X", "0.5")
unreal.WidgetService.set_property(widget_path, "PlayButton", "Alignment Y", "0.5")

# Z-Order
unreal.WidgetService.set_property(widget_path, "PlayButton", "ZOrder", "1")
```

### Vertical/Horizontal Box Slots
```python
import unreal

widget_path = "/Game/UI/WBP_Menu"

# Padding
unreal.WidgetService.set_property(widget_path, "PlayButton", "Padding Left", "10")
unreal.WidgetService.set_property(widget_path, "PlayButton", "Padding Top", "5")
unreal.WidgetService.set_property(widget_path, "PlayButton", "Padding Right", "10")
unreal.WidgetService.set_property(widget_path, "PlayButton", "Padding Bottom", "5")

# Horizontal Alignment
unreal.WidgetService.set_property(widget_path, "PlayButton", "Horizontal Alignment", "Center")  # Left, Center, Right, Fill

# Vertical Alignment
unreal.WidgetService.set_property(widget_path, "PlayButton", "Vertical Alignment", "Center")  # Top, Center, Bottom, Fill

# Size (for Fill)
unreal.WidgetService.set_property(widget_path, "PlayButton", "Size", "1.0")  # Weight for Fill
```

---

## Event Binding

### bind_event(widget_path, component_name, event_name, function_name)
Bind a widget event to a Blueprint function.

**Returns:** bool

**Example:**
```python
import unreal

# First, create the function in the widget's event graph
unreal.BlueprintService.create_function("/Game/UI/WBP_Menu", "OnPlayClicked", is_pure=False)

# Bind button click event to function
unreal.WidgetService.bind_event(
    "/Game/UI/WBP_Menu",
    "PlayButton",
    "OnClicked",      # Event name
    "OnPlayClicked"   # Function name
)

# Other common events:
# - OnClicked (Button)
# - OnPressed (Button)
# - OnReleased (Button)
# - OnHovered (Button)
# - OnUnhovered (Button)
# - OnTextChanged (EditableText)
# - OnTextCommitted (EditableText)
# - OnCheckStateChanged (CheckBox)
# - OnSelectionChanged (ComboBox)
```

### unbind_event(widget_path, component_name, event_name)
Unbind an event from a component.

**Returns:** bool

**Example:**
```python
import unreal

unreal.WidgetService.unbind_event(
    "/Game/UI/WBP_Menu",
    "PlayButton",
    "OnClicked"
)
```

### list_events(widget_path, component_name)
List all available events for a component type.

**Returns:** Array[str] (event names)

**Example:**
```python
import unreal

events = unreal.WidgetService.list_events("/Game/UI/WBP_Menu", "PlayButton")
print(f"Available events: {events}")
# Output: ['OnClicked', 'OnPressed', 'OnReleased', 'OnHovered', 'OnUnhovered']
```

---

## Common Widget Types

### Button
```python
import unreal

widget_path = "/Game/UI/WBP_Menu"

# Add button
unreal.WidgetService.add_component(widget_path, "Button", "PlayButton", "RootCanvas", False)

# Style
unreal.WidgetService.set_property(widget_path, "PlayButton", "Background Color", "(R=0.0,G=0.5,B=1.0,A=1.0)")
unreal.WidgetService.set_property(widget_path, "PlayButton", "Hovered Color", "(R=0.2,G=0.7,B=1.0,A=1.0)")
```

### TextBlock
```python
import unreal

# Add text
unreal.WidgetService.add_component(widget_path, "TextBlock", "TitleText", "RootCanvas", False)

# Properties
unreal.WidgetService.set_property(widget_path, "TitleText", "Text", "MAIN MENU")
unreal.WidgetService.set_property(widget_path, "TitleText", "Font.Size", "48")
unreal.WidgetService.set_property(widget_path, "TitleText", "ColorAndOpacity", "(R=1.0,G=1.0,B=1.0,A=1.0)")
unreal.WidgetService.set_property(widget_path, "TitleText", "Justification", "Center")  # Left, Center, Right
```

### Image
```python
import unreal

# Add image
unreal.WidgetService.add_component(widget_path, "Image", "BackgroundImage", "RootCanvas", False)

# Properties
unreal.WidgetService.set_property(widget_path, "BackgroundImage", "Brush.Image", "/Game/Textures/T_Background.T_Background")
unreal.WidgetService.set_property(widget_path, "BackgroundImage", "ColorAndOpacity", "(R=1.0,G=1.0,B=1.0,A=0.5)")
```

### ProgressBar
```python
import unreal

# Add progress bar
unreal.WidgetService.add_component(widget_path, "ProgressBar", "HealthBar", "RootCanvas", False)

# Properties
unreal.WidgetService.set_property(widget_path, "HealthBar", "Percent", "0.75")  # 0-1
unreal.WidgetService.set_property(widget_path, "HealthBar", "FillColorAndOpacity", "(R=0.0,G=1.0,B=0.0,A=1.0)")
```

### EditableText
```python
import unreal

# Add text input
unreal.WidgetService.add_component(widget_path, "EditableText", "NameInput", "RootCanvas", False)

# Properties
unreal.WidgetService.set_property(widget_path, "NameInput", "HintText", "Enter your name...")
unreal.WidgetService.set_property(widget_path, "NameInput", "Font.Size", "18")
```

---

## Container Panels

### Canvas Panel (Absolute positioning)
```python
import unreal

unreal.WidgetService.add_component(widget_path, "CanvasPanel", "RootCanvas", "", True)
```

### Vertical Box (Stack vertically)
```python
import unreal

unreal.WidgetService.add_component(widget_path, "VerticalBox", "MenuList", "RootCanvas", False)
```

### Horizontal Box (Stack horizontally)
```python
import unreal

unreal.WidgetService.add_component(widget_path, "HorizontalBox", "ButtonRow", "RootCanvas", False)
```

### Overlay (Stack on top of each other)
```python
import unreal

unreal.WidgetService.add_component(widget_path, "Overlay", "LayeredContent", "RootCanvas", False)
```

### Grid Panel (Grid layout)
```python
import unreal

unreal.WidgetService.add_component(widget_path, "GridPanel", "InventoryGrid", "RootCanvas", False)
```

---

## Critical Rules

### Always Save After Modifications
```python
import unreal

# Modify widget
unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)

# MUST save
unreal.EditorAssetLibrary.save_asset(path)
```

### Set Root Component First
```python
import unreal

# FIRST: Add root panel
unreal.WidgetService.add_component(path, "CanvasPanel", "RootCanvas", "", True)

# THEN: Add children
unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)
```

### Parent Must Exist Before Adding Children
```python
import unreal

# CORRECT order:
unreal.WidgetService.add_component(path, "CanvasPanel", "RootCanvas", "", True)
unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)  # Parent exists

# WRONG order:
unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)  # RootCanvas doesn't exist yet!
```

### Property Values Are Strings
```python
# WRONG:
unreal.WidgetService.set_property(path, "Text", "Font.Size", 24)

# CORRECT:
unreal.WidgetService.set_property(path, "Text", "Font.Size", "24")
```

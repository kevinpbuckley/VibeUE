# UMG Widget Development Guide

## Overview

Complete guide for building and styling UMG widgets with proper hierarchy, performance, and visual consistency.

## CRITICAL: Always Call This First

**Before any UMG work, understand the foundation:**
- Widget/Panel/Slot relationships
- Container-specific background patterns
- Proper component hierarchy
- Layout and sizing strategies

## Widget Structure Best Practices

### Discovery First Workflow

```python
# 1. Find the widget
widgets = search_items(search_term="Inventory", asset_type="Widget")
widget_path = widgets["items"][0]["package_path"]

# 2. Understand current structure
info = get_widget_blueprint_info(widget_path)

# 3. List all components
components = list_widget_components(widget_path)

# 4. Then make targeted changes
set_widget_property(widget_path, "Title_Text", "ColorAndOpacity", [0, 1, 1, 1])
```

### DO's ✅

**Work with Existing Components First:**
- ✅ Discover structure using `list_widget_components()` first
- ✅ Style existing components before adding new ones
- ✅ Use `set_widget_property()` for colors, fonts, visibility
- ✅ Understand hierarchy before changes
- ✅ Check if functionality exists before duplicating

**Component Addition Guidelines:**
- ✅ Add new components ONLY when necessary
- ✅ Justify each component - "Does this serve a purpose?"
- ✅ Prefer modifying existing TextBlocks over creating new
- ✅ Prefer existing containers over new panels
- ✅ Add backgrounds only when essential

**Proper Background Implementation:**
- ✅ Use Overlay widgets for layered backgrounds in Canvas panels
- ✅ Add background images as direct children to ScrollBox/Border
- ✅ Set image sizes to "Fill" for full container backgrounds
- ✅ Use proper parent-child relationships
- ✅ Set Z-ordering (-10 backgrounds, -5 borders, 0+ content)

### DON'Ts ❌

**Avoid Over-Engineering:**
- ❌ Don't create new containers if existing ones work
- ❌ Don't duplicate TextBlocks for similar content
- ❌ Don't add unnecessary layers
- ❌ Don't create nested Overlays unless required
- ❌ Don't add components without clear purpose

**Background Implementation Mistakes:**
- ❌ Don't add background images directly to Canvas panels
- ❌ Don't create backgrounds as root elements
- ❌ Don't use multiple backgrounds when one suffices
- ❌ Don't ignore Z-ordering for layering
- ❌ Don't use Overlay for simple single-background scenarios

## Container-Specific Patterns

### Canvas Panel + Overlay Pattern

**Use for**: Layered backgrounds with absolute positioning

```
SizeBox_Root
└── CanvasPanel_Main
    └── Overlay_Background ⚠️ Required for Canvas
        ├── MainBackground_Image (Fill, Z: -10)
        ├── BorderOverlay_Image (Fill, Z: -5)
        └── Content panels (Z: 0+)
```

### ScrollBox Pattern

**Use for**: Scrollable content with background

```
ScrollBox_Content
├── SectionBackground_Image (Fill, Z: -5) ✅ Direct child OK
└── Content items...
```

### Border Pattern

**Use for**: Single element with styled border/background

```
Border_Container
└── Content (automatic Fill behavior)
```

### Vertical/Horizontal Box Pattern

**Use for**: Linear layouts (backgrounds via parent container)

```
SizeBox_Root
├── Background_Image (Fill, Z: -10)
└── VerticalBox_Content (Z: 0)
    ├── Item1
    └── Item2
```

## Component Discovery

### Get Available Widget Types

```python
# See all widget types with categories
widgets = get_available_widgets()

# Filter by category
panels = get_available_widgets(category="Panel")

# Check compatibility
compatible = get_available_widgets(parent_compatibility="CanvasPanel")
```

### Universal Widget Creation

```python
# Add any widget type generically
add_widget_component(
    widget_name="WBP_Inventory",
    component_type="Button",  # From get_available_widgets()
    component_name="CloseButton",
    parent_name="ActionPanel",
    properties={"Text": "Close"}
)
```

## Styling Properties

### Common Properties by Component Type

**Text Components:**
- `Text`: Content string
- `ColorAndOpacity`: [R, G, B, A] (0.0-1.0)
- `Font`: {"Size": 16, "TypefaceFontName": "Bold"}
- `Justification`: "Left", "Center", "Right"

**Image Components:**
- `ColorAndOpacity`: Tint color [R, G, B, A]
- `BrushColor`: Background color
- `Brush`: Texture reference

**Button Components:**
- `Style`: Button style asset
- `ColorAndOpacity`: Overall tint
- `BackgroundColor`: Button background

**Panel Components:**
- `BrushColor`: Panel background
- `Padding`: [Left, Top, Right, Bottom]
- `BackgroundColor`: Fill color

**All Components:**
- `Visibility`: "Visible", "Hidden", "Collapsed", "HitTestInvisible"
- `IsEnabled`: true/false
- `ToolTipText`: Tooltip string

### Property Management

```python
# List available properties
props = list_widget_properties("WBP_UI", "Button_Start")

# Get specific property
value = get_widget_property("WBP_UI", "Button_Start", "ColorAndOpacity")

# Set property
set_widget_property(
    "WBP_UI",
    "Button_Start",
    "ColorAndOpacity",
    [1.0, 0.5, 0.0, 1.0]
)
```

## Compilation

**ALWAYS compile after widget changes:**

```python
compile_blueprint("WBP_Inventory")
```

Required for:
- Property changes to take effect
- New components to appear
- Layout updates to apply
- Style changes to render

## Component Hierarchy Validation

```python
# Validate widget structure
result = validate_widget_hierarchy("WBP_Inventory")

# Check for:
# - Orphaned components
# - Invalid parent-child relationships
# - Missing required components
# - Circular dependencies
```

## Modern UI Color Examples

```python
MODERN_COLORS = {
    "primary_blue": [0.2, 0.6, 1.0, 1.0],
    "dark_background": [0.08, 0.08, 0.08, 1.0],
    "text_light": [0.95, 0.95, 0.95, 1.0],
    "success_green": [0.3, 0.69, 0.31, 1.0],
    "neon_cyan": [0.0, 1.0, 1.0, 1.0],
    "neon_magenta": [1.0, 0.0, 1.0, 1.0]
}
```

## Complete Workflow Example

```python
# 1. Find widget
search_items(search_term="MainMenu", asset_type="Widget")

# 2. Get structure
info = get_widget_blueprint_info("/Game/UI/WBP_MainMenu")

# 3. List components
components = list_widget_components("/Game/UI/WBP_MainMenu")

# 4. Style existing components
set_widget_property("/Game/UI/WBP_MainMenu", "Title_Text", "ColorAndOpacity", [0, 1, 1, 1])
set_widget_property("/Game/UI/WBP_MainMenu", "Title_Text", "Font", {"Size": 48})

# 5. Add new components only if needed
add_widget_component(
    "/Game/UI/WBP_MainMenu",
    "Image",
    "Background_Image",
    "Overlay_Background",
    properties={"ColorAndOpacity": [0.1, 0.0, 0.2, 0.9]}
)

# 6. Compile to apply changes
compile_blueprint("WBP_MainMenu")

# 7. Validate
validate_widget_hierarchy("/Game/UI/WBP_MainMenu")
```

## Pro Tips

1. **Start Simple**: Modify existing before adding new
2. **Use Search**: Always `search_items()` before modifying
3. **Check Compatibility**: Use `get_available_widgets(parent_compatibility="...")`
4. **Z-Ordering**: Backgrounds at -10, borders at -5, content at 0+
5. **Compile Often**: See changes immediately
6. **Validate**: Check hierarchy after complex changes
7. **Package Paths**: Use package_path from search, not duplicated object paths

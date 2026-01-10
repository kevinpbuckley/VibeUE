---
name: umg-widgets
display_name: UMG Widget Blueprints
description: Create and modify Widget Blueprints for user interface elements
services:
  - WidgetService
keywords:
  - widget
  - umg
  - ui
  - user interface
  - WBP_
  - button
  - text
  - canvas
  - panel
auto_load_keywords:
  - WBP_
  - widget blueprint
  - widget
  - umg
  - user interface
  - ui widget
---

# UMG Widget Blueprints

This skill provides comprehensive documentation for working with Widget Blueprints (UMG) in Unreal Engine using the WidgetService.

## What's Included

- **WidgetService API**: Create Widget Blueprints, manage components, properties, and events
- **Workflows**: Common patterns for UI creation

## When to Use This Skill

Load this skill when working with:
- Widget Blueprints (WBP_* prefix)
- UI components (Button, Text, Image, Canvas Panel, etc.)
- Widget hierarchy and parenting
- Widget properties (visibility, position, color, etc.)
- Widget event binding

## Core Services

### WidgetService
Widget Blueprint manipulation (for **existing** widgets only):
- List Widget Blueprints
- Get widget hierarchy
- Add/remove widget components
- Get/set component properties
- Bind events to functions
- Search available widget types

**NOTE:** To CREATE a Widget Blueprint, use `BlueprintService.create_blueprint("Name", "UserWidget", "/Game/Path/")`

## Quick Examples

### Create a New Widget Blueprint

**IMPORTANT:** WidgetService does NOT have a `create_widget()` method. Widget Blueprints are created using BlueprintService with "UserWidget" as the parent class:

```python
import unreal

# Create Widget Blueprint using BlueprintService
widget_path = unreal.BlueprintService.create_blueprint(
    "MainMenu",       # Name (WBP_ prefix NOT added automatically)
    "UserWidget",     # Parent class for Widget Blueprints
    "/Game/UI/"       # Destination folder
)
print(f"Created: {widget_path}")  # /Game/UI/MainMenu

# Verify it's a widget blueprint
is_widget = unreal.BlueprintService.is_widget_blueprint(widget_path)
print(f"Is Widget Blueprint: {is_widget}")  # True
```

### Add Components to Widget
```python
import unreal

# Widget Blueprint must already exist
widget_path = "/Game/UI/WBP_MainMenu"

# Add canvas panel as root
unreal.WidgetService.add_component(widget_path, "CanvasPanel", "RootCanvas", "", True)

# Add button as child
unreal.WidgetService.add_component(widget_path, "Button", "PlayButton", "RootCanvas", True)

# Set button properties
unreal.WidgetService.set_property(widget_path, "PlayButton", "Position X", "100")
unreal.WidgetService.set_property(widget_path, "PlayButton", "Position Y", "200")
```

### Get Widget Hierarchy
```python
import unreal

hierarchy = unreal.WidgetService.get_hierarchy("/Game/UI/WBP_MainMenu")
print(hierarchy)
```

## Related Skills

- **asset-management**: For finding Widget Blueprints
- **blueprints**: For widget logic and event handling

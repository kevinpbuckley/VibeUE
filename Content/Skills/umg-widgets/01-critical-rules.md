# UMG Widget Critical Rules

---

## Creating Widget Blueprints

⚠️ **WidgetService does NOT create widgets.** Use BlueprintService:

```python
# CORRECT - Use BlueprintService with "UserWidget" parent
widget_path = unreal.BlueprintService.create_blueprint(
    "MainMenu",       # Name
    "UserWidget",     # Parent class makes it a Widget Blueprint
    "/Game/UI/"       # Folder
)

# WRONG - WidgetService has no create method
widget_path = unreal.WidgetService.create_widget(...)  # ❌ Does not exist
```

---

## Hierarchy Rules

### 1. Set Root First
```python
# FIRST: Add root panel (set_as_root=True)
unreal.WidgetService.add_component(path, "CanvasPanel", "RootCanvas", "", True)

# THEN: Add children
unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)
```

### 2. Parent Must Exist
```python
# WRONG - Parent doesn't exist yet
unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)

# CORRECT - Add parent first
unreal.WidgetService.add_component(path, "CanvasPanel", "RootCanvas", "", True)
unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)
```

---

## Property Values Are ALWAYS Strings

```python
# WRONG
unreal.WidgetService.set_property(path, "Text", "Font.Size", 24)

# CORRECT
unreal.WidgetService.set_property(path, "Text", "Font.Size", "24")
```

---

## Color Format

Colors use Unreal's struct format:
```python
unreal.WidgetService.set_property(path, "Text", "ColorAndOpacity", "(R=1.0,G=0.5,B=0.0,A=1.0)")
```

---

## Common Property Names

| Widget Type | Property | Example Value |
|------------|----------|---------------|
| TextBlock | Text | "Hello World" |
| TextBlock | Font.Size | "24" |
| TextBlock | Justification | "Center" |
| Button | Background Color | "(R=0.0,G=0.5,B=1.0,A=1.0)" |
| Image | ColorAndOpacity | "(R=1.0,G=1.0,B=1.0,A=0.5)" |
| Image | Brush.Image | "/Game/Textures/T_Image" |
| ProgressBar | Percent | "0.75" |
| Any | Visibility | "Visible" / "Hidden" / "Collapsed" |

---

## Canvas Panel Slot Properties

Position and size within a CanvasPanel:
```python
# Position
unreal.WidgetService.set_property(path, "Button", "Position X", "100")
unreal.WidgetService.set_property(path, "Button", "Position Y", "200")

# Size
unreal.WidgetService.set_property(path, "Button", "Size X", "200")
unreal.WidgetService.set_property(path, "Button", "Size Y", "50")

# Anchors (0-1 range, pivot point for responsive layout)
unreal.WidgetService.set_property(path, "Button", "Anchor Min X", "0.5")
unreal.WidgetService.set_property(path, "Button", "Anchor Min Y", "0.5")
unreal.WidgetService.set_property(path, "Button", "Anchor Max X", "0.5")
unreal.WidgetService.set_property(path, "Button", "Anchor Max Y", "0.5")

# Alignment (0-1 range, alignment within anchors)
unreal.WidgetService.set_property(path, "Button", "Alignment X", "0.5")
unreal.WidgetService.set_property(path, "Button", "Alignment Y", "0.5")
```

---

## Box Slot Properties

Padding and alignment within VerticalBox/HorizontalBox:
```python
# Padding
unreal.WidgetService.set_property(path, "Button", "Padding Left", "10")
unreal.WidgetService.set_property(path, "Button", "Padding Top", "5")

# Alignment
unreal.WidgetService.set_property(path, "Button", "Horizontal Alignment", "Center")  # Left, Center, Right, Fill
unreal.WidgetService.set_property(path, "Button", "Vertical Alignment", "Center")    # Top, Center, Bottom, Fill
```

---

## Panel Types

| Type | Purpose |
|------|---------|
| CanvasPanel | Absolute positioning (X, Y coords) |
| VerticalBox | Stack children vertically |
| HorizontalBox | Stack children horizontally |
| Overlay | Stack children on top of each other |
| GridPanel | Grid layout |

---

## Event Names

Common button events:
- `OnClicked` - Button clicked
- `OnPressed` - Button pressed down
- `OnReleased` - Button released
- `OnHovered` - Mouse enter
- `OnUnhovered` - Mouse leave

---

## Always Save After Modifications

```python
# After all changes
unreal.EditorAssetLibrary.save_asset(widget_path)
```

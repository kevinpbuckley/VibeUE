# UMG Widget Workflows

---

## Create Widget Blueprint

```python
import unreal

# Widget Blueprints are created via BlueprintService
widget_path = unreal.BlueprintService.create_blueprint(
    "MainMenu",
    "UserWidget",    # Parent class for widgets
    "/Game/UI/"
)

# Verify it's a widget
is_widget = unreal.BlueprintService.is_widget_blueprint(widget_path)
print(f"Is Widget: {is_widget}")
```

---

## Add Root and Children

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

# 1. Add root panel
unreal.WidgetService.add_component(path, "CanvasPanel", "RootCanvas", "", True)

# 2. Add button as child of root
unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)

# 3. Add text inside button
unreal.WidgetService.add_component(path, "TextBlock", "PlayText", "PlayButton", False)

# 4. Set properties
unreal.WidgetService.set_property(path, "PlayText", "Text", "PLAY")
unreal.WidgetService.set_property(path, "PlayText", "Font.Size", "24")

# 5. Save
unreal.EditorAssetLibrary.save_asset(path)
```

---

## Create Button List with VerticalBox

```python
import unreal

path = "/Game/UI/WBP_Menu"

# Root canvas
unreal.WidgetService.add_component(path, "CanvasPanel", "Root", "", True)

# Vertical box for button list
unreal.WidgetService.add_component(path, "VerticalBox", "ButtonList", "Root", False)
unreal.WidgetService.set_property(path, "ButtonList", "Position X", "760")
unreal.WidgetService.set_property(path, "ButtonList", "Position Y", "400")

# Add buttons
buttons = [("PlayButton", "PLAY"), ("QuitButton", "QUIT")]
for btn_name, btn_text in buttons:
    unreal.WidgetService.add_component(path, "Button", btn_name, "ButtonList", False)
    text_name = f"{btn_name}Text"
    unreal.WidgetService.add_component(path, "TextBlock", text_name, btn_name, False)
    unreal.WidgetService.set_property(path, text_name, "Text", btn_text)
    unreal.WidgetService.set_property(path, text_name, "Font.Size", "32")

unreal.EditorAssetLibrary.save_asset(path)
```

---

## Create Health Bar HUD

```python
import unreal

path = "/Game/UI/WBP_HUD"

# Root overlay
unreal.WidgetService.add_component(path, "Overlay", "Root", "", True)

# Canvas for positioning
unreal.WidgetService.add_component(path, "CanvasPanel", "HUDCanvas", "Root", False)

# Health bar background
unreal.WidgetService.add_component(path, "Image", "HealthBG", "HUDCanvas", False)
unreal.WidgetService.set_property(path, "HealthBG", "Position X", "50")
unreal.WidgetService.set_property(path, "HealthBG", "Position Y", "50")
unreal.WidgetService.set_property(path, "HealthBG", "Size X", "300")
unreal.WidgetService.set_property(path, "HealthBG", "Size Y", "30")
unreal.WidgetService.set_property(path, "HealthBG", "ColorAndOpacity", "(R=0.2,G=0.2,B=0.2,A=0.8)")

# Progress bar
unreal.WidgetService.add_component(path, "ProgressBar", "HealthBar", "HUDCanvas", False)
unreal.WidgetService.set_property(path, "HealthBar", "Position X", "50")
unreal.WidgetService.set_property(path, "HealthBar", "Position Y", "50")
unreal.WidgetService.set_property(path, "HealthBar", "Size X", "300")
unreal.WidgetService.set_property(path, "HealthBar", "Size Y", "30")
unreal.WidgetService.set_property(path, "HealthBar", "Percent", "1.0")
unreal.WidgetService.set_property(path, "HealthBar", "FillColorAndOpacity", "(R=0.0,G=1.0,B=0.0,A=1.0)")

unreal.EditorAssetLibrary.save_asset(path)
```

---

## Bind Button Event

```python
import unreal

path = "/Game/UI/WBP_Menu"

# Create function in widget
unreal.BlueprintService.create_function(path, "OnPlayClicked", is_pure=False)

# Bind button click to function
unreal.WidgetService.bind_event(path, "PlayButton", "OnClicked", "OnPlayClicked")

unreal.EditorAssetLibrary.save_asset(path)
```

---

## Get Widget Info

```python
import unreal

# Get widget details
info = unreal.WidgetService.get_info("/Game/UI/WBP_Menu")
if info:
    print(f"Widget: {info.name}")
    print(f"Root: {info.root_widget}")
    print(f"Components: {info.component_count}")

# Get hierarchy as text
hierarchy = unreal.WidgetService.get_hierarchy("/Game/UI/WBP_Menu")
print(hierarchy)

# List all components
components = unreal.WidgetService.list_components("/Game/UI/WBP_Menu")
for comp in components:
    print(f"{comp.name}: {comp.type}")
```

---

## Search Widget Types

```python
import unreal

# Find button types
buttons = unreal.WidgetService.search_types("Button")
for b in buttons:
    print(f"{b.name} ({b.category})")

# Find panel types
panels = unreal.WidgetService.search_types("Panel")
```

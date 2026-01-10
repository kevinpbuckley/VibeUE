# UMG Widget Workflows

Common patterns for creating and modifying Widget Blueprints.

---

## Create Main Menu Widget

```python
import unreal

widget_path = "/Game/UI/WBP_MainMenu"

# 1. Check if widget exists (assuming created in Unreal Editor)
existing = unreal.AssetDiscoveryService.find_asset_by_path(widget_path)
if not existing:
    print("Widget Blueprint must be created in Unreal Editor first")
else:
    # 2. Add root canvas panel
    unreal.WidgetService.add_component(
        widget_path,
        "CanvasPanel",
        "RootCanvas",
        "",
        True  # Set as root
    )

    # 3. Add background image
    unreal.WidgetService.add_component(
        widget_path,
        "Image",
        "Background",
        "RootCanvas",
        False
    )

    # Set background properties
    unreal.WidgetService.set_property(widget_path, "Background", "Position X", "0")
    unreal.WidgetService.set_property(widget_path, "Background", "Position Y", "0")
    unreal.WidgetService.set_property(widget_path, "Background", "Size X", "1920")
    unreal.WidgetService.set_property(widget_path, "Background", "Size Y", "1080")
    unreal.WidgetService.set_property(widget_path, "Background", "ColorAndOpacity", "(R=0.1,G=0.1,B=0.1,A=1.0)")

    # 4. Add title text
    unreal.WidgetService.add_component(
        widget_path,
        "TextBlock",
        "TitleText",
        "RootCanvas",
        False
    )

    unreal.WidgetService.set_property(widget_path, "TitleText", "Text", "MAIN MENU")
    unreal.WidgetService.set_property(widget_path, "TitleText", "Font.Size", "72")
    unreal.WidgetService.set_property(widget_path, "TitleText", "Position X", "960")
    unreal.WidgetService.set_property(widget_path, "TitleText", "Position Y", "200")
    unreal.WidgetService.set_property(widget_path, "TitleText", "Anchor Min X", "0.5")
    unreal.WidgetService.set_property(widget_path, "TitleText", "Anchor Min Y", "0.0")
    unreal.WidgetService.set_property(widget_path, "TitleText", "Anchor Max X", "0.5")
    unreal.WidgetService.set_property(widget_path, "TitleText", "Anchor Max Y", "0.0")
    unreal.WidgetService.set_property(widget_path, "TitleText", "Alignment X", "0.5")
    unreal.WidgetService.set_property(widget_path, "TitleText", "Alignment Y", "0.5")

    # 5. Add vertical box for buttons
    unreal.WidgetService.add_component(
        widget_path,
        "VerticalBox",
        "ButtonList",
        "RootCanvas",
        False
    )

    unreal.WidgetService.set_property(widget_path, "ButtonList", "Position X", "760")
    unreal.WidgetService.set_property(widget_path, "ButtonList", "Position Y", "400")
    unreal.WidgetService.set_property(widget_path, "ButtonList", "Size X", "400")
    unreal.WidgetService.set_property(widget_path, "ButtonList", "Size Y", "300")

    # 6. Add buttons
    buttons = [
        ("PlayButton", "PLAY GAME"),
        ("OptionsButton", "OPTIONS"),
        ("CreditsButton", "CREDITS"),
        ("QuitButton", "QUIT")
    ]

    for btn_name, btn_text in buttons:
        # Add button
        unreal.WidgetService.add_component(
            widget_path,
            "Button",
            btn_name,
            "ButtonList",
            False
        )

        # Add text to button
        text_name = f"{btn_name}Text"
        unreal.WidgetService.add_component(
            widget_path,
            "TextBlock",
            text_name,
            btn_name,
            False
        )

        # Set text properties
        unreal.WidgetService.set_property(widget_path, text_name, "Text", btn_text)
        unreal.WidgetService.set_property(widget_path, text_name, "Font.Size", "32")
        unreal.WidgetService.set_property(widget_path, text_name, "Justification", "Center")

    # 7. Save
    unreal.EditorAssetLibrary.save_asset(widget_path)

    print(f"Created main menu widget: {widget_path}")
```

---

## Create HUD Widget with Health Bar

```python
import unreal

widget_path = "/Game/UI/WBP_PlayerHUD"

# 1. Add root overlay
unreal.WidgetService.add_component(
    widget_path,
    "Overlay",
    "Root",
    "",
    True
)

# 2. Add canvas for HUD elements
unreal.WidgetService.add_component(
    widget_path,
    "CanvasPanel",
    "HUDCanvas",
    "Root",
    False
)

# 3. Add health bar background
unreal.WidgetService.add_component(
    widget_path,
    "Image",
    "HealthBarBG",
    "HUDCanvas",
    False
)

unreal.WidgetService.set_property(widget_path, "HealthBarBG", "Position X", "20")
unreal.WidgetService.set_property(widget_path, "HealthBarBG", "Position Y", "20")
unreal.WidgetService.set_property(widget_path, "HealthBarBG", "Size X", "300")
unreal.WidgetService.set_property(widget_path, "HealthBarBG", "Size Y", "30")
unreal.WidgetService.set_property(widget_path, "HealthBarBG", "ColorAndOpacity", "(R=0.2,G=0.2,B=0.2,A=0.8)")

# 4. Add health bar
unreal.WidgetService.add_component(
    widget_path,
    "ProgressBar",
    "HealthBar",
    "HUDCanvas",
    False
)

unreal.WidgetService.set_property(widget_path, "HealthBar", "Position X", "22")
unreal.WidgetService.set_property(widget_path, "HealthBar", "Position Y", "22")
unreal.WidgetService.set_property(widget_path, "HealthBar", "Size X", "296")
unreal.WidgetService.set_property(widget_path, "HealthBar", "Size Y", "26")
unreal.WidgetService.set_property(widget_path, "HealthBar", "Percent", "1.0")
unreal.WidgetService.set_property(widget_path, "HealthBar", "FillColorAndOpacity", "(R=0.0,G=1.0,B=0.0,A=1.0)")

# 5. Add health text
unreal.WidgetService.add_component(
    widget_path,
    "TextBlock",
    "HealthText",
    "HUDCanvas",
    False
)

unreal.WidgetService.set_property(widget_path, "HealthText", "Text", "100 / 100")
unreal.WidgetService.set_property(widget_path, "HealthText", "Font.Size", "18")
unreal.WidgetService.set_property(widget_path, "HealthText", "Position X", "170")
unreal.WidgetService.set_property(widget_path, "HealthText", "Position Y", "26")
unreal.WidgetService.set_property(widget_path, "HealthText", "Alignment X", "0.5")

# 6. Add ammo counter (top right)
unreal.WidgetService.add_component(
    widget_path,
    "TextBlock",
    "AmmoText",
    "HUDCanvas",
    False
)

unreal.WidgetService.set_property(widget_path, "AmmoText", "Text", "30 / 120")
unreal.WidgetService.set_property(widget_path, "AmmoText", "Font.Size", "32")
unreal.WidgetService.set_property(widget_path, "AmmoText", "Position X", "1820")
unreal.WidgetService.set_property(widget_path, "AmmoText", "Position Y", "20")
unreal.WidgetService.set_property(widget_path, "AmmoText", "Anchor Min X", "1.0")
unreal.WidgetService.set_property(widget_path, "AmmoText", "Anchor Max X", "1.0")
unreal.WidgetService.set_property(widget_path, "AmmoText", "Alignment X", "1.0")

# 7. Save
unreal.EditorAssetLibrary.save_asset(widget_path)
```

---

## Create Inventory Grid Widget

```python
import unreal

widget_path = "/Game/UI/WBP_Inventory"

# 1. Add root canvas
unreal.WidgetService.add_component(widget_path, "CanvasPanel", "Root", "", True)

# 2. Add background panel
unreal.WidgetService.add_component(widget_path, "Image", "Background", "Root", False)
unreal.WidgetService.set_property(widget_path, "Background", "Size X", "800")
unreal.WidgetService.set_property(widget_path, "Background", "Size Y", "600")
unreal.WidgetService.set_property(widget_path, "Background", "ColorAndOpacity", "(R=0.05,G=0.05,B=0.05,A=0.95)")

# 3. Add title
unreal.WidgetService.add_component(widget_path, "TextBlock", "Title", "Root", False)
unreal.WidgetService.set_property(widget_path, "Title", "Text", "INVENTORY")
unreal.WidgetService.set_property(widget_path, "Title", "Font.Size", "48")
unreal.WidgetService.set_property(widget_path, "Title", "Position X", "400")
unreal.WidgetService.set_property(widget_path, "Title", "Position Y", "30")
unreal.WidgetService.set_property(widget_path, "Title", "Alignment X", "0.5")

# 4. Add uniform grid for items
unreal.WidgetService.add_component(widget_path, "UniformGridPanel", "ItemGrid", "Root", False)
unreal.WidgetService.set_property(widget_path, "ItemGrid", "Position X", "50")
unreal.WidgetService.set_property(widget_path, "ItemGrid", "Position Y", "100")
unreal.WidgetService.set_property(widget_path, "ItemGrid", "Size X", "700")
unreal.WidgetService.set_property(widget_path, "ItemGrid", "Size Y", "420")

# 5. Add close button
unreal.WidgetService.add_component(widget_path, "Button", "CloseButton", "Root", False)
unreal.WidgetService.add_component(widget_path, "TextBlock", "CloseText", "CloseButton", False)

unreal.WidgetService.set_property(widget_path, "CloseButton", "Position X", "300")
unreal.WidgetService.set_property(widget_path, "CloseButton", "Position Y", "540")
unreal.WidgetService.set_property(widget_path, "CloseButton", "Size X", "200")
unreal.WidgetService.set_property(widget_path, "CloseButton", "Size Y", "40")

unreal.WidgetService.set_property(widget_path, "CloseText", "Text", "CLOSE")
unreal.WidgetService.set_property(widget_path, "CloseText", "Font.Size", "24")
unreal.WidgetService.set_property(widget_path, "CloseText", "Justification", "Center")

# 6. Save
unreal.EditorAssetLibrary.save_asset(widget_path)
```

---

## Bind Button Events

```python
import unreal

widget_path = "/Game/UI/WBP_MainMenu"

# 1. Create event handler functions
functions = [
    "OnPlayClicked",
    "OnOptionsClicked",
    "OnCreditsClicked",
    "OnQuitClicked"
]

for func_name in functions:
    unreal.BlueprintService.create_function(widget_path, func_name, is_pure=False)

# 2. Compile blueprint
unreal.BlueprintService.compile_blueprint(widget_path)

# 3. Bind events to functions
button_bindings = [
    ("PlayButton", "OnClicked", "OnPlayClicked"),
    ("OptionsButton", "OnClicked", "OnOptionsClicked"),
    ("CreditsButton", "OnClicked", "OnCreditsClicked"),
    ("QuitButton", "OnClicked", "OnQuitClicked")
]

for button_name, event_name, function_name in button_bindings:
    success = unreal.WidgetService.bind_event(
        widget_path,
        button_name,
        event_name,
        function_name
    )
    if success:
        print(f"Bound {button_name}.{event_name} to {function_name}")

# 4. Compile and save
unreal.BlueprintService.compile_blueprint(widget_path)
unreal.EditorAssetLibrary.save_asset(widget_path)
```

---

## Inspect Widget Hierarchy

```python
import unreal

widget_path = "/Game/UI/WBP_MainMenu"

# 1. Get widget info
info = unreal.WidgetService.get_info(widget_path)
if info:
    print(f"Widget: {info.name}")
    print(f"Root: {info.root_widget}")
    print(f"Component count: {info.component_count}")

# 2. Get hierarchy as text
hierarchy = unreal.WidgetService.get_hierarchy(widget_path)
print(f"\nHierarchy:\n{hierarchy}")

# 3. List all components
components = unreal.WidgetService.list_components(widget_path)
print(f"\nComponents:")
for comp in components:
    print(f"  {comp.name} ({comp.type})")
    if comp.parent_name:
        print(f"    Parent: {comp.parent_name}")
```

---

## Update Widget Layout

```python
import unreal

widget_path = "/Game/UI/WBP_MainMenu"

# 1. Get all components
components = unreal.WidgetService.list_components(widget_path)

# 2. Update button positions
button_y = 400
button_spacing = 80

for comp in components:
    if "Button" in comp.name and comp.name != "CloseButton":
        # Update position
        unreal.WidgetService.set_property(widget_path, comp.name, "Position Y", str(button_y))

        button_y += button_spacing

# 3. Save
unreal.EditorAssetLibrary.save_asset(widget_path)
```

---

## Create Responsive Layout with Anchors

```python
import unreal

widget_path = "/Game/UI/WBP_Responsive"

# 1. Add root canvas
unreal.WidgetService.add_component(widget_path, "CanvasPanel", "Root", "", True)

# 2. Add top-left anchored text
unreal.WidgetService.add_component(widget_path, "TextBlock", "TopLeft", "Root", False)
unreal.WidgetService.set_property(widget_path, "TopLeft", "Text", "TOP LEFT")
unreal.WidgetService.set_property(widget_path, "TopLeft", "Position X", "20")
unreal.WidgetService.set_property(widget_path, "TopLeft", "Position Y", "20")
unreal.WidgetService.set_property(widget_path, "TopLeft", "Anchor Min X", "0.0")
unreal.WidgetService.set_property(widget_path, "TopLeft", "Anchor Min Y", "0.0")
unreal.WidgetService.set_property(widget_path, "TopLeft", "Anchor Max X", "0.0")
unreal.WidgetService.set_property(widget_path, "TopLeft", "Anchor Max Y", "0.0")

# 3. Add top-right anchored text
unreal.WidgetService.add_component(widget_path, "TextBlock", "TopRight", "Root", False)
unreal.WidgetService.set_property(widget_path, "TopRight", "Text", "TOP RIGHT")
unreal.WidgetService.set_property(widget_path, "TopRight", "Position X", "-20")
unreal.WidgetService.set_property(widget_path, "TopRight", "Position Y", "20")
unreal.WidgetService.set_property(widget_path, "TopRight", "Anchor Min X", "1.0")
unreal.WidgetService.set_property(widget_path, "TopRight", "Anchor Min Y", "0.0")
unreal.WidgetService.set_property(widget_path, "TopRight", "Anchor Max X", "1.0")
unreal.WidgetService.set_property(widget_path, "TopRight", "Anchor Max Y", "0.0")
unreal.WidgetService.set_property(widget_path, "TopRight", "Alignment X", "1.0")

# 4. Add center text
unreal.WidgetService.add_component(widget_path, "TextBlock", "Center", "Root", False)
unreal.WidgetService.set_property(widget_path, "Center", "Text", "CENTER")
unreal.WidgetService.set_property(widget_path, "Center", "Position X", "0")
unreal.WidgetService.set_property(widget_path, "Center", "Position Y", "0")
unreal.WidgetService.set_property(widget_path, "Center", "Anchor Min X", "0.5")
unreal.WidgetService.set_property(widget_path, "Center", "Anchor Min Y", "0.5")
unreal.WidgetService.set_property(widget_path, "Center", "Anchor Max X", "0.5")
unreal.WidgetService.set_property(widget_path, "Center", "Anchor Max Y", "0.5")
unreal.WidgetService.set_property(widget_path, "Center", "Alignment X", "0.5")
unreal.WidgetService.set_property(widget_path, "Center", "Alignment Y", "0.5")

# 5. Add bottom-center button (stretched horizontally)
unreal.WidgetService.add_component(widget_path, "Button", "BottomButton", "Root", False)
unreal.WidgetService.set_property(widget_path, "BottomButton", "Position X", "100")
unreal.WidgetService.set_property(widget_path, "BottomButton", "Position Y", "-60")
unreal.WidgetService.set_property(widget_path, "BottomButton", "Size X", "-200")
unreal.WidgetService.set_property(widget_path, "BottomButton", "Size Y", "50")
unreal.WidgetService.set_property(widget_path, "BottomButton", "Anchor Min X", "0.0")
unreal.WidgetService.set_property(widget_path, "BottomButton", "Anchor Min Y", "1.0")
unreal.WidgetService.set_property(widget_path, "BottomButton", "Anchor Max X", "1.0")
unreal.WidgetService.set_property(widget_path, "BottomButton", "Anchor Max Y", "1.0")

# 6. Save
unreal.EditorAssetLibrary.save_asset(widget_path)
```

---

## Clone and Modify Widget

```python
import unreal

source_path = "/Game/UI/WBP_Button"
dest_path = "/Game/UI/WBP_ButtonLarge"

# 1. Duplicate widget
unreal.AssetDiscoveryService.duplicate_asset(source_path, dest_path)

# 2. Modify button size
components = unreal.WidgetService.list_components(dest_path)
for comp in components:
    if comp.type == "Button":
        # Increase size
        unreal.WidgetService.set_property(dest_path, comp.name, "Size X", "300")
        unreal.WidgetService.set_property(dest_path, comp.name, "Size Y", "60")

    if comp.type == "TextBlock":
        # Increase font size
        unreal.WidgetService.set_property(dest_path, comp.name, "Font.Size", "36")

# 3. Save
unreal.EditorAssetLibrary.save_asset(dest_path)
```

---

## Common Patterns Summary

### Pattern 1: Root First, Then Children
```python
# Add root
unreal.WidgetService.add_component(path, "CanvasPanel", "Root", "", True)

# Add children
unreal.WidgetService.add_component(path, "Button", "PlayButton", "Root", False)
```

### Pattern 2: Configure After Adding
```python
# Add component
unreal.WidgetService.add_component(path, "TextBlock", "Title", "Root", False)

# Configure properties
unreal.WidgetService.set_property(path, "Title", "Text", "TITLE")
unreal.WidgetService.set_property(path, "Title", "Font.Size", "48")
```

### Pattern 3: Create Functions Before Binding
```python
# Create function
unreal.BlueprintService.create_function(path, "OnClicked", is_pure=False)

# Compile
unreal.BlueprintService.compile_blueprint(path)

# Bind event
unreal.WidgetService.bind_event(path, "Button", "OnClicked", "OnClicked")
```

### Pattern 4: Always Save After Modifications
```python
# Make changes
unreal.WidgetService.add_component(path, "Button", "PlayButton", "Root", False)

# Save
unreal.EditorAssetLibrary.save_asset(path)
```

### Pattern 5: Use Anchors for Responsive UI
```python
# Center-anchored widget
unreal.WidgetService.set_property(path, "Widget", "Anchor Min X", "0.5")
unreal.WidgetService.set_property(path, "Widget", "Anchor Min Y", "0.5")
unreal.WidgetService.set_property(path, "Widget", "Anchor Max X", "0.5")
unreal.WidgetService.set_property(path, "Widget", "Anchor Max Y", "0.5")
unreal.WidgetService.set_property(path, "Widget", "Alignment X", "0.5")
unreal.WidgetService.set_property(path, "Widget", "Alignment Y", "0.5")
```

---
name: umg-widgets
display_name: UMG Widget Blueprints
description: Create and modify Widget Blueprints for user interface elements, including MVVM ViewModel support
vibeue_classes:
  - WidgetService
  - BlueprintService
unreal_classes:
  - EditorAssetLibrary
---

# UMG Widget Blueprints Skill

## Critical Rules

### ⚠️ Creating Widget Blueprints

**NEVER use `BlueprintEditorLibrary` or guess at factory APIs!**

WidgetService does NOT create widgets. **ONLY this pattern works:**

```python
import unreal

factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property("parent_class", unreal.UserWidget)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
widget_asset = asset_tools.create_asset("MainMenu", "/Game/UI", unreal.WidgetBlueprint, factory)
```

**Common hallucinations to AVOID:**
- ❌ `BlueprintEditorLibrary.get_blueprint_class()` - Does NOT exist
- ❌ `unreal.create_widget_blueprint()` - Does NOT exist  
- ❌ `WidgetService.create_widget()` - Does NOT exist

**Use the pattern above. Nothing else.**

### ⚠️ Hierarchy Rules

1. **Set root first** with `set_as_root=True`
2. **Parent must exist** before adding children

```python
# Root first
unreal.WidgetService.add_component(path, "CanvasPanel", "RootCanvas", "", True)
# Then children
unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)
```

### ⚠️ Property Values Are ALWAYS Strings

```python
unreal.WidgetService.set_property(path, "Text", "Font.Size", "24")  # Not 24
```

### ⚠️ WidgetPropertyInfo Field Names

| WRONG | CORRECT |
|-------|---------|
| `p.name` | `p.property_name` |
| `p.type` | `p.property_type` |
| `p.property_value` | `p.current_value` |

### ⚠️ Panel Types

| Type | Purpose |
|------|---------|
| `CanvasPanel` | Absolute positioning (X, Y coords) |
| `VerticalBox` | Stack children vertically |
| `HorizontalBox` | Stack children horizontally |
| `Overlay` | Stack children on top of each other |

### ⚠️ Using Custom Widget Blueprints as Components

You can add an existing Widget Blueprint as a sub-widget inside another WBP. **Always discover first** — use `list_widget_blueprints()` to find available WBPs, then pass the asset name (not the full path) as the `component_type`:

```python
import unreal

# Step 1: discover what custom WBPs exist
all_wbps = unreal.WidgetService.list_widget_blueprints("")
# Returns paths like ["/Game/UI/WBP_HealthBar.WBP_HealthBar", ...]

# Step 2: extract just the asset name to use as component_type
# e.g. "WBP_HealthBar" from "/Game/UI/WBP_HealthBar.WBP_HealthBar"
component_type = "WBP_HealthBar"

# Step 3: add it like any other component
path = "/Game/UI/WBP_HUD"
unreal.WidgetService.add_component(path, component_type, "HealthBarWidget", "RootCanvas", True)
unreal.EditorAssetLibrary.save_asset(path)
```

**Key rules:**
- `search_types()` returns both built-in types and discovered WBPs (prefixed with `[WBP]`)
- Use `list_widget_blueprints("")` to discover custom WBPs by path
- Pass just the asset name (e.g. `"WBP_HealthBar"`), not the full package path
- The custom WBP must already exist before adding it as a component
- **Circular references are rejected** — a WBP cannot contain itself as a child
- The parent WBP is automatically compiled after adding a custom WBP child

---

## Workflows

### Create Widget Blueprint

```python
import unreal

factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property("parent_class", unreal.UserWidget)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
widget_asset = asset_tools.create_asset("MainMenu", "/Game/UI", unreal.WidgetBlueprint, factory)
```

### Add Root and Children

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

unreal.WidgetService.add_component(path, "CanvasPanel", "RootCanvas", "", True)
unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)
unreal.WidgetService.add_component(path, "TextBlock", "PlayText", "PlayButton", False)

unreal.WidgetService.set_property(path, "PlayText", "Text", "PLAY")
unreal.WidgetService.set_property(path, "PlayText", "Font.Size", "24")
unreal.EditorAssetLibrary.save_asset(path)
```

### Canvas Panel Positioning

```python
unreal.WidgetService.set_property(path, "Button", "Position X", "100")
unreal.WidgetService.set_property(path, "Button", "Position Y", "200")
unreal.WidgetService.set_property(path, "Button", "Size X", "200")
unreal.WidgetService.set_property(path, "Button", "Size Y", "50")
unreal.WidgetService.set_property(path, "Button", "Anchor Min X", "0.5")
unreal.WidgetService.set_property(path, "Button", "Anchor Min Y", "0.5")
```

### Create Button List with VerticalBox

```python
import unreal

path = "/Game/UI/WBP_Menu"

unreal.WidgetService.add_component(path, "CanvasPanel", "Root", "", True)
unreal.WidgetService.add_component(path, "VerticalBox", "ButtonList", "Root", False)

buttons = [("PlayButton", "PLAY"), ("QuitButton", "QUIT")]
for btn_name, btn_text in buttons:
    unreal.WidgetService.add_component(path, "Button", btn_name, "ButtonList", False)
    text_name = f"{btn_name}Text"
    unreal.WidgetService.add_component(path, "TextBlock", text_name, btn_name, False)
    unreal.WidgetService.set_property(path, text_name, "Text", btn_text)

unreal.EditorAssetLibrary.save_asset(path)
```

### Bind Button Event

```python
import unreal

unreal.BlueprintService.create_function(path, "OnPlayClicked", is_pure=False)
unreal.WidgetService.bind_event(path, "PlayButton", "OnClicked", "OnPlayClicked")
unreal.EditorAssetLibrary.save_asset(path)
```

### Get Widget Hierarchy

```python
import unreal

hierarchy = unreal.WidgetService.get_hierarchy("/Game/UI/WBP_Menu")
for widget in hierarchy:
    print(f"{widget.widget_name} ({widget.widget_class})")
    print(f"  Parent: {widget.parent_widget}, Is Root: {widget.is_root_widget}")
```

---

## Common Properties

| Widget | Property | Example |
|--------|----------|---------|
| TextBlock | Text | "Hello World" |
| TextBlock | Font.Size | "24" |
| Button | Background Color | "(R=0.0,G=0.5,B=1.0,A=1.0)" |
| Image | ColorAndOpacity | "(R=1.0,G=1.0,B=1.0,A=0.5)" |
| ProgressBar | Percent | "0.75" |
| Any | Visibility | "Visible" / "Hidden" / "Collapsed" |

## Event Names

- `OnClicked` - Button clicked
- `OnPressed` / `OnReleased` - Press/release
- `OnHovered` / `OnUnhovered` - Mouse enter/leave

---

## MVVM ViewModel Support

### ⚠️ ViewModel Rules

1. **The ModelViewViewModel plugin must be enabled** in the project (VibeUE enables it automatically)
2. **ViewModel classes must inherit from `UMVVMViewModelBase`** or implement `INotifyFieldValueChanged`
3. **Add the ViewModel before creating bindings** — `add_view_model` must be called before `add_view_model_binding`
4. **Widget must exist before binding** — the target widget component must already be in the hierarchy
5. **Property names must match exactly** — use `list_properties` to discover available properties

### ⚠️ ViewModel CreationType Options

| Type | Purpose |
|------|---------|
| `CreateInstance` | Widget creates the ViewModel instance automatically (default, most common) |
| `Manual` | ViewModel is set manually at runtime via code |
| `GlobalViewModelCollection` | Shared ViewModel from the global collection |
| `PropertyPath` | ViewModel obtained from a property path on the widget |
| `Resolver` | Custom resolver class determines the ViewModel |

### ⚠️ Binding Mode Options

| Mode | Purpose |
|------|---------|
| `OneWayToDestination` | ViewModel → Widget (default, most common for display) |
| `TwoWay` | ViewModel ↔ Widget (for editable inputs like sliders, text boxes) |
| `OneTimeToDestination` | ViewModel → Widget (once on init, no updates) |
| `OneWayToSource` | Widget → ViewModel (widget drives ViewModel) |
| `OneTimeToSource` | Widget → ViewModel (once on init) |

### ⚠️ FWidgetViewModelInfo Field Names

| Field | Description |
|-------|-------------|
| `view_model_name` | Property name/alias of the ViewModel |
| `view_model_class_name` | Class name of the ViewModel |
| `creation_type` | How the ViewModel is created |
| `view_model_id` | GUID identifier |

### ⚠️ FWidgetViewModelBindingInfo Field Names

| Field | Description |
|-------|-------------|
| `binding_index` | Index for use with remove_view_model_binding |
| `source_path` | ViewModel property path |
| `destination_path` | Widget property path |
| `binding_mode` | Binding direction |
| `b_enabled` | Whether binding is active |
| `binding_id` | GUID identifier |

### Workflow: Add ViewModel to Widget Blueprint

```python
import unreal

path = "/Game/UI/WBP_HUD"

# Add a ViewModel (class must exist and inherit UMVVMViewModelBase)
unreal.WidgetService.add_view_model(path, "MyHealthViewModel", "HealthVM", "CreateInstance")

# List ViewModels to verify
vms = unreal.WidgetService.list_view_models(path)
for vm in vms:
    print(f"{vm.view_model_name} ({vm.view_model_class_name}) - {vm.creation_type}")
```

### Workflow: Bind ViewModel Property to Widget

```python
import unreal

path = "/Game/UI/WBP_HUD"

# Ensure ViewModel and widget exist first
# Then create a binding: HealthVM.CurrentHealth → HealthBar.Percent
unreal.WidgetService.add_view_model_binding(
    path,
    "HealthVM",          # ViewModel name (as registered)
    "CurrentHealth",     # Property on the ViewModel
    "HealthBar",         # Widget component name
    "Percent",           # Property on the widget
    "OneWayToDestination"  # Binding mode
)

# List bindings to verify
bindings = unreal.WidgetService.list_view_model_bindings(path)
for b in bindings:
    print(f"[{b.binding_index}] {b.source_path} -> {b.destination_path} ({b.binding_mode})")
```

### Workflow: Full MVVM HUD Setup

```python
import unreal

# Step 1: Create the Widget Blueprint
factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property("parent_class", unreal.UserWidget)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
widget_asset = asset_tools.create_asset("WBP_GameHUD", "/Game/UI", unreal.WidgetBlueprint, factory)

path = "/Game/UI/WBP_GameHUD"

# Step 2: Build the widget hierarchy
unreal.WidgetService.add_component(path, "CanvasPanel", "RootCanvas", "", True)
unreal.WidgetService.add_component(path, "ProgressBar", "HealthBar", "RootCanvas", True)
unreal.WidgetService.add_component(path, "TextBlock", "HealthText", "RootCanvas", True)

# Step 3: Add the ViewModel
unreal.WidgetService.add_view_model(path, "GameHUDViewModel", "HudVM")

# Step 4: Create bindings
unreal.WidgetService.add_view_model_binding(path, "HudVM", "HealthPercent", "HealthBar", "Percent", "OneWayToDestination")
unreal.WidgetService.add_view_model_binding(path, "HudVM", "HealthDisplayText", "HealthText", "Text", "OneWayToDestination")

# Step 5: Save
unreal.EditorAssetLibrary.save_asset(path)
```

### Workflow: Remove ViewModel and Bindings

```python
import unreal

path = "/Game/UI/WBP_HUD"

# Remove a specific binding by index
unreal.WidgetService.remove_view_model_binding(path, 0)

# Remove a ViewModel entirely (also invalidates its bindings)
unreal.WidgetService.remove_view_model(path, "HealthVM")
```

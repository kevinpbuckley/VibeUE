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

### ⚠️ Prefer Dedicated Style APIs For Full Edits

Use the dedicated service methods when changing a complete font or brush configuration. They keep related fields together and are easier to verify than a long sequence of nested `set_property` calls.

```python
import unreal

font_info = unreal.WidgetFontInfo()
font_info.size = 28
font_info.typeface = "Bold"
font_info.color = "(R=1.0,G=0.85,B=0.2,A=1.0)"
unreal.WidgetService.set_font(path, "HeaderTitle", font_info)

brush_info = unreal.WidgetBrushInfo()
brush_info.draw_as = "RoundedBox"
brush_info.tint_color = "(R=0.08,G=0.12,B=0.2,A=1.0)"
brush_info.corner_radius = "(TopLeft=12.0,TopRight=12.0,BottomRight=12.0,BottomLeft=12.0)"
unreal.WidgetService.set_brush(path, "BackgroundImage", brush_info)
```

Use `set_property` for single leaf values or slot aliases like `Position X`, `Size Y`, or `Anchor Min X`.

### ⚠️ Animation Tracks Require Real Property Paths

`add_animation_track` and `add_keyframe` work on actual widget properties or supported slot aliases.

Good animation targets:
- `RenderOpacity`
- `ColorAndOpacity`
- `Position X`
- `Position Y`
- `Size X`
- `Size Y`

Always create the animation first, then add the track, then add keyframes.

### ⚠️ Preview And PIE Have Different Purposes

- `capture_preview` renders an editor-side PNG without starting gameplay
- `start_pie` and `spawn_widget_in_pie` are for runtime verification against a live widget instance

Use preview first for appearance checks, then PIE only when you need runtime state or live property reads.

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

### Apply Full Font Styling

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

font_info = unreal.WidgetFontInfo()
font_info.font_family = "/Engine/EngineFonts/Roboto"
font_info.typeface = "Bold"
font_info.size = 30
font_info.letter_spacing = 20
font_info.color = "(R=1.0,G=0.9,B=0.25,A=1.0)"
font_info.shadow_offset = "(X=2.0,Y=2.0)"
font_info.shadow_color = "(R=0.0,G=0.0,B=0.0,A=0.75)"

unreal.WidgetService.set_font(path, "HeaderTitle", font_info)

applied = unreal.WidgetService.get_font(path, "HeaderTitle")
print(applied.size, applied.typeface, applied.color)
```

### Apply Full Brush Styling

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

brush_info = unreal.WidgetBrushInfo()
brush_info.resource_path = "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"
brush_info.tint_color = "(R=0.08,G=0.12,B=0.2,A=1.0)"
brush_info.draw_as = "RoundedBox"
brush_info.image_size = "(X=1920.0,Y=1080.0)"
brush_info.margin = "(Left=0.2,Top=0.2,Right=0.2,Bottom=0.2)"
brush_info.corner_radius = "(TopLeft=16.0,TopRight=16.0,BottomRight=16.0,BottomLeft=16.0)"

unreal.WidgetService.set_brush(path, "BackgroundImage", brush_info)

applied = unreal.WidgetService.get_brush(path, "BackgroundImage")
print(applied.draw_as, applied.tint_color, applied.corner_radius)
```

### Create Widget Animation

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

unreal.WidgetService.create_animation(path, "IntroFade")
unreal.WidgetService.add_animation_track(path, "IntroFade", "HeaderTitle", "RenderOpacity")

key_a = unreal.WidgetAnimKeyframe()
key_a.time = 0.0
key_a.value = "0.0"
key_a.interpolation = "Linear"

key_b = unreal.WidgetAnimKeyframe()
key_b.time = 0.35
key_b.value = "1.0"
key_b.interpolation = "Linear"

unreal.WidgetService.add_keyframe(path, "IntroFade", "HeaderTitle", "RenderOpacity", key_a)
unreal.WidgetService.add_keyframe(path, "IntroFade", "HeaderTitle", "RenderOpacity", key_b)

for anim in unreal.WidgetService.list_animations(path):
    print(anim.animation_name, anim.duration, anim.track_count)
```

### Capture Preview Render

```python
import unreal

result = unreal.WidgetService.capture_preview(
    "/Game/UI/WBP_MainMenu",
    unreal.Paths.project_saved_dir() + "WidgetPreviews/WBP_MainMenu.png",
    1280,
    720,
)

print(result.b_success, result.output_path, result.error_message)
```

### Spawn And Inspect Widget In PIE

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

if not unreal.WidgetService.is_pie_running():
    unreal.WidgetService.start_pie()

handle = unreal.WidgetService.spawn_widget_in_pie(path)
print(handle.b_valid, handle.instance_id, handle.error_message)

if handle.b_valid:
    live_text = unreal.WidgetService.get_live_property(handle.instance_id, "HeaderTitle", "RenderOpacity")
    print(live_text)
    unreal.WidgetService.remove_widget_from_pie(handle.instance_id)

unreal.WidgetService.stop_pie()
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

`bind_event` takes four arguments: `widget_path`, `widget_name`, `event_name`, `function_name`.

```python
import unreal

unreal.BlueprintService.create_function(path, "OnPlayClicked", is_pure=False)
unreal.WidgetService.bind_event(path, "PlayButton", "OnClicked", "OnPlayClicked")
unreal.EditorAssetLibrary.save_asset(path)
```

### Rename a Widget Component

```python
import unreal

path = "/Game/UI/WBP_Menu"
unreal.WidgetService.rename_widget(path, "ItemButton", "Item1_ActionButton")
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

### Get Full Widget Snapshot (preferred for UI inspection)

Use `get_widget_snapshot` instead of `get_hierarchy` + per-widget `list_properties` calls.
Returns hierarchy order with slot layout and all properties in a single call.

```python
import unreal

path = "/Game/UI/WBP_MainMenu"
snapshots = unreal.WidgetService.get_widget_snapshot(path)

for s in snapshots:
    print(f"{s.widget_name} ({s.widget_class})")
    print(f"  Parent: {s.parent_widget}, Root: {s.is_root_widget}, Children: {s.children}")

    slot = s.slot_info
    print(f"  SlotType: {slot.slot_type}")
    if slot.slot_type == "Canvas":
        print(f"    Anchors: {slot.anchor_min} -> {slot.anchor_max}")
        print(f"    Offsets: {slot.offsets}, Alignment: {slot.alignment}")
        print(f"    ZOrder: {slot.z_order}, AutoSize: {slot.auto_size}")
    elif slot.slot_type in ("VerticalBox", "HorizontalBox"):
        print(f"    Size: {slot.size_rule} ({slot.size_value})")
        print(f"    Padding: {slot.padding}, HAlign: {slot.horizontal_alignment}, VAlign: {slot.vertical_alignment}")
    elif slot.slot_type == "Overlay":
        print(f"    Padding: {slot.padding}, HAlign: {slot.horizontal_alignment}, VAlign: {slot.vertical_alignment}")

    for prop in s.properties:
        if prop.is_editable:
            print(f"  {prop.property_name} = {prop.current_value}")
```

### Get Single Component Snapshot

```python
import unreal

snapshot = unreal.WidgetService.get_component_snapshot("/Game/UI/WBP_MainMenu", "PlayButton")
print(f"Class: {snapshot.widget_class}, Parent: {snapshot.parent_widget}")
slot = snapshot.slot_info
print(f"SlotType: {slot.slot_type}")
for prop in snapshot.properties:
    print(f"  {prop.property_name} ({prop.property_type}) = {prop.current_value}")
```

### FWidgetSlotInfo Field Names

| Field | Type | Description |
|-------|------|-------------|
| `slot_type` | str | "Canvas", "VerticalBox", "HorizontalBox", "Overlay", "None" |
| `anchor_min` | Vector2D | Canvas: anchor minimum |
| `anchor_max` | Vector2D | Canvas: anchor maximum |
| `offsets` | Margin | Canvas: position+size or margins |
| `alignment` | Vector2D | Canvas: pivot alignment |
| `z_order` | int | Canvas: Z-order |
| `auto_size` | bool | Canvas: auto-size to content |
| `size_rule` | str | Box: "Fill" or "Automatic" |
| `size_value` | float | Box: fill coefficient |
| `padding` | Margin | Box/Overlay: slot padding |
| `horizontal_alignment` | EHorizontalAlignment | Box/Overlay: horizontal alignment |
| `vertical_alignment` | EVerticalAlignment | Box/Overlay: vertical alignment |

### FWidgetInfo Field Names

Returned by `list_components()` and `get_hierarchy()`.

| Field | Type | Description |
|-------|------|-------------|
| `widget_name` | str | Component name — **NOT** `component_name` or `name` |
| `widget_class` | str | UE class name (e.g. "TextBlock") |
| `parent_widget` | str | Parent component name (empty for root) |
| `is_root_widget` | bool | True if this is the root |
| `is_variable` | bool | Exposed as Blueprint variable |
| `children` | list[str] | Child component names |

### FWidgetComponentSnapshot Field Names

Returned by `get_widget_snapshot()` and `get_component_snapshot()`. Has all `FWidgetInfo` fields plus:

| Field | Type | Description |
|-------|------|-------------|
| `widget_name` | str | Component name |
| `widget_class` | str | UE class name (e.g. "TextBlock") |
| `parent_widget` | str | Parent component name (empty for root) |
| `is_root_widget` | bool | True if this is the root |
| `is_variable` | bool | Exposed as Blueprint variable |
| `children` | list[str] | Child component names |
| `slot_info` | FWidgetSlotInfo | Slot layout data |
| `properties` | list[FWidgetPropertyInfo] | All widget properties |

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

## FWidgetFontInfo Field Names

| Field | Type | Description |
|-------|------|-------------|
| `font_family` | str | Font asset object path or family name when available |
| `typeface` | str | Typeface name such as `Regular` or `Bold` |
| `size` | int | Font size |
| `letter_spacing` | int | Letter spacing/tracking |
| `color` | str | Text color as linear color text |
| `shadow_offset` | str | Shadow offset as vector text |
| `shadow_color` | str | Shadow color as linear color text |
| `outline_size` | int | Outline thickness |
| `outline_color` | str | Outline color as linear color text |

## FWidgetBrushInfo Field Names

| Field | Type | Description |
|-------|------|-------------|
| `resource_path` | str | Texture or material object path |
| `tint_color` | str | Brush tint as linear color text |
| `draw_as` | str | `Image`, `Box`, `Border`, or `RoundedBox` |
| `image_size` | str | Image size as vector text |
| `margin` | str | Brush margin as margin text |
| `corner_radius` | str | Rounded-box radii text |

## FWidgetAnimInfo Field Names

| Field | Type | Description |
|-------|------|-------------|
| `animation_name` | str | Animation asset name inside the widget blueprint |
| `duration` | float | Playback duration in seconds |
| `track_count` | int | Number of possessable property tracks |

## FWidgetPreviewResult Field Names

| Field | Type | Description |
|-------|------|-------------|
| `b_success` | bool | True when the preview PNG was written |
| `output_path` | str | Saved image path |
| `width` | int | Render width |
| `height` | int | Render height |
| `error_message` | str | Failure message when `b_success` is false |

## FPIEWidgetHandle Field Names

| Field | Type | Description |
|-------|------|-------------|
| `b_valid` | bool | True when the PIE widget instance was spawned |
| `instance_id` | str | Stable ID used by `get_live_property` and `remove_widget_from_pie` |
| `error_message` | str | Failure message when spawn fails |

## WidgetService Actions

| Action | Description |
|--------|-------------|
| `add_component` | Add a widget to the hierarchy |
| `remove_component` | Remove a widget and optionally its children |
| `rename_widget` | Rename a widget component (`old_name`, `new_name`) |
| `bind_event` | Bind a widget event to a function (`widget_name`, `event_name`, `function_name`) |
| `get_hierarchy` | Get all widgets as `FWidgetInfo` list |
| `list_components` | Get all widgets as `FWidgetInfo` list |
| `get_widget_snapshot` | Get full hierarchy + slot + properties as `FWidgetComponentSnapshot` list |
| `search_types` | Search available widget type names |
| `set_property` / `get_property` | Read/write a single widget property |
| `list_properties` | All editable properties for a widget |
| `get_available_events` | List valid event names for a widget |
| `set_font` / `get_font` | Full font configuration on a text widget |
| `set_brush` / `get_brush` | Full brush configuration on an image widget |
| `create_animation` / `add_animation_track` / `add_keyframe` | Widget animations |
| `capture_preview` | Render a PNG without starting PIE |
| `start_pie` / `stop_pie` / `spawn_widget_in_pie` | Runtime PIE testing |

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

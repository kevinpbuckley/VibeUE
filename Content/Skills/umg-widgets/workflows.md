---
name: umg-widgets/workflows
description: Step-by-step UMG widget task workflows — create a WBP, build a menu, position on canvas, inspect, style fonts/brushes, animate, bind events, edit the hierarchy, capture previews, and run PIE checks.
---

# UMG Widget Workflows

## Contents
- Create Widget Blueprint
- Build a Menu
- Canvas Positioning
- Inspect a Widget
- Font Styling
- Brush Styling
- Widget Animation
- Bind Event
- Edit the Hierarchy (rename / reparent / remove)
- Capture Preview
- PIE Runtime Check

Each workflow has a matching runnable example under `scripts/` where noted. Field names for the return
objects used below are in `reference.md`.

## Create Widget Blueprint

```python
import unreal

factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property("parent_class", unreal.UserWidget)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_tools.create_asset("MainMenu", "/Game/UI", unreal.WidgetBlueprint, factory)
```

Runnable: `scripts/create_widget.txt`.

## Build a Menu

Root canvas → vertical box → buttons, each with a child text label.

```python
import unreal

path = "/Game/UI/WBP_Menu"

unreal.WidgetService.add_component(path, "CanvasPanel", "Root", "", True)
unreal.WidgetService.add_component(path, "VerticalBox", "ButtonList", "Root", False)

for btn_name, btn_text in [("PlayButton", "PLAY"), ("QuitButton", "QUIT")]:
    unreal.WidgetService.add_component(path, "Button", btn_name, "ButtonList", False)
    text_name = f"{btn_name}Text"
    unreal.WidgetService.add_component(path, "TextBlock", text_name, btn_name, False)
    unreal.WidgetService.set_property(path, text_name, "Text", btn_text)

unreal.EditorAssetLibrary.save_asset(path)
```

Runnable: `scripts/build_menu.txt`.

## Canvas Positioning

Position/size/anchor aliases are set via `set_property` (values are strings):

```python
unreal.WidgetService.set_property(path, "Button", "Position X", "100")
unreal.WidgetService.set_property(path, "Button", "Position Y", "200")
unreal.WidgetService.set_property(path, "Button", "Size X", "200")
unreal.WidgetService.set_property(path, "Button", "Size Y", "50")
unreal.WidgetService.set_property(path, "Button", "Anchor Min X", "0.5")
unreal.WidgetService.set_property(path, "Button", "Anchor Min Y", "0.5")
```

To stretch a widget to fill its canvas, set anchors `Min (0,0)` → `Max (1,1)`. To **center** it, set
anchors to `0.5` and `Alignment X/Y` to `0.5` (the alignment is the pivot — without it the widget's
top-left sits on the anchor point):

```python
for k, v in [("Anchor Min X","0.5"),("Anchor Min Y","0.5"),("Anchor Max X","0.5"),("Anchor Max Y","0.5"),
             ("Alignment X","0.5"),("Alignment Y","0.5")]:
    unreal.WidgetService.set_property(path, "MenuStack", k, v)
```

`set_property` also writes `ZOrder` on canvas children and `Horizontal Alignment` / `Vertical Alignment`
/ `Padding` / `Size Rule` / `Size Value` on box/overlay children (alignment accepts friendly values like
`Fill`, `Top`, `Center`). See SKILL.md → "Slot editing via set_property" for the full alias list.

## Inspect a Widget

Prefer `get_widget_snapshot` — it returns hierarchy order + slot layout + all properties in one call
(do not stitch `get_hierarchy` + per-widget `list_properties`).

```python
import unreal

path = "/Game/UI/WBP_MainMenu"
snapshots = unreal.WidgetService.get_widget_snapshot(path)

for s in snapshots:
    print(f"{s.widget_name} ({s.widget_class}) parent={s.parent_widget} root={s.is_root_widget}")
    slot = s.slot_info
    print(f"  slot={slot.slot_type}")
    for prop in s.properties:
        if prop.is_editable:
            print(f"  {prop.property_name} = {prop.current_value}")
```

For a single widget use `get_component_snapshot(path, "PlayButton")`. For names-only hierarchy use
`get_hierarchy(path)` (returns `FWidgetInfo`). Runnable: `scripts/inspect_hierarchy.txt`.

## Font Styling

Use the dedicated font API for full edits, then read it back to verify.

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

Runnable: `scripts/apply_font.txt`. Field names: `reference.md` → FWidgetFontInfo.

## Brush Styling

`set_brush`/`get_brush` take a **`slot_name`** argument:
`set_brush(widget_path, component_name, slot_name, brush_info)`. For an Image the slot is `"Brush"`;
other widgets expose different brush slot names.

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
unreal.WidgetService.set_brush(path, "BackgroundImage", "Brush", brush_info)

applied = unreal.WidgetService.get_brush(path, "BackgroundImage", "Brush")
print(applied.draw_as, applied.tint_color, applied.corner_radius)
```

Runnable: `scripts/apply_brush.txt`. Field names: `reference.md` → FWidgetBrushInfo.

## Widget Animation

Create the animation, add a track on a real property, then add keyframes.

```python
import unreal

path = "/Game/UI/WBP_MainMenu"

unreal.WidgetService.create_animation(path, "IntroFade")
unreal.WidgetService.add_animation_track(path, "IntroFade", "HeaderTitle", "RenderOpacity")

for t, v in [(0.0, "0.0"), (0.35, "1.0")]:
    key = unreal.WidgetAnimKeyframe()
    key.time = t
    key.value = v
    key.interpolation = "Linear"
    unreal.WidgetService.add_keyframe(path, "IntroFade", "HeaderTitle", "RenderOpacity", key)

for anim in unreal.WidgetService.list_animations(path):
    print(anim.animation_name, anim.duration, anim.track_count)
```

Runnable: `scripts/create_animation.txt`.

## Bind Event

`bind_event(widget_path, widget_name, event_name, function_name)`. Create the function first.
The engine `BlueprintTools` toolset owns Blueprint function creation — call it via `call_tool`:

```python
# Step 1: create the function graph (engine BlueprintTools, via call_tool)
call_tool(
    tool_name="add_function_graph",
    toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
    arguments={"blueprint": path, "graph_name": "OnPlayClicked"},
)

# Step 2: bind the event to that function (WidgetService keeps this)
import unreal
unreal.WidgetService.bind_event(path, "PlayButton", "OnClicked", "OnPlayClicked")
unreal.EditorAssetLibrary.save_asset(path)
```

Common events: `OnClicked`, `OnPressed`, `OnReleased`, `OnHovered`, `OnUnhovered`. Use
`get_available_events(path, widget_name)` to discover valid events for a widget.

## Edit the Hierarchy (rename / reparent / remove)

```python
import unreal

path = "/Game/UI/WBP_Menu"

# Rename (GUID preserved)
unreal.WidgetService.rename_widget(path, "ItemButton", "Item1_ActionButton")

# Remove a widget (optionally its children)
unreal.WidgetService.remove_component(path, "Item3")

unreal.EditorAssetLibrary.save_asset(path)
```

Move a widget to a new parent with `reparent_widget(path, widget_name, new_parent_name)` — it preserves
the widget object/GUID (rejects moving a panel into itself/a descendant; root can't be reparented).

After any structural edit, re-read with `get_widget_snapshot(path)` to confirm before saving.

## Capture Preview

`capture_preview(widget_path, width=1920, height=1080)` takes **no output path** — it writes to
`<project>/Saved/WidgetPreviews/<WidgetName>.png` and returns the path in `result.output_path`.
The success field is `success` (not `b_success`).

```python
import unreal

result = unreal.WidgetService.capture_preview("/Game/UI/WBP_MainMenu", 1280, 720)
print(result.success, result.output_path, result.error_message)
```

Runnable: `scripts/capture_preview.txt`.

## PIE Runtime Check

`start_pie()` is **asynchronous** — PIE is not ready in the same call. Start PIE in one step, then
spawn/read in a later step. `get_live_property` and `remove_widget_from_pie` take the **handle object**
(not `handle.instance_id`), and the handle's validity field is `valid` (not `b_valid`).

```python
import unreal
ws = unreal.WidgetService
path = "/Game/UI/WBP_MainMenu"

# Step 1 (its own call):
if not ws.is_pie_running():
    ws.start_pie()

# Step 2 (after PIE is live):
handle = ws.spawn_widget_in_pie(path)
print(handle.valid, handle.instance_id, handle.error_message)
if handle.valid:
    live = ws.get_live_property(handle, "HeaderTitle", "RenderOpacity")
    print(live)
    ws.remove_widget_from_pie(handle)
ws.stop_pie()
```

Runnable: `scripts/pie_inspect.txt`.

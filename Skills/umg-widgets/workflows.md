---
name: umg-widgets/workflows
description: Step-by-step UMG widget task workflows — create a WBP, build a menu, position on canvas, inspect with DescribeWidget, style fonts/brushes with ObjectToolset, animate with WidgetAnimationToolset + SequencerScripting, bind events, edit the hierarchy, screenshot the open designer, and run PIE checks.
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
- Capture Preview (designer screenshot)
- PIE Runtime Check
- Compile and Save

Each workflow has a matching runnable example under `scripts/` where noted. Field names for the
return objects used below are in `reference.md`.

Two call styles are in play throughout — `unreal.WidgetService.*` for the 9 survivors,
`vibeue.exec_tool(toolset, tool, args)` (or `call_tool`) for Epic's toolsets. SKILL.md ▸ "Call
styles" explains which is which and why `unreal.ObjectToolset.*` does not exist.

## Create Widget Blueprint

```python
import unreal

factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property("parent_class", unreal.UserWidget)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_tools.create_asset("MainMenu", "/Game/UI", unreal.WidgetBlueprint, factory)
```

Nothing in either surface creates the asset — the factory is the only route. Runnable:
`scripts/create_widget.txt`.

## Build a Menu

Root canvas → vertical box → buttons, each with a child text label. The hierarchy is
`WidgetService`; the label text is `ObjectToolset`.

```python
import json, unreal, vibeue

path = "/Game/UI/WBP_Menu"
OT = "EditorToolset.ObjectToolset"

unreal.WidgetService.add_component(path, "CanvasPanel", "Root", "", True)
unreal.WidgetService.add_component(path, "VerticalBox", "ButtonList", "Root", False)

for btn_name, btn_text in [("PlayButton", "PLAY"), ("QuitButton", "QUIT")]:
    unreal.WidgetService.add_component(path, "Button", btn_name, "ButtonList", False)
    text_name = f"{btn_name}Text"
    unreal.WidgetService.add_component(path, "TextBlock", text_name, btn_name, False)
    vibeue.exec_tool(OT, "SetProperties", {
        "instance": f"{path}.WBP_Menu:WidgetTree.{text_name}",
        "values": json.dumps({"Text": btn_text}),
    })

unreal.EditorAssetLibrary.save_asset(path)
```

> The `instance` path is `<package>.<Asset>:WidgetTree.<WidgetName>` — the **editable** tree. Never
> `<Asset>_C:WidgetTree...`, which compile rebuilds.

Runnable: `scripts/build_menu.txt`.

## Canvas Positioning

Slot layout lives on the widget's `Slot` object. The simplest route is Python's typed setters:

```python
import unreal

w = unreal.load_object(None, "/Game/UI/WBP_Menu.WBP_Menu:WidgetTree.MenuStack")
slot = w.slot                                            # UCanvasPanelSlot
slot.set_position(unreal.Vector2D(100.0, 200.0))
slot.set_size(unreal.Vector2D(200.0, 50.0))
slot.set_z_order(5)
```

To **stretch** a widget to fill its canvas, set anchors `Min (0,0)` → `Max (1,1)`. To **centre** it,
set anchors to `0.5` *and* `Alignment` to `0.5` — the alignment is the pivot; without it the
widget's top-left sits on the anchor point:

```python
slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0.5, 0.5),
                                maximum=unreal.Vector2D(0.5, 0.5)))
slot.set_alignment(unreal.Vector2D(0.5, 0.5))
```

VibeUE's helper does anchors + offsets + alignment in one call, then compiles and saves:

```python
import vibeue
print(vibeue.set_canvas_slot("/Game/UI/WBP_Menu", "MenuStack",
                             anchor_min=(0.5, 0.5), anchor_max=(0.5, 0.5),
                             alignment=(0.5, 0.5), offsets=(-150, -100, 300, 200)))
```

Box/Overlay children use `Padding`, `HorizontalAlignment`, `VerticalAlignment`, and on box slots
`Size` (`SizeRule` + `Value`) — see `scripts/edit_slots.txt`. To write any slot field through
`ObjectToolset` instead, pass `slot.get_path_name()` as `instance` (the slot's own object name is
auto-generated, so get it from Python rather than constructing it).

Read current slot values back from `DescribeWidget`'s `SlotProperties[]` — see below.

## Inspect a Widget

`WidgetStaticAnalysisToolset.DescribeWidget` returns walk-order hierarchy + properties + slot
properties in **one** call.

```python
import unreal

wbp = unreal.load_asset("/Game/UI/WBP_MainMenu")
res = unreal.WidgetStaticAnalysisToolset.describe_widget(
    wbp,
    None,     # StartWidget: None = whole tree
    -1,       # MaxDepth: -1 = no limit
    "",       # PropertyNameFilter: substring, "" = every property
    False,    # bSkipDefaultValues: False to see defaults too (it defaults to True!)
)

print(res.available_widget_count, "widget(s)")
for e in res.widgets:
    parent = e.parent.get_name() if e.parent else "-"
    print(f"{'  ' * e.depth}{e.widget_name} ({e.widget_class_path}) parent={parent}")
    for p in e.slot_properties:
        print(f"{'  ' * e.depth}    [slot] {p.property_name} = {p.exported_value}")
```

- A **single widget**: pass that `UWidget` as `StartWidget` with `MaxDepth=0`.
- `Parent` is null for the root **and** for named-slot content — check `NamedSlotHost` to tell them
  apart.
- An **empty result means the asset path is wrong**, not that the widget is empty.
- Tree depth alone: `unreal.WidgetStaticAnalysisToolset.get_widget_tree_depth(wbp)`.

Runnable: `scripts/inspect_hierarchy.txt`.

## Font Styling

`FSlateFontInfo` is a plain `USTRUCT(BlueprintType)`, so a font is an ordinary property write.
**Read the schema before composing the JSON** — the member names are the struct's, not VibeUE's.

```python
import json, vibeue

OT = "EditorToolset.ObjectToolset"
txt = "/Game/UI/WBP_MainMenu.WBP_MainMenu:WidgetTree.HeaderTitle"

print(vibeue.exec_tool(OT, "GetPropertySchema", {"instance": txt, "root": "Font", "maxDepth": 1}))

vibeue.exec_tool(OT, "SetProperties", {"instance": txt, "values": json.dumps({
    "Font": {"Size": 30, "TypefaceFontName": "Bold", "LetterSpacing": 20},
    "ColorAndOpacity": {"SpecifiedColor": {"R": 1.0, "G": 0.9, "B": 0.25, "A": 1.0},
                        "ColorUseRule": "UseColor_Specified"},
    "ShadowOffset": {"X": 2.0, "Y": 2.0},
    "ShadowColorAndOpacity": {"R": 0.0, "G": 0.0, "B": 0.0, "A": 0.75},
})})

print(vibeue.exec_tool(OT, "GetProperties",
                       {"instance": txt,
                        "properties": ["Font.Size", "ColorAndOpacity", "ShadowOffset"]}))
```

`ColorAndOpacity` on a TextBlock is an **`FSlateColor`** (`SpecifiedColor` + `ColorUseRule`);
`ShadowColorAndOpacity` is a plain `FLinearColor`. UMG colours are **linear**. Runnable:
`scripts/apply_font.txt`.

## Brush Styling

Same story — `FSlateBrush` is an ordinary struct. On an `Image` the brush property is `Brush`; other
widgets name theirs differently (`WidgetStyle.Normal` on a Button, `Background` on a Border), so
find the real name with `ListProperties` first:

```python
import json, vibeue

OT  = "EditorToolset.ObjectToolset"
img = "/Game/UI/WBP_MainMenu.WBP_MainMenu:WidgetTree.BackgroundImage"

print(vibeue.exec_tool(OT, "ListProperties", {"instance": img, "nameFilter": "brush|background"}))

vibeue.exec_tool(OT, "SetProperties", {"instance": img, "values": json.dumps({
    "Brush": {
        "ResourceObject": "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture",
        "DrawAs": "RoundedBox",
        "TintColor": {"SpecifiedColor": {"R": 0.08, "G": 0.12, "B": 0.2, "A": 1.0},
                      "ColorUseRule": "UseColor_Specified"},
        "ImageSize": {"X": 1920.0, "Y": 1080.0},
        "Margin": {"Left": 0.2, "Top": 0.2, "Right": 0.2, "Bottom": 0.2},
        "OutlineSettings": {"CornerRadii": {"X": 16.0, "Y": 16.0, "Z": 16.0, "W": 16.0}},
    },
})})
```

> `DrawAs` accepts `Image`, `Box`, `Border`, `RoundedBox`, `NoDrawType`. Rounded-box corner radii
> live under `OutlineSettings.CornerRadii` (a Vector4), not a top-level `CornerRadius` — confirm with
> `GetPropertySchema` on `root="Brush"` before trusting any of these member names.

Runnable: `scripts/apply_brush.txt`.

## Widget Animation

`UWidgetAnimation` extends `UMovieSceneSequence`, so `WidgetAnimationToolset` owns the lifecycle and
bindings and SequencerScripting owns tracks, sections and keys.

```python
import unreal

WAT = unreal.WidgetAnimationToolset
path = "/Game/UI/WBP_MainMenu"
wbp  = unreal.load_asset(path)
title = unreal.load_object(None, f"{path}.WBP_MainMenu:WidgetTree.HeaderTitle")

# 1. The animation.
info = WAT.create_widget_animation(wbp, "IntroFade", 0.5)
if info.animation is None:
    raise RuntimeError("CreateWidgetAnimation failed (name collision?)")

# 2. Bind the widget -> a sequencer binding proxy.
binding = WAT.add_widget_to_animation(wbp, info.animation, title)

# 3. Track + property path.
track = unreal.MovieSceneBindingExtensions.add_track(binding, unreal.MovieSceneFloatTrack)
unreal.MovieScenePropertyTrackExtensions.set_property_name_and_path(
    track, "RenderOpacity", "RenderOpacity")

# 4. Section, then keys on its channel.
section = unreal.MovieSceneTrackExtensions.add_section(track)
channels = unreal.MovieSceneSectionExtensions.get_numeric_channels(section)
for frame, value in [(0, 0.0), (15, 1.0)]:          # display-rate frames
    unreal.MovieSceneSectionExtensions.add_float_key(
        channels[0], unreal.FrameNumber(frame), value)

for a in WAT.list_widget_animations(wbp):
    print(a.animation_name, a.length_seconds, "bindings:", a.binding_count)
```

- `CreateWidgetAnimation` signals failure with `info.animation is None` — it does not raise.
- `add_widget_to_animation` also accepts a **`UPanelSlot`** (with non-null `Content`) to animate
  slot properties such as padding or anchors.
- Key times are in **display-rate** frames unless you pass
  `time_unit=unreal.MovieSceneTimeUnit.TICK_RESOLUTION`. Convert seconds yourself, or set the
  animation length with `set_widget_animation_length(animation, seconds)`.
- Bool properties (e.g. a visibility-style toggle on a bool channel) use
  `add_bool_key(section, channel_name, frame, value)` — section + channel **name**, not a
  descriptor. Double-precision channels use `add_double_key`.
- `set_key_time(channel, key, new_frame)` moves a key the add call returned.

Runnable: `scripts/create_animation.txt`.

## Bind Event

`bind_event(widget_path, widget_name, event_name, function_name)` is VibeUE's — Epic's
`AddComponentBoundEvent` is typed to `UActorComponent*` and cannot take a `UWidget`. Create the
function graph first (that part is `BlueprintToolset`):

```python
import unreal

path = "/Game/UI/WBP_Menu"
wbp  = unreal.load_asset(path)

# Step 1: the function graph (engine BlueprintToolset).
unreal.BlueprintToolset.add_function_graph(wbp, "OnPlayClicked", None)

# Step 2: bind the event to it (WidgetService keeps this).
ok = unreal.WidgetService.bind_event(path, "PlayButton", "OnClicked", "OnPlayClicked")

unreal.BlueprintToolset.compile_blueprint(wbp, False)
unreal.EditorAssetLibrary.save_asset(path)
print("bound:", ok)
```

Via `call_tool` the same two steps are
`call_tool(tool_name="AddFunctionGraph", toolset_name="BlueprintToolsets.BlueprintToolset", …)`
then an `execute_python` call for `bind_event`.

Common events: `OnClicked`, `OnPressed`, `OnReleased`, `OnHovered`, `OnUnhovered`.
`MVVMToolset.ListWidgetViewEvents(WidgetBlueprint)` lists the multicast-delegate event paths the
MVVM view can bind on that Blueprint.

> `bind_event` is only provably bound on the `GameInstance_*`-outered PIE instance — asset and
> preview instances always read unbound. Simulate a click with `on_clicked.broadcast()` on that
> instance.

## Edit the Hierarchy (rename / reparent / remove)

All four are VibeUE's — nothing in Epic's surface mutates a `UWidgetTree`.

```python
import unreal

path = "/Game/UI/WBP_Menu"

unreal.WidgetService.rename_widget(path, "ItemButton", "Item1_ActionButton")   # GUID preserved
unreal.WidgetService.reparent_widget(path, "BackgroundImage", "MainContainer") # object preserved
unreal.WidgetService.reorder_component(path, "PlayButton", 0)                  # within its parent
res = unreal.WidgetService.remove_component(path, "Item3")                     # add True to take children
print(res.success, res.removed_components, res.orphaned_children)

unreal.EditorAssetLibrary.save_asset(path)
```

`reparent_widget` rejects moving a panel into itself or a descendant, and the root cannot be
reparented. After any structural edit, re-read with `DescribeWidget` to confirm before saving.

## Capture Preview (designer screenshot)

> 🚨 **`EditorAppToolset.CaptureAssetImage` refuses Widget Blueprints.** Its allowlist is
> AnimationAsset, Skeleton, StaticMesh, SkeletalMesh, Texture, MaterialInterface; anything else
> raises "Asset type does not support image capture".

The surviving route is three calls, **needs the designer open**, and gives you a screenshot of that
open designer cropped to a Slate widget — not a deterministic offscreen render at a size you pick.
There is no width/height argument.

```python
import vibeue

EA = "EditorToolset.EditorAppToolset"
SI = "SlateInspectorToolset.SlateInspectorToolset"

vibeue.exec_tool(EA, "OpenEditorForAsset", {"assetPath": "/Game/UI/WBP_MainMenu"})

# Find the designer's widget ref in the snapshot text, then crop to it.
tree = vibeue.exec_tool(SI, "Snapshot", {"ref": "", "maxDepth": 30})
print(tree)                        # locate the designer/preview ref, e.g. "w123"
img = vibeue.exec_tool(SI, "Screenshot", {"ref": "<ref from the snapshot>"})
```

`Screenshot("")` captures the active window instead (falling back to the main editor window). It
fails with the reason on an unknown ref, a widget in no visible window, or one entirely outside it.

Cheaper alternatives when you don't need the live designer:
`EditorAppToolset.GetAssetThumbnails([paths])` returns the Content Browser's cached thumbnails
without loading or rendering the asset (an empty image where there is none), and
`EditorAppToolset.CaptureEditorImage()` grabs the whole editor window.

Runnable: `scripts/capture_preview.txt`.

## PIE Runtime Check

PIE lifecycle is Epic's; spawning a WBP into the viewport is VibeUE's.

`StartPIE` is **async** — it returns an async result and PIE is not live in the same call. Start it
in one step, then spawn/read in a later step (or `await vibeue.exec_tool_await` in a single script).

```python
import json, unreal, vibeue

EA = "EditorToolset.EditorAppToolset"
SI = "SlateInspectorToolset.SlateInspectorToolset"
OT = "EditorToolset.ObjectToolset"
path = "/Game/UI/WBP_MainMenu"

# Step 1 (its own call):
if not vibeue.exec_tool(EA, "IsPIERunning"):
    vibeue.exec_tool(EA, "StartPIE")

# Step 2 (after PIE is live):
handle = unreal.WidgetService.spawn_widget_in_pie(path)
print(handle.valid, handle.instance_id, handle.error_message)
if handle.valid:
    # Live property read: Snapshot -> GetUMGInfo -> ObjectToolset.GetProperties
    print(vibeue.exec_tool(SI, "Snapshot", {"ref": ""}))
    info = vibeue.exec_tool(SI, "GetUMGInfo", {"ref": "<ref of the HeaderTitle widget>"})
    if not info.get("error"):
        print(vibeue.exec_tool(OT, "GetProperties",
                               {"instance": info["widget"],
                                "properties": ["RenderOpacity"]}))
    unreal.WidgetService.remove_widget_from_pie(handle)   # pass the HANDLE, not instance_id
vibeue.exec_tool(EA, "StopPIE")
```

`GetUMGInfo` returns a live `Widget` handle (serialized as `{"refPath": …}`) plus `WidgetName`,
`WidgetClass`, `Asset`, `ResolvedVia` and the `OuterUserWidgets` chain; `Widget` is non-null exactly
when `Error` is empty. Call `SlateInspectorToolset.Observe(ref)` first when working in a subtree
whose widgets appear as you go, and `Unobserve` when done.

Runnable: `scripts/pie_inspect.txt`.

## Compile and Save

```python
import unreal
path = "/Game/UI/WBP_MainMenu"
wbp  = unreal.load_asset(path)

unreal.BlueprintToolset.compile_blueprint(wbp, False)   # or BlueprintEditorLibrary.compile_blueprint
unreal.EditorAssetLibrary.save_asset(path)
```

Asset-level problems: `AssetToolset.ValidateAssets([path])` returns per-asset errors and warnings
(this is what replaced the old widget `validate`).

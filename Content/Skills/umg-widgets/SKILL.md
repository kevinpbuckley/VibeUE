---
name: umg-widgets
display_name: UMG Widget Blueprints
description: Create and modify UMG Widget Blueprints (UI) — build the widget hierarchy, set properties, style fonts/brushes, author widget animations, bind events, capture previews, run PIE checks, and wire MVVM ViewModels. Use when the user asks to create a UI/HUD/menu, add or arrange widgets (Button, TextBlock, Image, panels), style or animate a widget, or set up data bindings in a Widget Blueprint (WBP).
vibeue_classes:
  - WidgetService
  - BlueprintService
unreal_classes:
  - EditorAssetLibrary
keywords:
  - umg
  - widget
  - ui
  - hud
  - menu
  - canvas
  - button
  - textblock
  - panel
  - viewmodel
  - mvvm
  - binding
  - animation
related_skills:
  - blueprints
  - blueprint-graphs
---

# UMG Widget Blueprints Skill

This skill covers authoring **Widget Blueprints (WBP)** with `WidgetService`. Read the Critical Rules
below first — they prevent the most common failures. Then pick a task from the Task Index and load the
matching workflow doc and/or run the matching sample script.

> Load the **`blueprints`** skill when you also need to add Blueprint variables, functions, or
> components, and **`blueprint-graphs`** when wiring event-graph nodes for bound events.

## Critical Rules

### 🚨 Creating Widget Blueprints — only one pattern works

`WidgetService` does **not** create the asset. Create it with the factory, then use `WidgetService`
to populate it:

```python
import unreal
factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property("parent_class", unreal.UserWidget)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_tools.create_asset("MainMenu", "/Game/UI", unreal.WidgetBlueprint, factory)
```

Do **not** call `BlueprintEditorLibrary.get_blueprint_class()`, `unreal.create_widget_blueprint()`, or
`WidgetService.create_widget()` — none exist. (Runnable: `scripts/create_widget.pyx`.)

### 🚨 Hierarchy: set the root first, parents before children

```python
unreal.WidgetService.add_component(path, "CanvasPanel", "RootCanvas", "", True)   # root: set_as_root=True
unreal.WidgetService.add_component(path, "Button", "PlayButton", "RootCanvas", False)
```

### 🚨 Property values are ALWAYS strings

```python
unreal.WidgetService.set_property(path, "PlayText", "Font.Size", "24")  # "24", not 24
```

### 🚨 Use dedicated style APIs for full font/brush edits

Use `set_font` / `set_brush` (with `WidgetFontInfo` / `WidgetBrushInfo`) when changing a complete font
or brush — they keep related fields together. Use `set_property` only for single leaf values or slot
aliases like `Position X`, `Size Y`, `Anchor Min X`. See `reference.md` for every field name; runnable
examples in `scripts/apply_font.pyx` and `scripts/apply_brush.pyx`.

`set_brush` requires a **`slot_name`**: `set_brush(widget_path, component_name, slot_name, brush_info)`
(for an Image the slot is `"Brush"`). `set_font` takes an optional `property_name` that defaults to `"Font"`.

### 🚨 Animations require real property paths

`add_animation_track` / `add_keyframe` target actual widget properties or slot aliases
(`RenderOpacity`, `ColorAndOpacity`, `Position X/Y`, `Size X/Y`). Always create the animation first,
then the track, then keyframes. (Runnable: `scripts/create_animation.pyx`.)

### 🚨 Preview vs PIE — different purposes

- `capture_preview` renders an editor-side PNG without starting gameplay — use for appearance checks.
- `start_pie` + `spawn_widget_in_pie` are for runtime state / live property reads — use only when you
  need a live instance.

### 🚨 Custom WBPs as components — discover first, pass the asset name

Use `list_widget_blueprints("")` to find existing WBPs, then pass just the asset name (e.g.
`"WBP_HealthBar"`, not the full package path) as `component_type`. The child WBP must already exist;
circular references are rejected; the parent recompiles automatically.

### 🚨 Field-name gotchas (return objects)

| WRONG | CORRECT |
|-------|---------|
| `p.name` | `p.property_name` |
| `p.type` | `p.property_type` |
| `p.property_value` | `p.current_value` |
| `w.component_name` / `w.name` | `w.widget_name` |

Full field tables for every return type are in `reference.md`.

### 🚨 Slot editing limits (set_property)

`set_property` only edits **Canvas** slot layout, via these aliases:
`Position X`, `Position Y`, `Size X`, `Size Y`, `Anchor Min X/Y`, `Anchor Max X/Y`.

Not currently settable via `set_property` (read-only in `slot_info`):
- **Z-order** (`ZOrder` / `Z Order`) — readable as `slot_info.z_order` but no setter.
- **Box-slot** alignment / padding / size-rule (VerticalBox/HorizontalBox/Overlay children) — the
  child exposes the slot only as a whole `Slot` object; individual alignment/padding have no setter.

There is also **no reparent/move API** — a widget's parent is fixed at `add_component` time. To move a
widget, `remove_component` it and re-add it under the new parent.

### Panel types

| Type | Purpose |
|------|---------|
| `CanvasPanel` | Absolute positioning (X, Y coords) |
| `VerticalBox` | Stack children vertically |
| `HorizontalBox` | Stack children horizontally |
| `Overlay` | Stack children on top of each other |

---

## Task Index

Pick the task, load its workflow section, and/or run its sample script via `execute_python_code`.
Sample scripts under `scripts/` are **runnable examples** — edit the variables at the top, then execute.

| Task | Workflow | Sample script (run via `execute_python_code`) |
|------|----------|-----------------------------------------------|
| Create a Widget Blueprint | `workflows.md` → Create Widget Blueprint | `scripts/create_widget.pyx` |
| Build a menu (root + panel + buttons) | `workflows.md` → Build a Menu | `scripts/build_menu.pyx` |
| Position widgets on a CanvasPanel | `workflows.md` → Canvas Positioning | — |
| Inspect a widget (hierarchy + properties) | `workflows.md` → Inspect a Widget | `scripts/inspect_hierarchy.pyx` |
| Apply full font styling | `workflows.md` → Font Styling | `scripts/apply_font.pyx` |
| Apply full brush styling | `workflows.md` → Brush Styling | `scripts/apply_brush.pyx` |
| Author a widget animation | `workflows.md` → Widget Animation | `scripts/create_animation.pyx` |
| Bind a widget event to a function | `workflows.md` → Bind Event | — |
| Rename / remove a widget | `workflows.md` → Edit the Hierarchy | — |
| Capture a preview PNG | `workflows.md` → Capture Preview | `scripts/capture_preview.pyx` |
| Inspect a widget live in PIE | `workflows.md` → PIE Runtime Check | `scripts/pie_inspect.pyx` |
| Add MVVM ViewModel + bindings | `mvvm.md` | `scripts/mvvm_hud.pyx` |

## Sub-docs

Load these only when the task needs them (read the file under
`Plugins/VibeUE/Content/Skills/umg-widgets/`, or `manage_skills(action='load', skill_name='umg-widgets/<name>')`):

- **`workflows.md`** — step-by-step task workflows with copy-paste Python for every task above.
- **`reference.md`** — field-name tables for every return type (`FWidgetInfo`, `FWidgetComponentSnapshot`,
  `FWidgetSlotInfo`, `FWidgetFontInfo`, `FWidgetBrushInfo`, `FWidgetAnimInfo`, `FWidgetPreviewResult`,
  `FPIEWidgetHandle`), the WidgetService action list, common properties, and event names.
- **`mvvm.md`** — MVVM ViewModel support: rules, creation types, binding modes, field names, and full
  add/bind/list/remove workflows.

## Verification (do this before claiming success)

1. After hierarchy edits: re-read with `get_widget_snapshot(path)` and confirm the expected widgets/parents.
2. After property/style edits: read the value back (`get_property` / `get_font` / `get_brush`).
3. Open/compile the WBP and confirm no errors, then `unreal.EditorAssetLibrary.save_asset(path)`.

Never report a widget as created/configured until it appears in a fresh `get_widget_snapshot` result.

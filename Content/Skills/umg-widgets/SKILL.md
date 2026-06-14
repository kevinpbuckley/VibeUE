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

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "umg-and-slate"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

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

### 🚨 Color properties: FSlateColor vs FLinearColor — verify with a readback

`ColorAndOpacity` on text/image widgets is **FSlateColor**, whose export text is
`"(SpecifiedColor=(R=1.0,G=0.5,B=0.0,A=1.0))"` — a bare LinearColor string is auto-wrapped
into that form by `set_property`. `ShadowColorAndOpacity` is a plain **FLinearColor**:
`"(R=0.0,G=0.0,B=0.0,A=1.0)"`. Named color strings ("red", "orange") are NOT supported on
either — pass numeric RGBA.

**Always `get_property` after `set_property` and compare.** A struct set can return `True`
while writing nothing (UE struct import ignores unrecognized member names). If the readback
still shows the old value, your value string didn't match the struct's member layout — check
`list_properties` for the property's real type and use its export-text form.

### 🚨 Use dedicated style APIs for full font/brush edits

Use `set_font` / `set_brush` (with `WidgetFontInfo` / `WidgetBrushInfo`) when changing a complete font
or brush — they keep related fields together. Use `set_property` only for single leaf values or slot
aliases like `Position X`, `Size Y`, `Anchor Min X`. See `reference.md` for every field name; runnable
examples in `scripts/apply_font.pyx` and `scripts/apply_brush.pyx`.

`set_brush` requires a **`slot_name`**: `set_brush(widget_path, component_name, slot_name, brush_info)`
(for an Image the slot is `"Brush"`). `set_font` takes an optional `property_name` that defaults to `"Font"`.

> ⚠️ **`set_property("Font.Typeface", ...)` silently returns False** (size and color leaf-sets work,
> but typeface/family do not go through the struct sub-property path). Always change typeface/family
> via `set_font` with a `WidgetFontInfo`. Don't trust the `True/False` return of `set_property` on
> nested struct sub-fields — read it back to confirm.

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
| `w.widget_type` | `w.widget_class` |

`WidgetInfo` complete fields: `widget_name`, `widget_class`, `parent_widget`, `is_root_widget`, `is_variable`, `children`.

**`children` is an `Array[str]` of widget NAMES, not nested objects.** Iterating it and reading
`.widget_name`/`.is_variable` raises `AttributeError: 'str' object has no attribute ...`. To walk
the tree, build a name→info map first:

```python
infos = {w.widget_name: w for w in unreal.WidgetService.get_hierarchy(path)}
for child_name in infos["RootCanvas"].children:
    child = infos[child_name]          # look the child up by name
    print(child.widget_name, child.widget_class, child.is_variable)
```

`get_widget_snapshot(path)` returns full data (hierarchy + slot + all properties) for a valid
path — if it comes back **empty, the path is wrong**; do not conclude the widget is empty.

Full field tables for every return type are in `reference.md`.

### 🚨 Auditing widgets (best-practices review) — use these exact APIs

For "do our widgets follow best practices?" style audits:

- **Property bindings** (the #1 anti-pattern): in a snapshot's `properties`, any `*Delegate`
  entry (e.g. `VisibilityDelegate`, `TextDelegate`) with `current_value` of `(null).None` is
  **unbound** — a function name there means a polling property binding exists.
- **Tick/graph checks** go through `BlueprintService` (a WBP is a Blueprint):
  `list_graphs(path)` → `get_nodes_in_graph(path, "EventGraph")` → find `Event Tick` →
  `get_node_pins(path, graph, node_id)` and check each pin's `connected` flag. A Tick node with
  no connected pins is just the default stub, not live logic.
- These methods do **NOT** exist — do not guess them: `BlueprintService.get_graphs`,
  `get_functions`, `get_connected_nodes`, `unreal.find_class`. Use `list_graphs`,
  `list_functions`, `get_nodes_in_graph`, `get_node_details` (includes pin connections), and
  `unreal.load_class(None, path)` instead.

### Slot editing via `set_property`

`set_property` edits slot layout through these aliases (string values):

- **Canvas children:** `Position X/Y`, `Size X/Y`, `Anchor Min X/Y`, `Anchor Max X/Y`, `Alignment X/Y`
  (pivot — set to `0.5` with anchors at `0.5` to truly center), `ZOrder` (or `Z Order`).
- **Box/Overlay children** (VerticalBox/HorizontalBox/Overlay): `Horizontal Alignment` / `Vertical Alignment`
  (values `Fill`/`Left`/`Center`/`Right`/`Top`/`Bottom`), `Padding` (one value or `(Left=..,Top=..,Right=..,Bottom=..)`),
  `Padding Left/Top/Right/Bottom`, and on box slots `Size Rule` (`Fill`/`Automatic`) + `Size Value`.

```python
unreal.WidgetService.set_property(path, "PlayButton", "ZOrder", "5")
unreal.WidgetService.set_property(path, "HeaderRow", "Vertical Alignment", "Top")
unreal.WidgetService.set_property(path, "HeaderRow", "Padding", "8")
```

### Reparenting — `reparent_widget`

Move an existing widget to a new parent panel (preserves the widget object/GUID):

```python
unreal.WidgetService.reparent_widget(path, "BackgroundImage", "MainContainer")
```

Rejects moving a panel into itself/a descendant; the root cannot be reparented.

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
| Edit slot layout (z-order, alignment, padding) / reparent | SKILL.md → Slot editing | `scripts/edit_slots.pyx` |
| Capture a preview PNG | `workflows.md` → Capture Preview | `scripts/capture_preview.pyx` |
| Inspect a widget live in PIE | `workflows.md` → PIE Runtime Check | `scripts/pie_inspect.pyx` |
| Add MVVM ViewModel + bindings | `mvvm.md` | `scripts/mvvm_hud.pyx` |

## Sub-docs

Load these only when the task needs them (read the file under
`Plugins/VibeUE/Content/Skills/umg-widgets/`, or `vibeue-skills-manager(action='load', skill_name='umg-widgets/<name>')`):

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

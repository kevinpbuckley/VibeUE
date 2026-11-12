---
name: umg-widgets
display_name: UMG Widget Blueprints
description: Create and modify UMG Widget Blueprints (UI). Widget event binding, spawning a widget into PIE and removing a ViewModel are VibeUE's `unreal.WidgetService.*`; everything else — tree edits (add/remove/rename/reorder/reparent via `UMGEditor.WidgetBlueprintEditorSubsystem`), reading the tree, properties, fonts, brushes, slots, animations, MVVM ViewModels, PIE lifecycle, screenshots — is Epic's engine toolsets via `call_tool`. Use when the user asks to create a UI/HUD/menu, add or arrange widgets (Button, TextBlock, Image, panels), style or animate a widget, audit a widget's bindings, or set up data bindings in a Widget Blueprint (WBP).
vibeue_classes:
  - WidgetService
unreal_classes:
  - WidgetStaticAnalysisToolset
  - WidgetAnimationToolset
  - MVVMToolset
  - ObjectToolset
  - AssetToolset
  - EditorAppToolset
  - SlateInspectorToolset
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

Authoring **Widget Blueprints (WBP)** is now split between two surfaces. Read **Which surface owns
what** and **Call styles** first — picking the wrong one is the most common failure. Then pick a task
from the Task Index and load the matching workflow doc and/or run the matching sample script.

> Load the **`blueprints`** skill when you also need to add Blueprint variables, functions, or
> components, and **`blueprint-graphs`** when wiring event-graph nodes for bound events.

## Which surface owns what

`UWidgetService` is down to **4 methods**. They are the ones Epic's toolsets cannot do:

| VibeUE keeps | Why nothing in Epic's surface replaces it |
|---|---|
| `bind_event` | Epic's `BlueprintToolset.AddComponentBoundEvent` is typed `UActorComponent*`, and a `UWidget` is not one. |
| `spawn_widget_in_pie`, `remove_widget_from_pie` | No Epic tool constructs a `UUserWidget` or calls `AddToViewport`. `CommonUIToolset.ActivateWidget` only addresses widgets the game already created. |
| `remove_view_model` | `MVVMToolset` can add a ViewModel to a widget but never remove one. |

**Everything else is Epic's.** Editing the tree (`UMGEditor.WidgetBlueprintEditorSubsystem`: `CreateAndAddWidget`, `MoveWidget`, `RemoveWidget`, `RenameWidget`, `ToggleWidgetAsVariable`), reading the tree, every property read/write (fonts and brushes
included), slots, animations, MVVM, PIE lifecycle, asset discovery, validation and screenshots.
See the removal table at the bottom for the exact 39 → replacement mapping.

## Call styles — get this right or every call fails

**1. VibeUE's 4 survivors are Python**, inside `execute_python`:

```python
import unreal
unreal.WidgetService.bind_event(path, "PlayButton", "OnClicked", "HandlePlay")
```

**2. Epic's toolsets are `call_tool`** — one tool per MCP round-trip:

```
call_tool(tool_name="DescribeWidget",
          toolset_name="WidgetStaticAnalysisToolset.WidgetStaticAnalysisToolset",
          arguments={"widgetBlueprint": "/Game/UI/WBP_Menu.WBP_Menu"})
```

**3. Inside `execute_python`, reach Epic's toolsets with `vibeue.exec_tool`** — the same dispatch
`call_tool` uses, but in-process, so a whole task batches into one round-trip. This is what the
sample scripts use:

```python
import vibeue
desc = vibeue.exec_tool("WidgetStaticAnalysisToolset.WidgetStaticAnalysisToolset", "DescribeWidget",
                        {"widgetBlueprint": "/Game/UI/WBP_Menu.WBP_Menu"})
```

`exec_tool` fills missing optional params from the tool's schema, reports all missing required
params in one error, and returns fully-decoded Python values. For a genuinely async tool
(`AssetToolset.FindAssets`, `EditorAppToolset.StartPIE` / `CaptureAssetImage` /
`GetAssetThumbnails`) use `await vibeue.exec_tool_await(...)` or `vibeue.exec_tool_collect(...)`.

### 🚨 Four of Epic's toolsets have NO Python binding at all

A UFUNCTION reaches `unreal.*` only if it is `BlueprintCallable` or carries `PythonCallable`.

| Toolset | `unreal.<Toolset>.<method>()` |
|---|---|
| `WidgetStaticAnalysisToolset`, `WidgetAnimationToolset`, `MVVMToolset`, `MovieScene*Extensions`, `BlueprintToolset` | ✅ works — these are `BlueprintCallable`/`PythonCallable` |
| `ObjectToolset`, `AssetToolset`, `EditorAppToolset`, `SlateInspectorToolset` | ❌ **fails** — `AICallable` only. `call_tool` / `vibeue.exec_tool` is the only path |

So `unreal.ObjectToolset.set_properties(...)` and `unreal.AssetToolset.find_assets(...)` do not
exist. Don't guess them.

### Object arguments

Epic's tools take live objects, not path strings. In JSON, pass the **object path** (or
`{"refPath": "<object path>"}`), e.g. `"/Game/UI/WBP_Menu.WBP_Menu"` for the WBP asset and
`"/Game/UI/WBP_Menu.WBP_Menu:WidgetTree.PlayButton"` for a widget inside it. From Python, pass the
real object: `unreal.load_asset("/Game/UI/WBP_Menu")`.

> ⚠️ **Edit `<Asset>:WidgetTree`, never `<Asset>_C:WidgetTree`.** The first is the editable source
> tree; the second is the generated copy that **compile rebuilds**, so writes there are silently
> discarded. (This is the real reason the old "slot writes vanish on compile" warning existed.)

For exact JSON key names on any tool, read its schema rather than guessing:
`describe_tools`, or `vibeue.get_toolset_schema("<Toolset.Name>")` in Python.

---

## Critical Rules

### 🚨 Creating Widget Blueprints

Epic's `UMGEditor.WidgetBlueprintEditorSubsystem.CreateWidgetBlueprint(folderPath, assetName, parentClass)`
creates the asset; in Python the factory does the same. Then populate it:

```python
import unreal
factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property("parent_class", unreal.UserWidget)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_tools.create_asset("MainMenu", "/Game/UI", unreal.WidgetBlueprint, factory)
```

Do **not** call `BlueprintEditorLibrary.get_blueprint_class()` or `unreal.create_widget_blueprint()`
— neither exists. (Runnable: `scripts/create_widget.txt`.)

### 🚨 Hierarchy: set the root first, parents before children

Tree edits are Epic's `UMGEditor.WidgetBlueprintEditorSubsystem` (reach it with `vibeue.exec_tool`):

```python
import vibeue
WS = "UMGEditor.WidgetBlueprintEditorSubsystem"
WBP = "/Game/UI/WBP_Menu.WBP_Menu"
TREE = "/Game/UI/WBP_Menu.WBP_Menu:WidgetTree."
# no parentWidget + empty tree -> the new widget becomes the root
vibeue.exec_tool(WS, "CreateAndAddWidget", {"widgetBlueprint": WBP, "widgetClass": "/Script/UMG.CanvasPanel",
                                            "widgetDisplayName": "RootCanvas"})
vibeue.exec_tool(WS, "CreateAndAddWidget", {"widgetBlueprint": WBP, "widgetClass": "/Script/UMG.Button",
                                            "widgetDisplayName": "PlayButton",
                                            "parentWidget": TREE + "RootCanvas", "childIndex": -1})
```

`widgetClass` is a class path (`/Script/UMG.<Type>` for native, `/Game/UI/WBP_X.WBP_X_C` for a custom
WBP), not a short name. Pass the parent panel explicitly: only an **empty tree** makes the first widget
the root. `childIndex` inserts at a position; `-1` appends. The new widget is not compiled, and
whether it is a Blueprint variable is not set here -- call `ToggleWidgetAsVariable(widgetBlueprint,
widget, bIsVariable)` when you need `BindWidget` or a graph reference. See `reference.md` ▸ Child order.

> ⚠️ **Widget names are unique per-blueprint, NOT per-parent.** UMG enforces one name across the
> whole Widget Blueprint, so you can't add an `ItemLabel`/`ItemButton` under each of `Item1`,
> `Item2`, `Item3`. Suffix per-parent (`Item1Label`, `Item2Label`, …) — reusing a name silently
> lands the component on the wrong parent or no-ops.

### 🚨 Reading a widget — `DescribeWidget` is the one call

`WidgetStaticAnalysisToolset.DescribeWidget` replaces every VibeUE read in one shot. It returns
`Widgets[]` in **walk order**, each entry carrying:

| Field | Meaning |
|---|---|
| `Widget` | the live `UWidget` handle — feed it straight to `ObjectToolset` or `AddWidgetToAnimation` |
| `Parent` | parent `UPanelWidget`; **null** for the root *and* for named-slot content (see `NamedSlotHost`) |
| `WidgetClassPath` | class path, e.g. `/Script/UMG.Button` |
| `WidgetName` | instance name |
| `bIsVariable` | exposed as a Blueprint variable (what `BindWidget` needs) |
| `Depth` | 0 at the walk's `StartWidget`, +1 per level |
| `NamedSlotHost` | set when this widget is content in a named slot on that host |
| `bInherited` | declared on the parent class, not user-created |
| `Properties[]` | `{PropertyName, ExportedValue}` pairs on the widget |
| `SlotProperties[]` | the same shape for the widget's **`Slot`** object (padding, alignment, anchors, …) |

Plus `AvailableWidgetCount` (the canary if `Widgets` got cut) and, with
`bIncludeFullDescription=true`, a verbose `Description` text tree whose line `[N]` matches
`Widgets[N]`.

> 🚨 **`bSkipDefaultValues` defaults to `true`** — `Properties[]`/`SlotProperties[]` show only
> values that differ from the class default. A property you set to its default value, or never
> touched, is simply **absent**. Pass `bSkipDefaultValues=false` to see everything.

Parameters: `(WidgetBlueprint, StartWidget=null, MaxDepth=-1, PropertyNameFilter="",
bSkipDefaultValues=true, bIncludeFullDescription=false, bIncludeComposedColors=false)`.
`MaxDepth` -1 = no limit, 0 = `StartWidget` only. `PropertyNameFilter` is a substring match.

An empty result means the **asset path is wrong** — not that the widget is empty.

```python
import unreal
wbp = unreal.load_asset("/Game/UI/WBP_MainMenu")
res = unreal.WidgetStaticAnalysisToolset.describe_widget(wbp, None, -1, "", False)
for e in res.widgets:
    print("  " * e.depth, e.widget_name, e.widget_class_path,
          "var" if e.is_variable else "")
```

Tree depth alone: `GetWidgetTreeDepth(WidgetBlueprint, StartWidget=null)` → int (`-1` on error or
no root).

### 🚨 Properties are JSON now, not export-text strings

`ObjectToolset` is JSON-schema driven. Four tools, all `call_tool`-only:

| Tool | Signature | Note |
|---|---|---|
| `ListProperties` | `(Instance, Root="", NameFilter="", MaxDepth=1)` → `string[]` | dotted paths. `MaxDepth` defaults to **1**; pass 0 for every level. `NameFilter` is a regex |
| `GetPropertySchema` | `(Instance, Root="", MaxDepth=1)` → JSON Schema | go here for a property's **type/shape**; much larger than `ListProperties` |
| `GetProperties` | `(Instance, Properties[])` → JSON string | keyed by the path as written |
| `SetProperties` | `(Instance, Values)` → bool | `Values` is a **JSON string**, not an object |

```python
import json, vibeue
OT = "EditorToolset.ObjectToolset"
txt = "/Game/UI/WBP_MainMenu.WBP_MainMenu:WidgetTree.PlayText"

vibeue.exec_tool(OT, "SetProperties", {"instance": txt,
                                       "values": json.dumps({"Text": "PLAY"})})
print(vibeue.exec_tool(OT, "GetProperties", {"instance": txt, "properties": ["Text"]}))
```

Dotted paths and subscripts work (`"Font.Size"`, `"OverrideMaterials[0]"`). A subscript addresses an
existing element — set the whole container to add or remove elements. A value outside the bounds its
own schema advertises (`minimum`/`maximum`/`maxLength`) is **refused**, not clamped.

Passing the **WidgetBlueprint asset** as `Instance` resolves to its generated class's CDO — that is
how you edit the `UserWidget` itself rather than a child widget.

### 🚨 Fonts and brushes are ordinary property writes

`FSlateFontInfo` and `FSlateBrush` are both `USTRUCT(BlueprintType)`, so there is nothing special
left: write them with `ObjectToolset.SetProperties` like any other struct. Read the exact member
shape from `GetPropertySchema` first — don't hand-build it from memory.

```python
vibeue.exec_tool(OT, "SetProperties", {"instance": txt, "values": json.dumps({
    "Font": {"Size": 30, "TypefaceFontName": "Bold", "LetterSpacing": 20},
})})
```

Runnable: `scripts/apply_font.txt`, `scripts/apply_brush.txt`.

### 🚨 Colour: `FSlateColor` vs `FLinearColor` — and UMG colours are LINEAR

`ColorAndOpacity` on text/image widgets is an **`FSlateColor`**: two members, `SpecifiedColor`
(an `FLinearColor`) and `ColorUseRule`. A bare RGBA object is **not** an `FSlateColor` — nothing
auto-wraps it any more:

```json
{"ColorAndOpacity": {"SpecifiedColor": {"R": 1.0, "G": 0.5, "B": 0.0, "A": 1.0},
                     "ColorUseRule": "UseColor_Specified"}}
```

`ShadowColorAndOpacity` is a plain `FLinearColor`: `{"R": 0.0, "G": 0.0, "B": 0.0, "A": 1.0}`.
Named colour strings ("red", "orange") are not supported on either. UMG colours are **linear** — a
dark sRGB red is about `(0.035, 0.002, 0.002)`. Confirm the shape with `GetPropertySchema` and read
the value back with `GetProperties`.

For "what colour does this actually render as", `DescribeWidget` with
`bIncludeComposedColors=true` fills each entry's `ComposedColors[]`: the composed pre-display
**linear** tint per interactable state, with `CompositionFactors` and, when a factor could not be
resolved, `Gaps`. It is never a display-space value, and never covers `URichTextBlock`.

### 🚨 Slots: read with `DescribeWidget`, write on the slot object

Slot layout lives on the widget's `Slot` object (`UCanvasPanelSlot`, `UVerticalBoxSlot`, …). Read it
from the entry's `SlotProperties[]`. To write it, you need the slot object — the slot's own name is
auto-generated, so get it from Python rather than constructing a path:

```python
import unreal, json, vibeue
w = unreal.load_object(None, "/Game/UI/WBP_Menu.WBP_Menu:WidgetTree.PlayButton")
slot = w.slot                                     # UCanvasPanelSlot
slot.set_z_order(5)                               # typed setters are simplest
# ...or hand the slot to ObjectToolset by its real path:
vibeue.exec_tool("EditorToolset.ObjectToolset", "SetProperties",
                 {"instance": slot.get_path_name(),
                  "values": json.dumps({"ZOrder": 5})})
```

Canvas children: `Anchors` (`Minimum`/`Maximum`), `Offsets` (a Margin: left/top/right/bottom, which
is position+size when anchors coincide), `Alignment` (the **pivot** — set it to `0.5` with anchors
at `0.5` to truly centre), `ZOrder`, `bAutoSize`. Box/Overlay children: `Padding`,
`HorizontalAlignment`, `VerticalAlignment`, and on box slots `Size` (`SizeRule` + `Value`).

VibeUE ships two helpers for the common canvas case:
`vibeue.canvas_slot(wbp_path, widget_name)` returns the design-time `CanvasPanelSlot` (or `None`),
and `vibeue.set_canvas_slot(wbp_path, widget_name, anchor_min=…, anchor_max=…, offsets=…,
alignment=…, auto_size=…)` applies whole anchors/offsets then compiles and saves. Runnable:
`scripts/edit_slots.txt`.

### 🚨 Override-gated properties (SizeBox width/height etc.)

`set_editor_property("width_override", 34.0)` on a `USizeBox` stores the value but leaves the paired
`bOverride_WidthOverride` edit-condition flag false — the override silently never applies (the value
even reads back as 34.0). Either call the dedicated setters (`set_width_override()`,
`set_height_override()`, `set_min_desired_width()`, …), which set flag + value together, or write
**both** keys in one `SetProperties` call:
`{"bOverride_WidthOverride": true, "WidthOverride": 34.0}`. Applies to any widget property gated by
a `bOverride_*` flag; after a raw property set, read the flag back before trusting the layout.

### 🚨 Animations: lifecycle on `WidgetAnimationToolset`, keys on SequencerScripting

`UWidgetAnimation` **extends `UMovieSceneSequence`**. That is the whole design: `WidgetAnimationToolset`
owns the UMG-specific lifecycle and bindings, and every `MovieSceneSequenceExtensions` /
`MovieSceneTrackExtensions` / `MovieSceneSectionExtensions` / `MovieScenePropertyTrackExtensions`
method works on a widget animation **unmodified**.

The track+keyframe chain is now:

```
AddWidgetToAnimation                       -> FMovieSceneBindingProxy
  MovieSceneBindingExtensions.AddTrack              (TrackType, e.g. MovieSceneFloatTrack)
  MovieScenePropertyTrackExtensions.SetPropertyNameAndPath   (name + path, e.g. "RenderOpacity")
  MovieSceneTrackExtensions.AddSection
  MovieSceneSectionExtensions.GetNumericChannels    -> pick the channel descriptor
  MovieSceneSectionExtensions.AddFloatKey / AddDoubleKey / AddBoolKey [+ SetKeyTime]
```

`WidgetAnimationToolset` (10 tools): `CreateWidgetAnimation(WidgetBlueprint, AnimationName,
LengthSeconds=1.0)`, `ListWidgetAnimations`, `FindWidgetAnimation`, `RenameWidgetAnimation`,
`SetWidgetAnimationDisplayLabel`, `SetWidgetAnimationLength`, `RemoveWidgetAnimation`,
`AddWidgetToAnimation(WidgetBlueprint, Animation, ObjectToBind)`, `GetWidgetAnimationBindings`,
`RemoveWidgetBinding(Animation, BindingGuid)`.

`ObjectToBind` must be a `UWidget` in the tree, **or** a `UPanelSlot` with non-null `Content` — that
second form is how you animate slot properties (padding, anchors). `CreateWidgetAnimation` signals
failure by returning an empty info (`Animation == None`), not an exception.

`AddFloatKey`/`AddDoubleKey` take a `FMovieSceneNumericChannelDescriptor` (from
`GetNumericChannels`), a frame number and a value; `InTime` is in **display-rate** frames unless you
pass `TimeUnit=TickResolution`. `AddBoolKey` takes `(Section, ChannelName, Frame, Value)` instead.
Runnable: `scripts/create_animation.txt`.

### 🚨 Preview capture is the weakest replacement — say so to the user

**`EditorAppToolset.CaptureAssetImage` explicitly refuses Widget Blueprints.** Its allowlist is
`AnimationAsset`, `Skeleton`, `StaticMesh`, `SkeletalMesh`, `Texture`, `MaterialInterface`; anything
else raises *"Asset type does not support image capture"*.

The surviving route is **3 calls and needs the designer open**:

```
EditorAppToolset.OpenEditorForAsset(assetPath)      # the WBP designer must be open
SlateInspectorToolset.Snapshot(ref="")              # find the designer's widget ref
SlateInspectorToolset.Screenshot(ref)               # crops to that Slate widget
```

Be honest about what that is: a **screenshot of the open designer**, cropped to a Slate widget —
not a deterministic offscreen render at a size you choose. There is no width/height. If the designer
is not open, or the widget is in no visible window, `Screenshot` fails with the reason. For a cheap
"what does this asset look like" with no designer, `EditorAppToolset.GetAssetThumbnails(assetPaths)`
returns the Content Browser's cached thumbnails (empty entry when an asset has none), and
`EditorAppToolset.CaptureEditorImage()` grabs the whole editor window. Runnable:
`scripts/capture_preview.txt`.

### 🚨 Preview vs PIE — different purposes

- The designer-screenshot route above is for **appearance** checks, no gameplay.
- `EditorAppToolset.StartPIE` + `unreal.WidgetService.spawn_widget_in_pie` are for **runtime state /
  live property reads** — use only when you need a live instance.

`StartPIE` is **async**: it returns a `UAsyncFunctionResultVoid`, and PIE is not live in the same
call. Start it in one step (or `await vibeue.exec_tool_await`), then spawn/read in a later step.
`IsPIERunning` → bool is the gate. Reading a live widget's property is a 3-step chain:
`SlateInspectorToolset.Snapshot` → `GetUMGInfo(ref)` (returns a live `Widget` handle, serialized as
`{refPath}`) → `ObjectToolset.GetProperties` on that handle. Runnable: `scripts/pie_inspect.txt`.

### 🚨 Custom WBPs as components — discover first, pass the asset name

`CreateAndAddWidget` takes a **class path** as `widgetClass`: `/Script/UMG.Button` for a native widget,
or the generated class of a custom WBP (`/Game/UI/WBP_HealthBar.WBP_HealthBar_C`). Find existing WBPs
with `AssetToolset.FindAssets` (filter `assetType` = `/Script/UMGEditor.WidgetBlueprint`) and append
`_C` to the object path. The child WBP must already exist and be compiled; circular references are
rejected by Epic's tool too. Compile the parent afterwards (`BlueprintToolset.CompileBlueprint`).

For **native** widget types, `ObjectToolset.SearchSubclasses(BaseClass=/Script/UMG.Widget)` lists
them (optional `ClassName` substring filter).

### 🚨 Auditing widgets (best-practices review) — use these exact tools

`WidgetStaticAnalysisToolset` has two tools built for exactly this:

- **`GetWidgetConnectionMap(WidgetBlueprint, ScopeFilter="")`** — every MVVM field binding, event
  binding, delegate and **legacy property binding** in one pre-sorted checklist,
  Unconnected/Misconfigured rows first, with `ConnectedCount` / `UnconnectedCount` /
  `MisconfiguredCount`. Legacy property bindings (the polling-delegate anti-pattern) surface as
  `Kind == LegacyPropertyBinding` — that single call replaces hand-scanning for `*Delegate`
  properties. `Misconfigured` only applies to `VMFieldBinding` rows, with `Fixability`
  `Fixable`/`Unbridgeable`/`Unknown`. `ScopeFilter` is a case-insensitive substring of the Kind name
  (`"VM"`, `"Event"`, `"Delegate"`, `"Legacy"`).
- **`TraceWidgetPropertyOverrides(WidgetBlueprint, PropertyNameFilter="", WidgetNameFilter="")`** —
  for "who actually sets this property?". Merges Blueprint-graph writes with class defaults,
  archetype overrides and bindings, grouped per (widget, property): `Chains[]` (every write, with
  `Source`, `SourceDetail`, `ExportedValue`, `ApplicationStatus`), `Winners[]` (the write it
  believes wins, with `Rationale` and `bCertain`) and `Flags[]` (`Redundant` / `Conflict` with a
  `Recommendation`). Both filters are **exact** matches, not substrings.
- **`GetViewModelConnectionMap(ViewModelClass, ScopeFilter="")`** — reverse search: every WBP whose
  MVVM View references that ViewModel class, grouped per widget. Native (non-Blueprint) ViewModel
  classes never surface here (no on-disk package identity to search for).

For Tick / event-graph checks on a WBP (a WBP is a Blueprint), load the **`blueprint-graphs`** skill
— that surface is its scope, not this one.

### Panel types

| Type | Purpose |
|------|---------|
| `CanvasPanel` | Absolute positioning (X, Y coords) |
| `VerticalBox` | Stack children vertically |
| `HorizontalBox` | Stack children horizontally |
| `Overlay` | Stack children on top of each other |

---

## Task Index

Pick the task, load its workflow section, and/or run its sample script via `execute_python`.
Sample scripts under `scripts/` are **runnable examples** — edit the variables at the top, then execute.

| Task | Workflow | Sample script (run via `execute_python`) |
|------|----------|-----------------------------------------------|
| Create a Widget Blueprint | `workflows.md` → Create Widget Blueprint | `scripts/create_widget.txt` |
| Build a menu (root + panel + buttons) | `workflows.md` → Build a Menu | `scripts/build_menu.txt` |
| Position widgets on a CanvasPanel | `workflows.md` → Canvas Positioning | `scripts/edit_slots.txt` |
| Inspect a widget (hierarchy + properties) | `workflows.md` → Inspect a Widget | `scripts/inspect_hierarchy.txt` |
| Apply full font styling | `workflows.md` → Font Styling | `scripts/apply_font.txt` |
| Apply full brush styling | `workflows.md` → Brush Styling | `scripts/apply_brush.txt` |
| Author a widget animation | `workflows.md` → Widget Animation | `scripts/create_animation.txt` |
| Bind a widget event to a function | `workflows.md` → Bind Event | — |
| Rename / remove a widget | `workflows.md` → Edit the Hierarchy | — |
| Edit slot layout (z-order, alignment, padding) / reparent | SKILL.md → Slots | `scripts/edit_slots.txt` |
| Screenshot the open designer | `workflows.md` → Capture Preview | `scripts/capture_preview.txt` |
| Inspect a widget live in PIE | `workflows.md` → PIE Runtime Check | `scripts/pie_inspect.txt` |
| Add MVVM ViewModel + bindings | `mvvm.md` | `scripts/mvvm_hud.txt` |
| Audit bindings / find who sets a property | SKILL.md → Auditing widgets | — |

## Sub-docs

Load these only when the task needs them — read the sibling file directly under
`Plugins/VibeUE/Skills/umg-widgets/` (the engine `AgentSkillToolset` `GetSkills` is the loader that
exposes this skill; there is no `vibeue-skills-manager` tool):

- **`workflows.md`** — step-by-step task workflows with copy-paste Python for every task above.
- **`reference.md`** — the 4 surviving `WidgetService` signatures and their return-struct fields,
  the Epic toolset/tool index with toolset names, common widget properties, and event names.
- **`mvvm.md`** — MVVM on `MVVMToolset`: rules, binding modes, GUID-keyed bindings, the
  creation-type limitation, and full add/bind/list/remove workflows.

## Verification (do this before claiming success)

1. After hierarchy edits: re-read with `DescribeWidget` and confirm the expected widgets/parents.
2. After property/style edits: read the value back with `ObjectToolset.GetProperties`.
3. Compile — `BlueprintToolset.CompileBlueprint(Blueprint, bWarningsAsErrors=false)` or
   `unreal.BlueprintEditorLibrary.compile_blueprint(bp)` — then save with
   `unreal.EditorAssetLibrary.save_asset(path)` (or `AssetToolset.SaveAsset`).
4. Asset-level problems: `AssetToolset.ValidateAssets([paths])` returns per-asset errors/warnings.

Never report a widget as created/configured until it appears in a fresh `DescribeWidget` result.

## Additional gotchas

- A fresh WBP has no root: the first `CreateAndAddWidget` with no `parentWidget` becomes it.
  `BindWidget` binds only when the tree widget is `bIsVariable=True`; reading a bound member from
  Python is blocked, so enumerate `ObjectIterator(unreal.TextBlock)` filtered by the instance path.
- `bind_event` is only provably bound on the `GameInstance_*`-outered PIE instance (asset/preview
  instances always read unbound); simulate a click with `on_clicked.broadcast()` on that instance.
  Hand-built bound-event nodes compile but never fire — go through `bind_event`.
- A widget can be fully authored yet never compiled; compile a legacy widget and read the error
  first. UE 5.8 renamed `WidgetBlueprintLibrary` to `unreal.WidgetLibrary`, and `UserWidget`
  instances do not expose `get_widget_from_name`.
- Input modes: a UI-only mode drops the first WASD press after it closes, so use
  `SetInputMode_GameAndUIEx` in Construct; the input mode survives `ServerTravel`; `DefaultInput.ini`
  holds a full serialised InputSettings block mid-file, so a key appended at the section top is
  silently overridden.
- `AssetToolset.FindAssets` only sees assets **saved to disk** — a WBP you just created and have not
  saved will not be found.

---

## The 39 removed `WidgetService` tools → what to call now

Every name in the left column is **removed from `UWidgetService`**. Calling one fails.

| Removed | Use instead |
|---|---|
| `get_hierarchy`, `get_root_widget`, `list_components`, `get_component_properties`, `get_widget_snapshot`, `get_component_snapshot`, `widget_exists` | `WidgetStaticAnalysisToolset.DescribeWidget` (one call: walk-order tree + `Properties[]` + `SlotProperties[]`) |
| `add_component` | `UMGEditor.WidgetBlueprintEditorSubsystem.CreateAndAddWidget(widgetBlueprint, widgetClass, widgetDisplayName, parentWidget, childIndex)`, then `ToggleWidgetAsVariable(widgetBlueprint, widget, bIsVariable)` only if the flag must change. `widgetClass` is a class path, `parentWidget` is explicit |
| `reparent_widget`, `reorder_component` | `WidgetBlueprintEditorSubsystem.MoveWidget(widgetBlueprint, widget, newParent, childIndex)`. To reorder, pass the widget's current parent; moving **forward** within the same parent to final position N needs `childIndex = N+1` (designer drop-position semantics). Carries the old slot's layout properties across |
| `remove_component` | `WidgetBlueprintEditorSubsystem.RemoveWidget(widgetBlueprint, widget)` — removes the widget **and its children** (keep children: `MoveWidget` them first, or `ReplaceWidgetWithChild` for a single-child panel) |
| `rename_widget` | `WidgetBlueprintEditorSubsystem.RenameWidget(widgetBlueprint, widget, newDisplayName)` — validates the name and updates variable/graph/binding/animation references |
| `validate` | `AssetToolset.ValidateAssets` |
| `widget_blueprint_exists` | `AssetToolset.Exists` |
| `list_widget_blueprints` | `AssetToolset.FindAssets` (`assetType` = WidgetBlueprint) |
| `search_types` | `ObjectToolset.SearchSubclasses(BaseClass=/Script/UMG.Widget)` for native types; `AssetToolset.FindAssets` for custom WBPs |
| `get_property`, `set_property`, `list_properties` | `ObjectToolset.GetProperties` / `SetProperties` / `ListProperties` (+ `GetPropertySchema` for types) |
| `set_font`, `get_font`, `set_brush`, `get_brush` | `ObjectToolset.SetProperties` / `GetProperties` — `FSlateFontInfo` and `FSlateBrush` are plain `USTRUCT(BlueprintType)` property writes |
| `list_animations`, `create_animation`, `remove_animation` | `WidgetAnimationToolset.ListWidgetAnimations` / `CreateWidgetAnimation` / `RemoveWidgetAnimation` (also `RenameWidgetAnimation`, `SetWidgetAnimationLength`) |
| `add_animation_track`, `add_keyframe` | `WidgetAnimationToolset.AddWidgetToAnimation` → `MovieSceneBindingExtensions.AddTrack` → `MovieScenePropertyTrackExtensions.SetPropertyNameAndPath` → `MovieSceneTrackExtensions.AddSection` → `MovieSceneSectionExtensions.AddDoubleKey`/`AddFloatKey`/`AddBoolKey` (+ `SetKeyTime`) |
| `list_view_models`, `add_view_model` | `MVVMToolset.ListWidgetViewModels` / `AddViewModelToWidget` — **loses the alias name and `CreationType`**, see `mvvm.md` |
| `list_view_model_bindings`, `add_view_model_binding`, `remove_view_model_binding` | `MVVMToolset.ListWidgetViewBindings` / `CreateViewBinding` / `RemoveWidgetViewBinding` (+ `SetBindingMode`). `RemoveWidgetViewBinding` takes a **GUID**, not an index |
| `get_available_events` | `MVVMToolset.ListWidgetViewEvents` |
| `start_pie`, `stop_pie`, `is_pie_running` | `EditorAppToolset.StartPIE` / `StopPIE` / `IsPIERunning` (`StartPIE` is async) |
| `get_live_property` | `SlateInspectorToolset.Snapshot` → `GetUMGInfo(ref)` → `ObjectToolset.GetProperties` |
| `capture_preview` | `EditorAppToolset.OpenEditorForAsset` → `SlateInspectorToolset.Snapshot` → `Screenshot(ref)`. **`CaptureAssetImage` refuses Widget Blueprints** — see the caveat above |

Also gone from the pack's guidance, because the tool they described is gone: VibeUE's export-text
property path (the `"Font.Typeface"` sub-property write, the doubled-struct `get_font` readback, and
the `set_font` colour-landing quirk). `ObjectToolset.SetProperties` is JSON-schema driven and shares
none of them.

## Reference pages

Open these with the paths below; UE rewrites them to absolute paths when it serves this skill.

- [mvvm](mvvm.md) - MVVM ViewModel support on Epic's MVVMToolset — rules, binding modes, GUID-keyed bindings, the creation-type limitation, and full workflows.
- [reference](reference.md) - The 4 surviving WidgetService signatures and return-struct fields, the Epic toolset/tool index with exact toolset names, common widget properties and event names.
- [workflows](workflows.md) - Step-by-step UMG widget task workflows — create a WBP, build a menu, position on canvas, inspect, style fonts/brushes, animate, bind events, edit the hierarchy, screenshot the designer, and run PIE checks.
- [scripts/apply_brush.txt](scripts/apply_brush.txt) - runnable sample
- [scripts/apply_font.txt](scripts/apply_font.txt) - runnable sample
- [scripts/build_menu.txt](scripts/build_menu.txt) - runnable sample
- [scripts/capture_preview.txt](scripts/capture_preview.txt) - runnable sample
- [scripts/create_animation.txt](scripts/create_animation.txt) - runnable sample
- [scripts/create_widget.txt](scripts/create_widget.txt) - runnable sample
- [scripts/edit_slots.txt](scripts/edit_slots.txt) - runnable sample
- [scripts/inspect_hierarchy.txt](scripts/inspect_hierarchy.txt) - runnable sample
- [scripts/mvvm_hud.txt](scripts/mvvm_hud.txt) - runnable sample
- [scripts/pie_inspect.txt](scripts/pie_inspect.txt) - runnable sample

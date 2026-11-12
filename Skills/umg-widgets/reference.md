---
name: umg-widgets/reference
description: UMG reference — the 4 surviving WidgetService signatures and their return-struct field names, the Epic toolset index with exact toolset_name strings and tool signatures, child-order rules, common widget properties, and event names.
---

# UMG Widget Reference

Two surfaces, two call styles. See SKILL.md ▸ "Call styles" before using either.

## Contents
- VibeUE `WidgetService` — the 4 survivors
- Return structs (the only one still reachable)
- Child order
- Epic toolset index (`toolset_name` strings + signatures)
- `DescribeWidget` result shape
- Common properties
- Event names

---

## VibeUE `WidgetService` — the 4 survivors

Python, inside `execute_python`. Python names are the snake_case of the C++ method.

| Method | Signature | Returns |
|---|---|---|
| `bind_event` | `(widget_path, widget_name, event_name, function_name)` | `bool` |
| `spawn_widget_in_pie` | `(widget_path, z_order=0)` | `PIEWidgetHandle` |
| `remove_widget_from_pie` | `(handle)` | `bool` |
| `remove_view_model` | `(widget_path, view_model_name)` | `bool` |

Notes:

- `remove_view_model` **invalidates but does not delete** that ViewModel's bindings — see `mvvm.md`.

## Return structs (the only one still reachable)

Python field names drop the `b` prefix from C++ bools.

**`PIEWidgetHandle`** — from `spawn_widget_in_pie`. Pass the **whole handle object** to
`remove_widget_from_pie`, not `handle.instance_id`:

| Field | Type | Note |
|---|---|---|
| `valid` | bool | **not** `b_valid` |
| `instance_id` | str | informational |
| `error_message` | str | e.g. "PIE is not running." |

> The `FWidget*` result structs that the removed tools returned (`FWidgetAddComponentResult`, `FWidgetRemoveComponentResult`, `FWidgetInfo`,
> `FWidgetComponentSnapshot`, `FWidgetBrushInfo`, `FWidgetAnimInfo`, `FWidgetAnimKeyframe`,
> `FWidgetPreviewResult`, `FWidgetEventInfo`, `FWidgetValidationResult`, `FWidgetViewModelInfo`,
> `FWidgetViewModelBindingInfo`) have been **deleted** from `UWidgetService.h` — they were
> reflected types no surviving method could produce, so they cost UHT generation and showed up in
> the Python surface as types you could never obtain. Nothing to build around; `get_symbol` will not
> show them either.

## Child order

Panels keep their children in an ordered list, and that order is what a VerticalBox, HorizontalBox
or Overlay actually lays out. Both controls are on `UMGEditor.WidgetBlueprintEditorSubsystem`:

- `CreateAndAddWidget(..., childIndex=N)` — insert the new widget at position `N` instead of
  appending. `childIndex` defaults to `-1` (append).
- `MoveWidget(widgetBlueprint, widget, newParent, childIndex)` — move an existing widget. To
  **reorder** under its current parent, pass that parent as `newParent`. It uses designer
  drop-position semantics: moving to the front is `0`; moving **forward** to final position `N`
  within the same parent needs `N+1`. To move to a *different* parent, pass the new parent; the old
  slot's layout properties are carried across.

```python
WS = "UMGEditor.WidgetBlueprintEditorSubsystem"
WBP = "/Game/UI/WBP_Menu.WBP_Menu"
LIST = WBP + ":WidgetTree.ButtonList"
# Insert a banner above everything already in the list
vibeue.exec_tool(WS, "CreateAndAddWidget", {"widgetBlueprint": WBP, "widgetClass": "/Script/UMG.TextBlock",
                                            "widgetDisplayName": "Banner", "parentWidget": LIST, "childIndex": 0})
# ...or move an existing widget to the front
vibeue.exec_tool(WS, "MoveWidget", {"widgetBlueprint": WBP, "widget": WBP + ":WidgetTree.PlayButton",
                                    "newParent": LIST, "childIndex": 0})
```

---

## Epic toolset index

`toolset_name` is the string `call_tool` / `vibeue.exec_tool` needs. Tool names are PascalCase
exactly as written. JSON argument keys are the lowerCamelCase of the parameter name — read a tool's
schema (`describe_tools`, or `vibeue.get_toolset_schema(name)`) when in doubt instead of guessing.

| Toolset | `toolset_name` | Python-callable? |
|---|---|---|
| Widget static analysis | `WidgetStaticAnalysisToolset.WidgetStaticAnalysisToolset` | ✅ `unreal.WidgetStaticAnalysisToolset.*` |
| Widget tree edits | `UMGEditor.WidgetBlueprintEditorSubsystem` | use `vibeue.exec_tool` (`CreateAndAddWidget`, `MoveWidget`, `RemoveWidget`, `RenameWidget`, `ToggleWidgetAsVariable`) |
| Widget animations | `WidgetAnimationToolset.WidgetAnimationToolset` | ✅ `unreal.WidgetAnimationToolset.*` |
| MVVM | `MVVMToolset.MVVMToolset` | ✅ `unreal.MVVMToolset.*` |
| Properties | `EditorToolset.ObjectToolset` | ❌ `call_tool` / `exec_tool` only |
| Assets | `EditorToolset.AssetToolset` | ❌ `call_tool` / `exec_tool` only |
| Editor app / PIE | `EditorToolset.EditorAppToolset` | ❌ `call_tool` / `exec_tool` only |
| Live Slate UI | `SlateInspectorToolset.SlateInspectorToolset` | ❌ `call_tool` / `exec_tool` only |
| Blueprint graphs / compile | `BlueprintToolsets.BlueprintToolset` | ✅ `unreal.BlueprintToolset.*` |
| Sequencer bindings | `SequencerScripting.MovieSceneBindingExtensions` | ✅ `unreal.MovieSceneBindingExtensions.*` |
| Sequencer tracks | `SequencerScripting.MovieSceneTrackExtensions` | ✅ |
| Sequencer property tracks | `SequencerScripting.MovieScenePropertyTrackExtensions` | ✅ |
| Sequencer sections / keys | `SequencerScripting.MovieSceneSectionExtensions` | ✅ |

### `WidgetStaticAnalysisToolset`

| Tool | Signature |
|---|---|
| `DescribeWidget` | `(WidgetBlueprint, StartWidget=null, MaxDepth=-1, PropertyNameFilter="", bSkipDefaultValues=true, bIncludeFullDescription=false, bIncludeComposedColors=false)` → `FWidgetDescriptionResult` |
| `GetWidgetTreeDepth` | `(WidgetBlueprint, StartWidget=null)` → int32 (root-only = 0; `-1` on error / no root) |
| `TraceWidgetPropertyOverrides` | `(WidgetBlueprint, PropertyNameFilter="", WidgetNameFilter="")` → `{AvailablePropertyCount, Chains[], Winners[], Flags[]}`. Both filters are **exact** matches |
| `GetWidgetConnectionMap` | `(WidgetBlueprint, ScopeFilter="")` → `{AvailableConnectionCount, Connections[], ConnectedCount, UnconnectedCount, MisconfiguredCount}` |
| `GetViewModelConnectionMap` | `(ViewModelClass, ScopeFilter="")` → `{AvailableConnectionCount, AvailableWidgetCount, Widgets[]}` |

`DescribeWidget` reports **designer-time state stored in the asset**. Anything set at runtime (a
Blueprint graph writing a slot through `SlotAsCanvasSlot`, style state that is not a UPROPERTY) does
not appear — use `TraceWidgetPropertyOverrides` for graph writes.

### `WidgetAnimationToolset`

| Tool | Signature |
|---|---|
| `CreateWidgetAnimation` | `(WidgetBlueprint, AnimationName, LengthSeconds=1.0)` → `FWidgetAnimationInfo`; `Animation == null` means failure |
| `ListWidgetAnimations` | `(WidgetBlueprint)` → `FWidgetAnimationInfo[]` |
| `FindWidgetAnimation` | `(WidgetBlueprint, AnimationName)` → `FWidgetAnimationInfo` (empty when not found) |
| `RenameWidgetAnimation` | `(WidgetBlueprint, Animation, NewName)` → bool |
| `SetWidgetAnimationDisplayLabel` | `(Animation, DisplayLabel)` → bool |
| `SetWidgetAnimationLength` | `(Animation, LengthSeconds)` → bool; must be > 0 |
| `RemoveWidgetAnimation` | `(WidgetBlueprint, Animation)` → bool |
| `AddWidgetToAnimation` | `(WidgetBlueprint, Animation, ObjectToBind)` → `FMovieSceneBindingProxy` |
| `GetWidgetAnimationBindings` | `(Animation)` → `FMovieSceneBindingProxy[]` |
| `RemoveWidgetBinding` | `(Animation, BindingGuid)` → bool |

`FWidgetAnimationInfo`: `Animation` (the `UWidgetAnimation` — pass it to `AddWidgetToAnimation` and
to every SequencerScripting tool that accepts a `UMovieSceneSequence`), `AnimationName` (internal
name, also the Blueprint variable name for runtime playback), `DisplayLabel`, `LengthSeconds`,
`BindingCount`.

`ObjectToBind` must be a `UWidget` in the tree, or a `UPanelSlot` whose `Content` is non-null (to
animate slot properties). The WBP needs a compiled `GeneratedClass`; the implementation compiles it
if needed.

### SequencerScripting — tracks, sections, keys

`UWidgetAnimation` extends `UMovieSceneSequence`, so these work on widget animations unmodified.

| Tool | Signature |
|---|---|
| `MovieSceneBindingExtensions.AddTrack` | `(Binding, TrackType)` → `UMovieSceneTrack` |
| `MovieScenePropertyTrackExtensions.SetPropertyNameAndPath` | `(Track, PropertyName, PropertyPath)` |
| `MovieSceneTrackExtensions.AddSection` | `(Track)` → `UMovieSceneSection` |
| `MovieSceneSectionExtensions.GetNumericChannels` | `(Section)` → `FMovieSceneNumericChannelDescriptor[]` (float channels then double channels) |
| `MovieSceneSectionExtensions.AddFloatKey` | `(Channel, InTime, NewValue, SubFrame=0, TimeUnit=DisplayRate, Interpolation=SmartAuto)` → key descriptor |
| `MovieSceneSectionExtensions.AddDoubleKey` | same shape, `double` value |
| `MovieSceneSectionExtensions.AddBoolKey` | `(Section, ChannelName, Frame, Value)` — section + channel **name**, not a descriptor |
| `MovieSceneSectionExtensions.SetKeyTime` | `(Channel, Key, NewFrameNumber, SubFrame=0, TimeUnit=DisplayRate)` |
| `MovieSceneSectionExtensions.GetChannelNames` / `GetChannelKeys` | `(Section)` / `(Section, ChannelName)` — readback |

`FMovieSceneNumericChannelDescriptor`: `Section`, `ChannelType` (`Float`/`Double`), `ChannelIndex`,
`ChannelName`. `InTime` is in **display-rate** frames unless `TimeUnit=TickResolution`.

### `ObjectToolset`

| Tool | Signature |
|---|---|
| `ListProperties` | `(Instance, Root="", NameFilter="", MaxDepth=1)` → `string[]` dotted paths |
| `GetPropertySchema` | `(Instance, Root="", MaxDepth=1)` → JSON Schema string |
| `GetProperties` | `(Instance, Properties[])` → JSON string, keyed by the path as written |
| `SetProperties` | `(Instance, Values)` → bool. **`Values` is a JSON *string***, so `json.dumps({...})` |
| `ResetProperties` | `(Instance, Properties[])` → bool — drops per-instance overrides |
| `SearchSubclasses` | `(BaseClass, ClassName="")` → `FSoftClassPath[]` |
| `GetClass` | `(Instance)` → `UClass` (resolves a Blueprint to its generated class) |

`MaxDepth` defaults to **1** on `ListProperties`/`GetPropertySchema` so an agent cannot dump a whole
object tree by accident; pass 0 for every level. `ListProperties`' `NameFilter` is a **regex**
(case-insensitive) searched in the name and Keywords metadata. An object reference is a **leaf** —
to list what it points at, pass that object as `Instance`. Container brackets report live occupancy
(`[0..2]`, `["Alpha","Beta"]`, `[]`, `[?]`); narrow to one entry before using the path in
`GetProperties`/`SetProperties`.

`Instance` for a widget inside a WBP is `"<pkg>.<Asset>:WidgetTree.<WidgetName>"`. Passing the
**WidgetBlueprint asset** instead resolves to its generated class's CDO — that edits the
`UserWidget` itself.

### `AssetToolset`

| Tool | Signature |
|---|---|
| `FindAssets` | `(Filter)` → async, object paths. Filter: `folderPath`, `name` (case-insensitive contains), `assetType` (a UClass), `bRecursive`, `tags`, `maxResults` (default 100, 0 = no limit) |
| `Exists` | `(Path)` → bool |
| `ValidateAssets` | `(AssetPaths[])` → `FAssetValidationResult` with per-asset errors and warnings |
| `SaveAsset` / `LoadAsset` / `IsDirty` | `(AssetPath)` |

`FindAssets` only sees assets **saved to disk**; an in-memory asset is not found. An empty
`folderPath` searches the whole project including plugin content.

### `EditorAppToolset`

| Tool | Signature |
|---|---|
| `StartPIE` | `(Options?)` → async void. **PIE is not live in the same call** |
| `StopPIE` | `()` → async void |
| `IsPIERunning` | `()` → bool |
| `OpenEditorForAsset` | `(AssetPath)` |
| `GetAssetThumbnails` | `(AssetPaths[])` → async, one image per path in request order; an empty image where an asset has no cached thumbnail. Does not load or render the asset |
| `CaptureEditorImage` | `()` → `FToolsetImage` of the whole editor window |
| `CaptureAssetImage` | `(AssetPath)` → async image. **Refuses Widget Blueprints** — allowlist is AnimationAsset, Skeleton, StaticMesh, SkeletalMesh, Texture, MaterialInterface; anything else raises "Asset type does not support image capture" |

### `SlateInspectorToolset`

| Tool | Signature |
|---|---|
| `Snapshot` | `(Ref, MaxDepth=30, bIncludeSourceLocations=false)` → text tree of refs. `Ref=""` = all windows. Each `[pos=X,Y size=W,H]` is desktop pixels |
| `GetUMGInfo` | `(Ref)` → `{Error, Widget, WidgetName, WidgetClass, Asset, ResolvedVia, OuterUserWidgets[]}`. `Widget` is a live handle serialized as `{refPath}` — feed it to `ObjectToolset`. `Widget` is non-null exactly when `Error` is empty |
| `Screenshot` | `(Ref)` → `FToolsetImage`, cropped to that Slate widget. `Ref=""` = active window. Fails (with the reason) on an unknown ref or a widget in no visible window |
| `Observe` / `Unobserve` | `(Ref, MaxDepth=30)` → identifier / `(Identifier)` — keep a subtree's refs fresh (~100ms tick) before working in it |
| `Click` / `Hover` / `Type` / `Focus` / `Windows` | live input simulation; see the toolset's own schema |

Refs from an earlier `Snapshot` stay usable — you do not need a fresh `Snapshot` before every
action. `ResolvedVia` is `"self"` when the Slate widget itself carried UMG metadata, `"parent"` when
an ancestor did.

### `BlueprintToolset`

| Tool | Signature |
|---|---|
| `AddFunctionGraph` | `(Blueprint, GraphName, Flags?)` → `UEdGraph` — create the function `bind_event` will target |
| `CompileBlueprint` | `(Blueprint, bWarningsAsErrors=false)` |
| `GetGraph` | `(Blueprint, GraphName)` → `UEdGraph` |

> `AddComponentBoundEvent` is typed `UActorComponent*` and cannot bind a `UWidget` event — that is
> why `unreal.WidgetService.bind_event` survives.

---

## `DescribeWidget` result shape

`FWidgetDescriptionResult`:

| Field | Type | Description |
|---|---|---|
| `AvailableWidgetCount` | int32 | widgets within `StartWidget`/`MaxDepth`'s bound; the canary if `Widgets` got cut |
| `Description` | str | verbose per-property text tree, only when `bIncludeFullDescription=true`. Line `[N]` matches `Widgets[N]` |
| `Widgets` | `FWidgetDescriptionEntry[]` | flat **walk order**, always populated |

`FWidgetDescriptionEntry`:

| Field | Type | Description |
|---|---|---|
| `Widget` | `UWidget` | the live instance |
| `Parent` | `UPanelWidget` | null for the root **and** for named-slot content |
| `WidgetClassPath` | `FSoftClassPath` | e.g. `/Script/UMG.Button` |
| `WidgetName` | `FName` | |
| `bIsVariable` | bool | exposed as a Blueprint variable |
| `Depth` | int32 | 0 at `StartWidget`, +1 per level (panel child or named-slot content alike) |
| `NamedSlotHost` | `UWidget` | set when this widget is content in a named slot on that host |
| `bInherited` | bool | the name was already a widget property on the parent class |
| `UIComponents` | `FWidgetUIComponentInfo[]` | attached UI components, in display order |
| `Properties` | `FWidgetPropertyEntry[]` | `{PropertyName, ExportedValue}` |
| `SlotProperties` | `FWidgetPropertyEntry[]` | same shape, for the widget's `Slot` object |
| `ComposedColors` | `FWidgetComposedColorEntry[]` | only with `bIncludeComposedColors=true` |

Hierarchy reading rule: `Parent` set → child of that panel. `NamedSlotHost` set → named-slot content
on that host. **Both null** → the root widget, *or* content in a slot inherited from a parent class.

`FWidgetComposedColorEntry`: `State` (`Normal`/`Hovered`/`Pressed`/`Disabled`), `bResolved`,
`ComposedLinearColor` (meaningful only when `bResolved` — otherwise opaque black),
`CompositionFactors[]` (ordered, always populated), `Gaps[]` (one per unresolved factor).
Non-interactable widget kinds (Border, Image, Text) report a single `Normal` entry.

---

## Common properties

Values are JSON now, matching the property's real type — confirm with `GetPropertySchema`.

| Widget | Property | Example JSON value |
|--------|----------|--------------------|
| TextBlock | `Text` | `"Hello World"` |
| TextBlock | `Font.Size` | `24` |
| TextBlock | `ColorAndOpacity` | `{"SpecifiedColor": {"R":1,"G":1,"B":1,"A":1}, "ColorUseRule": "UseColor_Specified"}` |
| TextBlock | `ShadowColorAndOpacity` | `{"R":0,"G":0,"B":0,"A":0.75}` (plain FLinearColor) |
| Button | `BackgroundColor` | `{"SpecifiedColor": {"R":0,"G":0.5,"B":1,"A":1}, "ColorUseRule": "UseColor_Specified"}` |
| Image | `Brush.DrawAs` | `"RoundedBox"` |
| Image | `Brush.TintColor` | `{"SpecifiedColor": {"R":1,"G":1,"B":1,"A":0.5}, "ColorUseRule": "UseColor_Specified"}` |
| ProgressBar | `Percent` | `0.75` |
| Any | `Visibility` | `"Visible"` / `"Hidden"` / `"Collapsed"` |
| Any | `RenderOpacity` | `1.0` |
| SizeBox | width override | `{"bOverride_WidthOverride": true, "WidthOverride": 34.0}` — both keys, always |

Slot properties live on the slot object, not the widget — Canvas: `Anchors` (`Minimum`/`Maximum`),
`Offsets` (Margin), `Alignment` (pivot), `ZOrder`, `bAutoSize`. Box/Overlay: `Padding`,
`HorizontalAlignment`, `VerticalAlignment`, and on box slots `Size` (`SizeRule` + `Value`).

## Event names

For `unreal.WidgetService.bind_event(path, widget_name, event_name, function_name)`:

- `OnClicked` — Button clicked
- `OnPressed` / `OnReleased` — Press/release
- `OnHovered` / `OnUnhovered` — Mouse enter/leave

`MVVMToolset.ListWidgetViewEvents(WidgetBlueprint)` lists the multicast-delegate event property
paths the MVVM view can bind on that Blueprint.

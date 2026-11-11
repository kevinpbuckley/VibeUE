---
name: blueprints
display_name: Blueprint System
description: Blueprint asset, variable, function, component, dispatcher and interface authoring is Epic's in UE 5.9 — `BlueprintToolsets.BlueprintToolset` owns Create/CompileBlueprint/variables/function graphs/events/dispatchers/interfaces, `BlueprintToolsets.BlueprintDslToolset` owns graph bodies, `EditorToolset.ActorToolset` owns components on a Blueprint and `EditorToolset.ObjectToolset` owns every CDO property. VibeUE keeps only Timelines, macro-graph creation (CreateMacroGraph), layout measurement (AnalyzeGraphLayout) and RefreshBlueprintEditor. Use when the user asks to create a Blueprint/BP, add a variable or component, make an event dispatcher/delegate, implement a Blueprint interface, add a Timeline, or inspect a Blueprint's class and components. For node-level graph wiring, also load blueprint-graphs.
vibeue_classes:
  - BlueprintService
  - AssetDiscoveryService
unreal_classes:
  - EditorAssetLibrary
  - BlueprintDslToolset
  - BlueprintToolset
keywords:
  - blueprint
  - create blueprint
  - variable
  - component
  - compile
  - introspection
  - event dispatcher
  - dispatcher
  - delegate
  - multicast
  - broadcast
  - call delegate
  - interface
  - blueprint interface
  - implement interface
  - add interface
related_skills:
  - blueprint-graphs
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "blueprint-fundamentals"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Blueprint System Skill

> **For graph bodies** — statements, wires, pin values, layout, Timelines — load `blueprint-graphs`.

## Who owns what in 5.9

`BlueprintService` went from 96 AI tools to **19**, and none of the 19 is about the class itself.

| Job | Tool |
|---|---|
| Create a Blueprint asset, compile it, reparent it | `BlueprintToolsets.BlueprintToolset` — `Create`, `CompileBlueprint`, `GetParent`, `SetParent`, `GetDefaultObject` |
| Variables and their flags | same toolset — `AddVariable`, `AddObjectVariable`, `AddStructVariable`, `ListVariables`, `RemoveVariable`, `Get`/`SetVariableCategory` / `Tooltip` / `ReadOnly` / `Replication`, `SetVariableInstanceEditable` |
| Function graphs, their params and detail-panel flags | same toolset — `AddFunctionGraph`, `RemoveFunctionGraph`, `ListFunctions`, `AddFunctionParam`, `AddStructFunctionParam`, `AddObjectFunctionParam`, `RemoveFunctionParam`, `Get`/`SetFunctionFlags` |
| Events, overrides, dispatchers, delegates | same toolset — `AddEvent`, `ListEvents`, `AddEventDispatcher`, `ListEventDispatchers`, `AddComponentBoundEvent`, `ListComponentEvents`, `AddMatchingDelegateEvent`/`Function`, `ListCompatibleEventFunctions`, `Get`/`SetCreateEventFunction`, `GetDelegateSignature` |
| Interfaces | same toolset — `AddInterface`, `RemoveInterface`, `ListInterfaces` |
| **Graph bodies** | `BlueprintToolsets.BlueprintDslToolset` — see `blueprint-graphs` |
| **Components on a Blueprint** | `EditorToolset.ActorToolset` — `AddComponent`, `RemoveComponent`, `GetComponents`, `GetRootComponent`, `GetParentComponent`, `SetParentComponent`, `Get`/`SetComponentTransform` |
| **Any property, on the CDO or a component template** | `EditorToolset.ObjectToolset` — `ListProperties`, `GetPropertySchema`, `GetProperties`, `SetProperties`, `ResetProperties`, `SearchSubclasses` |
| Find / save / validate assets | `EditorToolset.AssetToolset` — `FindAssets`, `Exists`, `SaveAsset`, `ValidateAssets` |
| What is open and selected | `EditorToolset.EditorAppToolset` — `GetAssetEditorSelection`, `GetOpenAssets`, `OpenEditorForAsset` |
| **Timelines** | `unreal.BlueprintService.*timeline*` (VibeUE — no engine tool touches a `UTimelineTemplate`) |
| **Macro graph on a Macro Library** | `unreal.BlueprintService.create_macro_graph` (VibeUE — the DSL refuses macro libraries) |
| **Measure a graph's layout** | `unreal.BlueprintService.analyze_graph_layout` (VibeUE — Epic's layout tools only write) |
| **Redraw an open Blueprint editor** | `unreal.BlueprintService.refresh_blueprint_editor` (VibeUE) |

## Critical Rules

### ⚠️ The toolset name `editor_toolset.toolsets.blueprint.BlueprintTools` does not exist

Older revisions of this pack used that string, with lowercase tool names like `create` and
`add_variable`. There is no such toolset and there are no such tools. The real names are
**`BlueprintToolsets.BlueprintToolset`** and **`BlueprintToolsets.BlueprintDslToolset`**, and their
tools are **PascalCase** (`Create`, `AddVariable`, `CompileBlueprint`) because they are C++
toolsets.

### ⚠️ Call styles

**1. VibeUE's 19 survivors are Python**, inside `execute_python`:

```python
import unreal
unreal.BlueprintService.get_timelines("/Game/Blueprints/BP_Player")
```

**2. Epic's toolsets are `call_tool`** — one tool per MCP round-trip:

```
call_tool(tool_name="AddVariable",
          toolset_name="BlueprintToolsets.BlueprintToolset",
          arguments={"blueprint": {"refPath": "/Game/BP/BP_Player.BP_Player"},
                     "name": "Health", "typeName": "float"})
```

**3. Inside `execute_python`, `vibeue.exec_tool`** — the same dispatch in-process, so a whole task
is one round-trip. It fills missing optional args from the tool schema, unwraps `returnValue` and
raises `RuntimeError` on a tool error. This is what the sample scripts use:

```python
import vibeue
BT = "BlueprintToolsets.BlueprintToolset"
vibeue.exec_tool(BT, "AddVariable", {"blueprint": {"refPath": "/Game/BP/BP_Player.BP_Player"},
                                     "name": "Health", "typeName": "float"})
```

### ⚠️ Object arguments are `{refPath}` handles, and the path must be the FULL object path

Every UObject/UClass argument (`blueprint`, `assetType`, `graph`, `parentClass`, `interfaceClass`,
`instance`, `component`…) is a reference object `{"refPath": "<object path>"}`, and these tools
**return** the same shape. A plain string fails with
`"<x> is not a valid object path for property '<arg>'"`.

The path is `/Game/Dir/Asset.Asset` — the asset name repeated after the dot. A bare **package** path
(`/Game/Dir/Asset`, no `.Asset`) is rejected. Two safe ways to get one:

```python
# (a) Reuse what a tool returned — it is already a {refPath} object:
bp = vibeue.exec_tool(BT, "Create", {"folderPath": "/Game/Blueprints", "assetName": "BP_Player",
                                     "assetType": {"refPath": "/Script/Engine.Character"}})

# (b) Build one from a package path you already have:
def ref(p):
    tail = p.rsplit("/", 1)[-1]
    return {"refPath": p if "." in tail else "{}.{}".format(p, tail)}
```

**Chain handles rather than rebuilding them.** `AddFunctionGraph` returns the graph, and
`AddFunctionParam` takes a graph; `AddEventDispatcher` returns the dispatcher's signature graph, and
`AddFunctionParam` adds its parameters; `GetDefaultObject` returns the CDO, and `ObjectToolset` takes
an instance. A Blueprint's class is `/Game/Dir/BP_X.BP_X_C`, distinct from the asset
`/Game/Dir/BP_X.BP_X` — use the `_C` form only where a **class** is wanted.

VibeUE's own `unreal.BlueprintService.*` methods are unaffected: they take plain string paths.

### ⚠️ Nothing works during Play-In-Editor

Every `BlueprintService` entry point **refuses** while a PIE session is running and logs it with a
`PIE_ACTIVE:` prefix. Asset writes during PIE are unsafe anyway, and editor-world edits issued
immediately after stopping PIE can silently no-op — let the editor settle before editing.

```python
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not les.is_in_play_in_editor(), "stop PIE before editing a Blueprint"
```

`EditorAppToolset.IsPIERunning` / `StopPIE` are the engine-side equivalents.

### Level Blueprints: pass the MAP path

Every `BlueprintService` `BlueprintPath` parameter also accepts a **map/world path** (e.g.
`/Game/Maps/MainMenu`) and resolves it to that level's **Level Blueprint** — so a Timeline or a
layout audit on a level script works exactly like a normal Blueprint. Explicit subobject paths
(`/Game/Maps/MyMap.MyMap:PersistentLevel.MyMap`) work too. The **MAP** asset is what you save
afterwards (`unreal.EditorAssetLibrary.save_asset("/Game/Maps/MyMap")`).

### ⚠️ Compile after every structural change

A variable only exists on the compiled **skeleton** class after a compile, and an added
function/dispatcher/interface is only fully real afterwards. `AddInterface`'s own doc says it:
"The Blueprint must be recompiled for the change to take effect." The order is always
**add structure → `CompileBlueprint` → author graphs → `CompileBlueprint` → save**, and never save
on a compile error.

---

## Create a Blueprint

```python
import json, vibeue
BT = "BlueprintToolsets.BlueprintToolset"      # the snippets below also use:
OT = "EditorToolset.ObjectToolset"             #   properties
AT = "EditorToolset.ActorToolset"              #   components
# `ref()` is the {refPath} helper from the section above.

bp = vibeue.exec_tool(BT, "Create", {"folderPath": "/Game/Blueprints",
                                     "assetName": "BP_Player",
                                     "assetType": {"refPath": "/Script/Engine.Character"}})
# -> {"refPath": "/Game/Blueprints/BP_Player.BP_Player"} — reuse this handle
```

`folderPath` and `assetName` are **separate strings** — do not jam a full asset path into one.
`assetType` is the parent **class** as a `{refPath}`: `/Script/Engine.Actor`,
`/Script/Engine.Pawn`, `/Script/Engine.Character`, or another Blueprint's generated class
`/Game/.../BP_Base.BP_Base_C`. `EditorToolset.ObjectToolset.SearchSubclasses` finds a parent class
when you only know part of its name.

`GetParent` / `SetParent` reparent an existing Blueprint afterwards.

## Variables

Three tools, by what the type is:

| Tool | For |
|---|---|
| `AddVariable(Blueprint, Name, TypeName, Options)` | primitives and the basic structs, by **string** |
| `AddStructVariable(Blueprint, Name, StructType, Options)` | **any** `UScriptStruct` — custom structs and engine structs outside the basic list (`HitResult`, `GameplayTag`, `FStateTreeDelegateDispatcher`…) |
| `AddObjectVariable(Blueprint, Name, ObjectClass, Options)` | an object/class reference |

**`AddVariable`'s supported `TypeName` strings**, from its own documentation: primitives `bool`,
`int`, `float`, `byte`, `string`, `name`, `text`; structs `Vector`, `Rotator`, `Transform`,
`Vector2D`, `LinearColor`; **and the bare name or object path of any `UEnum` or `UScriptStruct`**.
Use `int` (not `int32`) and `float` (not `double`). Because that last clause covers any
`UScriptStruct` or `UEnum`, a struct type such as `StateTreeDelegateDispatcher` or
`/Script/Engine.HitResult` is accepted here too — `AddStructVariable` is the unambiguous route
rather than the only one. An **object or class reference** is the case `AddVariable` genuinely does
not cover: that is `AddObjectVariable`.

> An earlier revision of this pack claimed the engine's `add_variable` rejects
> `FStateTreeDelegateDispatcher` with "Unknown type", live-verified. That was measured against
> `editor_toolset.toolsets.blueprint.BlueprintTools.add_variable`, a tool that does not exist.
> 5.9's `BlueprintToolset.AddVariable` documents struct names and paths as supported, so do not
> carry the old assumption forward — if a type string is refused, the error names the supported
> set.

`Options` (`FBlueprintAddVariableOptions`) is one struct on all three, every field optional:

| Field | Effect |
|---|---|
| `graph` | add a **local** variable to that function graph instead of a member |
| `containerType` | array / set / map — **a MAP uses `FString` as its key type** |
| `tooltip` | editor tooltip |
| `category` | My Blueprint panel category |
| `bReadOnly` | Blueprint Read Only (graphs read but cannot write). Member variables only |

```python
bp = ref("/Game/Blueprints/BP_Player")

vibeue.exec_tool(BT, "AddVariable", {"blueprint": bp, "name": "Health", "typeName": "float"})
vibeue.exec_tool(BT, "AddVariable", {"blueprint": bp, "name": "Waypoints", "typeName": "Vector",
                                     "options": {"containerType": "ARRAY",
                                                 "category": "Patrol"}})
vibeue.exec_tool(BT, "AddObjectVariable", {"blueprint": bp, "name": "DeathFX",
                                           "objectClass": {"refPath": "/Script/Niagara.NiagaraSystem"}})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
```

A variable of a **Blueprint class** type is `AddObjectVariable` with that Blueprint's generated
class: `{"refPath": "/Game/Blueprints/BP_Enemy.BP_Enemy_C"}`.

### Reading variables back

`ListVariables(Blueprint, Scope, Graph, bIncludeInherited)` returns `FBlueprintVariableInfo` —
**just `name` and `writable`**, one entry per variable a graph may read. It is not the old
`BlueprintVariableDetailedInfo`; there is no type, category or default on it. For those:

- type and default → `ObjectToolset.GetPropertySchema` / `GetProperties` on the CDO
  (`GetDefaultObject`)
- category / tooltip / read-only / replication → the matching `GetVariable*` tools
- `Scope` is `MEMBER` when unset (inherited members only with `bIncludeInherited`); `LOCAL`
  **requires** `Graph`, and the other scopes refuse it.

### Variable default values live on the CDO

There is no `set_variable_default_value`. A Blueprint's defaults are properties on its Class Default
Object:

```python
cdo = vibeue.exec_tool(BT, "GetDefaultObject", {"blueprint": bp})
vibeue.exec_tool("EditorToolset.ObjectToolset", "SetProperties",
                 {"instance": cdo, "values": json.dumps({"Health": 100.0, "bReplicates": True})})
```

## Function graphs

```python
g = vibeue.exec_tool(BT, "AddFunctionGraph", {"blueprint": bp, "graphName": "TakeDamage"})
vibeue.exec_tool(BT, "AddFunctionParam", {"graph": g, "paramName": "Amount",
                                          "paramType": "float", "inputParam": True})
vibeue.exec_tool(BT, "AddFunctionParam", {"graph": g, "paramName": "NewHealth",
                                          "paramType": "float", "inputParam": False})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
```

- `AddFunctionGraph` is **idempotent**, and **naming an inherited overridable function makes the new
  graph an override of it** — that is how a function-style override is taken now. For an
  event-style (void/latent) override, `AddEvent` with the member's name puts the event node in the
  EventGraph instead. Together they replace `override_function`.
- **"What can I override?"** is `ListFunctions` / `ListEvents`: both list local members **plus
  inheritable ones from the parent chain and implemented interfaces**, and each entry's
  `is_implemented` is the old `already_overridden` flag. Which of the two lists a member turns up in
  is what tells you whether to take it with `AddFunctionGraph` or `AddEvent`. That is the
  replacement for `list_overridable_functions`; `ListCompatibleEventFunctions` answers a different
  question (what a `CreateDelegate` node can bind to).
- `AddFunctionParam` takes the **graph**, not the Blueprint, and `bInputParam` decides input vs
  output. Supported `ParamType` strings are the same set `AddVariable` accepts; use
  `AddStructFunctionParam` / `AddObjectFunctionParam` for anything else.
- Detail-panel settings (access specifier, pure, const, call-in-editor…) are
  `Get`/`SetFunctionFlags`; unset fields on a `Set` keep the current value.
- The body is a `BlueprintDslToolset.Write` with a `(fn …)` entry — see `blueprint-graphs`.

## Components on a Blueprint

Components are `EditorToolset.ActorToolset`, and the same tools work on a Blueprint asset and on a
placed actor instance.

```python
# AddComponent's Owner takes "the actor blueprint, instance, or SceneComponent to add to".
body = vibeue.exec_tool(AT, "AddComponent", {"owner": bp, "name": "BodyMesh",
                                             "componentType": {"refPath": "/Script/Engine.StaticMeshComponent"}})
glow = vibeue.exec_tool(AT, "AddComponent", {"owner": body, "name": "Glow",
                                             "componentType": {"refPath": "/Script/Engine.PointLightComponent"}})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
```

- **Pass the parent component as `owner`** to nest, or reparent afterwards with `SetParentComponent`.
- **`SetParentComponent` is how you change the scene root.** Passing a component as the parent of
  the current root *promotes it to the scene root* and makes the old root its child; an
  auto-generated `DefaultSceneRoot` is removed by Unreal. Passing `None` as the parent detaches.
  This is the replacement for `set_root_component` / `reparent_component`.
- `GetComponents(Actor, ComponentType?)`, `GetRootComponent`, `GetParentComponent`,
  `RemoveComponent`, `Get`/`SetComponentTransform` round out the set.

### ✅ `GetComponents` on a Blueprint's CDO *does* enumerate the SCS

The old warning that CDO component enumeration comes back empty applies only to
`K2_GetComponentsByClass` in raw Python. `ActorToolset.GetComponents` detects a Blueprint actor and
switches to `SubobjectDataSubsystem` (`ActorToolset.cpp:497-519`), so handing it the Blueprint's CDO
lists the SCS-added components:

```python
cdo  = vibeue.exec_tool(BT, "GetDefaultObject", {"blueprint": bp})
comps = vibeue.exec_tool(AT, "GetComponents", {"actor": cdo})
```

### Component properties go through `ObjectToolset`

There is no `set_component_property`. A component template is just an object, so:

```python
print(vibeue.exec_tool(OT, "ListProperties", {"instance": glow, "nameFilter": "intensity|color"}))
print(vibeue.exec_tool(OT, "GetPropertySchema", {"instance": glow, "root": "LightColor"}))
vibeue.exec_tool(OT, "SetProperties", {"instance": glow, "values": json.dumps(
    {"Intensity": 5000.0, "LightColor": {"r": 255, "g": 127, "b": 0, "a": 255}})})
```

Workflow: **`ListProperties` to find the path → `GetPropertySchema` for its type → `GetProperties` /
`SetProperties`.** `ListProperties` defaults to `MaxDepth` 1 so an agent cannot dump a whole object
tree by accident; a container's bracket shows its live occupancy (`[0..2]`, `["Alpha","Beta"]`,
`[]`, `[?]`), and a path must be narrowed to one element before `GetProperties` / `SetProperties`.
An object reference is a **leaf** — to list what it points at, pass that object as `instance`.
`ResetProperties` drops a per-instance override back to the default.

### ⚠️ `FColor` is bytes 0–255; `FLinearColor` is floats 0–1

| Property type | Value |
|---|---|
| `float` / `int32` / `bool` | `5000.0`, `25`, `true` |
| `FVector` / `FRotator` | `{"x": 0, "y": 0, "z": 50}`, `{"pitch": 0, "yaw": 90, "roll": 0}` |
| **`FColor`** (e.g. a light's `LightColor`) | `{"r": 255, "g": 127, "b": 0, "a": 255}` — **integer bytes** |
| **`FLinearColor`** | `{"r": 1.0, "g": 0.5, "b": 0.0, "a": 1.0}` — **floats 0–1** |
| Enum (e.g. `IntensityUnits`) | `"Candelas"` |

❌ The classic bug: `LightColor` is an `FColor`, so LinearColor-style floats
(`{"r": 1, "g": 0.5, "b": 0}`) give a nearly-black light (R=1 of 255), not orange. Check the type
with `GetPropertySchema` before guessing — `SetProperties` refuses a value outside the bounds its
schema advertises, but 1-of-255 is in bounds.

Collision lives in `BodyInstance`, which `SetProperties` reaches by dotted path
(`"BodyInstance.MassInKgOverride"`, `"BodyInstance.CollisionProfileName"`). Engine collision profiles
predate custom trace channels, so a channel with no explicit entry falls back to the ini default
response (**BLOCK**) — a scripted volume or primitive must set each custom channel's response
explicitly.

## Event dispatchers (multicast delegates)

A dispatcher is **not** a variable — do not try to add one with `AddVariable`.

```python
sig = vibeue.exec_tool(BT, "AddEventDispatcher", {"blueprint": bp, "name": "OnDied"})
# AddEventDispatcher returns the dispatcher's SIGNATURE GRAPH. Its parameters are
# AddFunctionParam on that graph — inputs only; outputs are not supported on a dispatcher.
vibeue.exec_tool(BT, "AddFunctionParam", {"graph": sig, "paramName": "Killer",
                                          "paramType": "Actor", "inputParam": True})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
print(vibeue.exec_tool(BT, "ListEventDispatchers", {"blueprint": bp}))
```

- **Removing one is `RemoveFunctionGraph(Blueprint, GraphName)`** — its documentation covers "a
  function graph **or event dispatcher**". There is no `RemoveEventDispatcher`.
- **Broadcasting is a DSL statement**, not a node-placement call. `add_call_delegate_node` is gone:

```
(event `self.ReceiveAnyDamage`
  (`self.OnDied` (`self.GetInstigator`)))      ; broadcast, with the signature's parameter
```

- **Subscribing** is `(add …)` with a `|CreateDelegate` handler, or `AddComponentBoundEvent` /
  `AddMatchingDelegateEvent` for the imperative route:

```
(event `HandleDied` :!signature `self.OnDied`
  (params (Killer :type "Actor Object Reference")))

(event `self.ReceiveBeginPlay`
  (add `self.OnDied` (`|CreateDelegate` :!event `HandleDied`)))
```

`` (remove `self.OnDied` handler) `` unsubscribes and `` (clear `self.OnDied`) `` drops every
listener.
`GetDelegateSignature` / `ListCompatibleEventFunctions` / `Get`/`SetCreateEventFunction` are the
tools for inspecting and retargeting an existing `CreateDelegate` node.

For a **component's** delegate (a button's `OnClicked`, a collider's
`OnComponentBeginOverlap`), the tools are `ListComponentEvents(Component)` then
`AddComponentBoundEvent(Component, EventName, Graph)` — a component-bound event must be created
atomically, because its generated handler name is part of the binding. In the DSL it reads as
`` (event `self.MeshComp.OnComponentHit` …) ``.

## Blueprint interfaces

```python
bpi = ref("/Game/Interfaces/BPI_Interactable")       # the interface ASSET
cls = {"refPath": "/Game/Interfaces/BPI_Interactable.BPI_Interactable_C"}   # its CLASS

vibeue.exec_tool(BT, "AddInterface", {"blueprint": bp, "interfaceClass": cls})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
print(vibeue.exec_tool(BT, "ListInterfaces", {"blueprint": bp, "includeInherited": True}))
```

- `interfaceClass` is a **class** handle (`..._C`), not the asset path — this is the most common
  mistake here.
- `AddInterface` is **idempotent**; adding one the Blueprint already implements is a no-op. Its
  function-shape members become function graphs on the Blueprint and its event-shape members become
  overridable events. **The Blueprint must be recompiled for the change to take effect.**
- `RemoveInterface(Blueprint, InterfaceClass, bPreserveFunctions)` — `bPreserveFunctions=True` keeps
  the implemented graphs as ordinary Blueprint functions instead of deleting them. Recompile
  afterwards.
- `ListInterfaces(Blueprint, bIncludeInherited)` — with `True` it also reports interfaces inherited
  through the parent chain, natively-implemented ones included.
- **Implementing** a member: a void interface function is an event
  (`AddEvent`, or an `` (event `BPI_Interactable.Interact` …) `` entry); a value-returning one gets a
  graph (`AddFunctionGraph` named after it, then a `(fn …)` body).
- **Authoring the interface itself**: a Blueprint Interface is nothing but its function graphs, so an
  empty one contributes nothing. Add them with `AddFunctionGraph` on the **BPI** asset and give each
  its parameters with `AddFunctionParam`. There is no `create_function_graph` any more.
- The asset must be a true Blueprint Interface (`BPTYPE_Interface`, made with
  `BlueprintInterfaceFactory`). A normal Blueprint, or one merely parented to `UInterface`, is not
  one and will fail to resolve as an interface class.
- Calling an interface function at runtime: `` (message `Interface.Function` target args…) `` is the
  runtime-checked call. A plain member call is direct and needs a target that really implements it.

## StateTree dispatcher variables

To add a StateTree delegate dispatcher to a Blueprint task (`STT_*`), the type is the **USTRUCT**
`FStateTreeDelegateDispatcher`. `"EventDispatcher"` is a Blueprint-only concept and is not a valid
type string at all. `AddStructVariable` is the unambiguous route (`AddVariable` also documents
struct names and paths, so `typeName="StateTreeDelegateDispatcher"` should work — but the struct
handle cannot be misread):

```python
vibeue.exec_tool(BT, "AddStructVariable",
                 {"blueprint": bp, "name": "FinishRotatingDispatcher",
                  "structType": {"refPath": "/Script/StateTreeModule.StateTreeDelegateDispatcher"}})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
```

Then use `StateTreeService.bind_transition_to_delegate` to link it to a transition — it matches the
task by **registered class name** (`STT_Rotate_C`), not asset path.

## VibeUE's 19 survivors

All `unreal.BlueprintService.<snake_name>()` from inside `execute_python`. Sixteen are Timelines;
`create_macro_graph`, `analyze_graph_layout` and `refresh_blueprint_editor` are the other three.
**Nothing in Epic's toolsets replaces any of them.** Full coverage lives in the `blueprint-graphs`
pack (`node-reference.md` for Timelines, `node-layout.md` for the layout audit), but the short
version:

```python
import unreal
bs = unreal.BlueprintService

# Timelines: the node + its UTimelineTemplate + tracks + keys. No engine tool touches a template.
node = bs.add_timeline("/Game/Blueprints/BP_Door", "EventGraph", "SwingTimeline", 1.0)
bs.add_timeline_float_track("/Game/Blueprints/BP_Door", "SwingTimeline", "Alpha")
bs.add_timeline_float_key("/Game/Blueprints/BP_Door", "SwingTimeline", "Alpha", 0.0, 0.0)
bs.add_timeline_float_key("/Game/Blueprints/BP_Door", "SwingTimeline", "Alpha", 1.0, 1.0)
for t in bs.get_timelines("/Game/Blueprints/BP_Door"):
    print(t.timeline_name, t.track_count, t.length, t.loop, t.auto_play)

# A macro graph on a Macro Library: BlueprintToolset.AddFunctionGraph makes FUNCTION graphs (and
# asserts on a Macro Library), and BlueprintDslToolset refuses a macro library outright —
# "macro graphs have no Graph DSL representation" (BlueprintDslDomain.cpp:51-55). Idempotent.
bs.create_macro_graph("/Game/Macros/ML_Doors", "SwingOpen")

# Measure a layout (Epic's ArrangeGraphLayered / ArrangeNodes only write one).
report, err = bs.analyze_graph_layout("/Game/Blueprints/BP_Door", "EventGraph")

# Make an open editor re-run its construction script and redraw. False = no editor open,
# which is not an error: the on-disk asset is already current.
bs.refresh_blueprint_editor("/Game/Blueprints/BP_Door")
```

A Macro Library **asset** is still a factory call (`unreal.BlueprintMacroFactory`, not
`BlueprintFactory`) — see `workflows.md`.

---

## Migration table — removed VibeUE tool → replacement

| Removed | Replacement |
|---|---|
| `create_blueprint` | `BlueprintToolset.Create` |
| `compile_blueprint` | `BlueprintToolset.CompileBlueprint` |
| `get_blueprint_info` | `BlueprintToolset.ListGraphs` / `ListVariables` / `ListFunctions` / `ListEvents` / `GetParent` |
| `add_variable`, `add_member_variable` | `BlueprintToolset.AddVariable` / `AddStructVariable` / `AddObjectVariable` |
| `remove_member_variable` | `BlueprintToolset.RemoveVariable` |
| `list_variables`, `get_variable_info` | `BlueprintToolset.ListVariables` + the `GetVariable*` tools; type and default from `ObjectToolset` on the CDO |
| `search_variable_types` | the type strings are documented on `AddVariable`; `ObjectToolset.SearchSubclasses` finds a class, `GetPropertySchema` a live type |
| `set_variable_default_value` | `ObjectToolset.SetProperties` on `BlueprintToolset.GetDefaultObject` |
| `get_property` / `set_property` | `ObjectToolset.GetProperties` / `SetProperties` on the CDO |
| `create_function` / `add_function` / `create_function_graph` | `BlueprintToolset.AddFunctionGraph` |
| `add_function_parameter`, `get_function_parameters`, `get_function_info` | `BlueprintToolset.AddFunctionParam` / `RemoveFunctionParam` / `GetFunctionFlags`, `SetFunctionFlags`; the DSL `(params …)` / `(returns …)` sections for a whole signature |
| `add_function_local_variable` | `BlueprintToolset.AddVariable` with `options.graph` set, or the DSL `(locals …)` section |
| `list_functions`, `list_graphs` | `BlueprintToolset.ListFunctions` / `ListEvents` / `ListGraphs` |
| `list_overridable_functions` | `BlueprintToolset.ListEvents` / `ListFunctions` — both list local **plus inheritable** members, and `is_implemented` is the old `already_overridden` |
| `override_function` | `BlueprintToolset.AddEvent` (event-style) / `AddFunctionGraph` (function-style), named after the inherited member |
| `open_function_graph` | `EditorToolset.EditorAppToolset.OpenEditorForAsset` |
| `add_component`, `remove_component` | `EditorToolset.ActorToolset.AddComponent` / `RemoveComponent` |
| `reparent_component`, `set_root_component` | `ActorToolset.SetParentComponent` |
| `list_components`, `get_component_hierarchy`, `component_exists` | `ActorToolset.GetComponents` on the Blueprint's CDO (+ `GetRootComponent` / `GetParentComponent`) |
| `get_component_property` / `set_component_property` / `get_all_component_properties` / `list_component_properties` | `ObjectToolset.ListProperties` / `GetPropertySchema` / `GetProperties` / `SetProperties` on the component |
| `set_collision_settings` | `ObjectToolset.SetProperties` on the component's `BodyInstance.*` paths |
| `get_available_components`, `get_component_info` | `ObjectToolset.SearchSubclasses` (base `/Script/Engine.ActorComponent`) + `GetPropertySchema` |
| `compare_components` | `ActorToolset.Diff` (text diff of two actors' classes, top-level properties and per-component properties), or `ObjectToolset.GetProperties` on both |
| `add_event_dispatcher` | `BlueprintToolset.AddEventDispatcher` |
| `add_event_dispatcher_parameter` | `BlueprintToolset.AddFunctionParam` on the signature graph `AddEventDispatcher` returned |
| `remove_event_dispatcher` | `BlueprintToolset.RemoveFunctionGraph` — it removes a function graph **or** a dispatcher |
| `add_call_delegate_node` | a DSL broadcast statement, `` (`self.Dispatcher` args…) `` |
| `add_delegate_bind_on_variable`, `add_delegate_bind_node` | a DSL `` (add `Owner.Dispatcher` …) `` with a `CreateDelegate` handler, or `BlueprintToolset.AddComponentBoundEvent` / `AddMatchingDelegateEvent` |
| `add_interface` / `remove_interface` | `BlueprintToolset.AddInterface` / `RemoveInterface` (class handle, not asset path) |
| everything node-level (`build_graph`, `connect_nodes`, `discover_nodes`, …) | `BlueprintDslToolset` / `BlueprintToolset` — see the `blueprint-graphs` pack |

---

## Task Index

| Task | Workflow | Sample script (run via `execute_python`) |
|------|----------|-----------------------------------------------|
| Create a Blueprint + variables | `workflows.md` → Create a Blueprint | `scripts/create_blueprint.txt` |
| Add components + set properties | `workflows.md` → Add components | `scripts/add_component.txt` |
| Add an event dispatcher (delegate) | `workflows.md` → Event dispatcher | `scripts/event_dispatcher.txt` |
| Implement a Blueprint interface | `workflows.md` → Implement an interface | `scripts/add_interface.txt` |
| Inspect a Blueprint (components, parent class) | `introspection.md` | — |
| Timelines, macro graphs, layout audits | load the **`blueprint-graphs`** skill | — |
| Node-level graph editing | load the **`blueprint-graphs`** skill | — |

## Sub-docs

- **`workflows.md`** — step-by-step create / component / dispatcher / interface workflows with
  copy-paste Python.
- **`introspection.md`** — what Blueprint introspection works and what does not from Python in
  5.9: CDO reads, parent class via asset tags, listing Blueprints by class, level actors, and which
  reads are now toolset calls.

## Verification

After any edit: `BlueprintToolset.CompileBlueprint`, read the result, then save
(`EditorToolset.AssetToolset.SaveAsset`, or `unreal.EditorAssetLibrary.save_asset(path)`). Don't
claim success until the compile reports zero errors, and never save a Blueprint whose compile status
is `BS_ERROR`. For a graph change, also read the shape back with `BlueprintDslToolset.Read` — a tool
returning without error is not proof the graph matches the request.

## Additional gotchas

- `SubobjectDataSubsystem` is an **engine** subsystem (`get_engine_subsystem`);
  `get_object_for_blueprint` lives on `SubobjectDataBlueprintFunctionLibrary`; SCS templates are
  named `<Name>_GEN_VARIABLE`; a `SubobjectDataHandle` has no `is_valid()` — test
  `k2_find_subobject_data_from_handle(h) is not None`. You rarely need any of this now that
  `ActorToolset.GetComponents` enumerates the SCS for you.
- `set_collision_enabled(NO_COLLISION)` on a spawned actor's component does not survive
  save/reload; `CollisionProfileName = "NoCollision"` serialises, and
  `bCanEverAffectNavigation=False` persists.
- Engine collision profiles predate custom trace channels, so a channel with no explicit entry falls
  back to the ini default response (BLOCK); a scripted volume or primitive must set each custom
  channel's response explicitly.
- Changing a component's collision on a Blueprint can dirty **placed actors** that had not
  overridden the old value — each such actor's package (its external actor package under World
  Partition) is marked dirty with no undo, so the next save sweep writes it.
- Script-baked BP-CDO `TArray<TSubclassOf<...>>` defaults can be LOST on editor reload — read them
  back after a restart to confirm they persisted. Writing the CDO is allowed; the engine just drops
  these array defaults across a reload.
- Components that come from the **parent C++ class** are real sub-objects on the CDO and can be set
  directly; only SCS-added ones are templates. Either way, `ObjectToolset.SetProperties` on the
  object `ActorToolset.GetComponents` handed you is the correct write path.
- A compiled Blueprint's class is `<Asset>_C`. Mixing the asset path and the class path is the
  single most common `{refPath}` error — `assetType`, `objectClass`, `interfaceClass` and
  `parentClass` all want the **class**.

## Reference pages

Open these with the paths below; UE rewrites them to absolute paths when it serves this skill.

- [introspection](introspection.md) - What Blueprint introspection works and what doesn't from Python in UE 5.9 - level actors, finding BP assets, reading components and the parent class, listing BPs by class
- [workflows](workflows.md) - Step-by-step Blueprint asset workflows - create a Blueprint with variables, add components with properties, add event dispatchers, implement interfaces
- [scripts/add_component.txt](scripts/add_component.txt) - runnable sample
- [scripts/add_interface.txt](scripts/add_interface.txt) - runnable sample
- [scripts/create_blueprint.txt](scripts/create_blueprint.txt) - runnable sample
- [scripts/event_dispatcher.txt](scripts/event_dispatcher.txt) - runnable sample

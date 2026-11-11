---
name: blueprints/workflows
description: Step-by-step Blueprint asset workflows in UE 5.9 — create a Blueprint with variables (BlueprintToolset.Create / AddVariable), add components with properties (ActorToolset + ObjectToolset), add event dispatchers, implement Blueprint interfaces, and create a Macro Library with VibeUE's create_macro_graph.
---

# Blueprint Workflows

## Contents
- Create a Blueprint with variables
- Add components with properties
- Event dispatcher (multicast delegate)
- Implement a Blueprint interface
- Create a Macro Library and a macro graph

Each workflow has a matching runnable example under `scripts/`.

Shared preamble for every snippet below:

```python
import json, unreal, vibeue

BT = "BlueprintToolsets.BlueprintToolset"      # Blueprint class, variables, functions, events
AT = "EditorToolset.ActorToolset"              # components
OT = "EditorToolset.ObjectToolset"             # every property
AS = "EditorToolset.AssetToolset"              # exists / save / validate

def ref(p):
    """Package path -> the {refPath} handle these tools require."""
    tail = p.rsplit("/", 1)[-1]
    return {"refPath": p if "." in tail else "{}.{}".format(p, tail)}

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not les.is_in_play_in_editor(), "stop PIE before editing a Blueprint"
```

`vibeue.exec_tool` is the in-process form of `call_tool`; from an MCP client the same calls are
`call_tool(tool_name=..., toolset_name=..., arguments={...})`.

---

## Create a Blueprint with variables

```python
# 1. The asset. folderPath and assetName are SEPARATE strings; assetType is a CLASS handle.
bp = vibeue.exec_tool(BT, "Create", {"folderPath": "/Game/Blueprints",
                                     "assetName": "BP_Player",
                                     "assetType": {"refPath": "/Script/Engine.Character"}})
path = "/Game/Blueprints/BP_Player"

# 2. Variables. AddVariable's string types: bool, int, float, byte, string, name, text,
#    Vector, Rotator, Transform, Vector2D, LinearColor, and the bare name or object path of
#    any UEnum / UScriptStruct. Use int (not int32) and float (not double).
vibeue.exec_tool(BT, "AddVariable", {"blueprint": bp, "name": "Health", "typeName": "float"})
vibeue.exec_tool(BT, "AddVariable", {"blueprint": bp, "name": "IsAlive", "typeName": "bool"})

# An array / set / map, a category, a tooltip and Blueprint-Read-Only all live in one options
# struct. NOTE: a MAP container uses FString as its key type.
vibeue.exec_tool(BT, "AddVariable", {"blueprint": bp, "name": "Waypoints", "typeName": "Vector",
                                     "options": {"containerType": "ARRAY",
                                                 "category": "Patrol",
                                                 "tooltip": "Patrol route, in world space"}})

# Anything AddVariable's string list does not cover: AddStructVariable / AddObjectVariable.
vibeue.exec_tool(BT, "AddObjectVariable", {"blueprint": bp, "name": "DeathFX",
                                           "objectClass": {"refPath": "/Script/Niagara.NiagaraSystem"}})

# 3. Compile — the variables only exist on the skeleton class afterwards.
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})

# 4. Defaults are CDO properties, not a variable tool.
cdo = vibeue.exec_tool(BT, "GetDefaultObject", {"blueprint": bp})
vibeue.exec_tool(OT, "SetProperties", {"instance": cdo,
                                       "values": json.dumps({"Health": 100.0, "IsAlive": True})})

vibeue.exec_tool(AS, "SaveAsset", {"assetPath": path})
```

Runnable: `scripts/create_blueprint.txt`.

A variable whose type is another **Blueprint** class is `AddObjectVariable` with that Blueprint's
generated class — `{"refPath": "/Game/Blueprints/BP_Enemy.BP_Enemy_C"}`. The `_C` suffix belongs on
a **class** handle and nowhere else.

---

## Add components with properties

```python
bp = ref("/Game/Blueprints/BP_Player")

# AddComponent's owner is "the actor blueprint, instance, or SceneComponent to add to" — so pass
# the Blueprint for a root-level component, or a component to nest under it.
body = vibeue.exec_tool(AT, "AddComponent",
                        {"owner": bp, "name": "BodyMesh",
                         "componentType": {"refPath": "/Script/Engine.StaticMeshComponent"}})
glow = vibeue.exec_tool(AT, "AddComponent",
                        {"owner": body, "name": "Glow",
                         "componentType": {"refPath": "/Script/Engine.PointLightComponent"}})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})

# Find the property, check its type, then write it.
print(vibeue.exec_tool(OT, "ListProperties", {"instance": glow, "nameFilter": "intensity|color"}))
print(vibeue.exec_tool(OT, "GetPropertySchema", {"instance": glow, "root": "LightColor"}))

# LightColor is an FColor: INTEGER BYTES 0-255. LinearColor-style floats give a near-black light.
vibeue.exec_tool(OT, "SetProperties", {"instance": glow, "values": json.dumps(
    {"Intensity": 5000.0, "LightColor": {"r": 255, "g": 127, "b": 0, "a": 255}})})

# Mesh and anim assets are object references — set them by path.
vibeue.exec_tool(OT, "SetProperties", {"instance": body, "values": json.dumps(
    {"StaticMesh": {"refPath": "/Game/Meshes/SM_Body.SM_Body"}})})

vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
vibeue.exec_tool(AS, "SaveAsset", {"assetPath": "/Game/Blueprints/BP_Player"})
```

Runnable: `scripts/add_component.txt`.

**Hierarchy and the scene root.** Pass the parent as `owner` when adding, or reparent afterwards
with `SetParentComponent`. Passing a component as the parent of the *current root* **promotes it to
the scene root** and makes the old root its child; an auto-generated `DefaultSceneRoot` is removed by
Unreal. Passing `None` as the parent detaches. That one tool replaces the old
`reparent_component` + `set_root_component` pair.

**Listing them back**: `ActorToolset.GetComponents` on the Blueprint's CDO. It detects a Blueprint
actor and switches to `SubobjectDataSubsystem`, so it really does enumerate the SCS — this is not
the empty-list trap that raw `K2_GetComponentsByClass` on a CDO is.

```python
cdo = vibeue.exec_tool(BT, "GetDefaultObject", {"blueprint": bp})
for c in vibeue.exec_tool(AT, "GetComponents", {"actor": cdo}):
    print(c)
```

---

## Event dispatcher (multicast delegate)

```python
bp = ref("/Game/Blueprints/BP_Player")

# 1. The dispatcher. AddEventDispatcher returns its SIGNATURE GRAPH.
sig = vibeue.exec_tool(BT, "AddEventDispatcher", {"blueprint": bp, "name": "OnDied"})

# 2. Its parameters are AddFunctionParam on that graph. Dispatchers take INPUTS ONLY.
vibeue.exec_tool(BT, "AddFunctionParam", {"graph": sig, "paramName": "Killer",
                                          "paramType": "Actor", "inputParam": True})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
print(vibeue.exec_tool(BT, "ListEventDispatchers", {"blueprint": bp}))
```

**Broadcasting it** is a DSL statement, not a node-placement call (`add_call_delegate_node` is
gone):

```python
DSL = "BlueprintToolsets.BlueprintDslToolset"
vibeue.exec_tool(DSL, "Docs", {})          # grammar first, before any Write
vibeue.exec_tool(DSL, "Write", {"assetPath": "/Game/Blueprints/BP_Player.BP_Player",
                                "scope": "EventGraph", "dsl": """
(event `self.ReceiveAnyDamage`
  (`self.OnDied` (`self.GetInstigator`)))
"""})
```

**Subscribing from another Blueprint** is `(add …)` with a `|CreateDelegate` handler:

```
(event `HandleDied` :!signature `/Game/Blueprints/BP_Player.OnDied`
  (params (Killer :type "Actor Object Reference")))

(event `self.ReceiveBeginPlay`
  (add `/Game/Blueprints/BP_Player.OnDied` (`|CreateDelegate` :!event `HandleDied`) (`self.Player`)))
```

`` (remove `Owner.Dispatcher` handler) `` unsubscribes, `` (clear `Owner.Dispatcher`) `` drops every
listener. `BlueprintToolset.AddComponentBoundEvent` / `AddMatchingDelegateEvent` are the imperative
alternatives, and `ListComponentEvents` / `GetDelegateSignature` /
`ListCompatibleEventFunctions` tell you what a given delegate will accept.

**Removing a dispatcher** is `RemoveFunctionGraph(Blueprint, GraphName)` — it handles "a function
graph **or event dispatcher**". There is no `RemoveEventDispatcher`.

Runnable: `scripts/event_dispatcher.txt`.

---

## Implement a Blueprint interface

```python
bp  = ref("/Game/Blueprints/BP_Player")
bpi = "/Game/Interfaces/BPI_Interactable"
cls = {"refPath": bpi + ".BPI_Interactable_C"}     # the interface CLASS, not the asset

# Authoring the interface itself: a Blueprint Interface is nothing but its function graphs, so an
# empty one contributes nothing when implemented.
ig = vibeue.exec_tool(BT, "AddFunctionGraph", {"blueprint": ref(bpi), "graphName": "GetDisplayName"})
vibeue.exec_tool(BT, "AddFunctionParam", {"graph": ig, "paramName": "Name",
                                          "paramType": "string", "inputParam": False})  # a return
vibeue.exec_tool(BT, "AddFunctionGraph", {"blueprint": ref(bpi), "graphName": "Interact"})  # void
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": ref(bpi)})

# Implement it. Idempotent; the Blueprint MUST be recompiled for the change to take effect.
vibeue.exec_tool(BT, "AddInterface", {"blueprint": bp, "interfaceClass": cls})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
print(vibeue.exec_tool(BT, "ListInterfaces", {"blueprint": bp, "includeInherited": True}))

# A void interface member becomes an overridable EVENT; a value-returning one becomes a graph.
vibeue.exec_tool(BT, "AddEvent", {"blueprint": bp, "eventName": "Interact", "position": [0, 0]})
vibeue.exec_tool(BT, "AddFunctionGraph", {"blueprint": bp, "graphName": "GetDisplayName"})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
vibeue.exec_tool(AS, "SaveAsset", {"assetPath": "/Game/Blueprints/BP_Player"})
```

Runnable: `scripts/add_interface.txt`.

- `interfaceClass` wants the **`_C` class handle**. Passing the asset path is the usual failure here.
- `RemoveInterface(Blueprint, InterfaceClass, bPreserveFunctions)` — `True` keeps the implemented
  graphs as ordinary Blueprint functions instead of deleting them. Recompile afterwards.
- The asset must be a real Blueprint Interface (`BPTYPE_Interface`, from
  `BlueprintInterfaceFactory`). A normal Blueprint, or one merely parented to `UInterface`, is not
  one.
- At runtime, `` (message `BPI_Interactable.Interact` target) `` is the runtime-checked interface
  call; a plain member call is direct and needs a target that really implements it.

---

## Create a Macro Library and a macro graph

This is the one authoring job that is still VibeUE's, because
`BlueprintToolset.AddFunctionGraph` makes *function* graphs (and adding one to a Macro Library
asserts in the K2 schema) and `BlueprintDslToolset` refuses a macro library outright: "macro graphs
have no Graph DSL representation" (`BlueprintDslDomain.cpp:51-55`).

```python
import unreal

# 1. The library asset — BlueprintMacroFactory, NOT BlueprintFactory.
f = unreal.BlueprintMacroFactory()
f.set_editor_property("parent_class", unreal.Actor.static_class())
unreal.AssetToolsHelpers.get_asset_tools().create_asset("ML_Doors", "/Game/Macros",
                                                        unreal.Blueprint, f)

# 2. The macro graph inside it — VibeUE's surviving tool. Idempotent.
unreal.BlueprintService.create_macro_graph("/Game/Macros/ML_Doors", "SwingOpen")
unreal.EditorAssetLibrary.save_asset("/Game/Macros/ML_Doors")
```

Place an **instance** of it from the DSL by owner reference — the library asset path, dot, the macro
name, the same shape the engine's own StandardMacros use:

```
(`/Game/Macros/ML_Doors.SwingOpen` door)
(`/Engine/EditorBlueprintResources/StandardMacros.Gate` :!via `Open` :!label _gate)
```

> ⚠️ `create_macro_graph`'s own doc comment still shows the legacy
> `"/Game/MyMacroLib.MyMacroLib:MyMacroName"` reference format. That was the argument shape of the
> deleted `add_macro_instance_node`. The **DSL** owner reference is
> `<LibraryAssetPath>.<MacroName>`, as above — confirm it with
> `BlueprintDslToolset.FindNodeReferences` if in doubt.

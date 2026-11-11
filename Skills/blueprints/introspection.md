---
name: blueprints/introspection
description: What Blueprint introspection works and what doesn't in UE 5.9 — which reads are now BlueprintToolset / ActorToolset / ObjectToolset calls, getting level actors, finding BP assets, reading components and the parent class, listing Blueprints by class, and material parameters.
---

# Blueprint Introspection — What Works and What Doesn't (UE 5.9)

## Contents
- Read it with a toolset first
- Getting level actors
- Finding Blueprint assets
- Inspecting Blueprint components
- Getting the parent class
- Listing Blueprints by class
- Reading functions, events and variables
- Material parameters

## Read it with a toolset first

In 5.9 the fast, reliable reads are tool calls, not hand-rolled Python. Reach for raw `unreal.*`
only for what no tool covers (asset-registry tags, factories).

| Question | Call |
|---|---|
| What graphs / functions / events / dispatchers does it have? | `BlueprintToolset.ListGraphs` / `ListFunctions` / `ListEvents` / `ListEventDispatchers` |
| What variables, and which are writable? | `BlueprintToolset.ListVariables` |
| What is its parent class? | `BlueprintToolset.GetParent` |
| What interfaces does it implement? | `BlueprintToolset.ListInterfaces` (`bIncludeInherited` for the parent chain too) |
| What components does it have? | `ActorToolset.GetComponents` on `BlueprintToolset.GetDefaultObject` |
| What is this property's type / value? | `ObjectToolset.ListProperties` → `GetPropertySchema` → `GetProperties` |
| What does the graph actually do? | `BlueprintDslToolset.Read` (see `blueprint-graphs`) |
| What is open / selected right now? | `EditorAppToolset.GetOpenAssets` / `GetAssetEditorSelection` |

All of these are `{refPath}`-handle tools reached with `call_tool` or `vibeue.exec_tool`; the
preamble in `workflows.md` has the `ref()` helper.

## Getting level actors

```python
# CORRECT
actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_subsys.get_all_level_actors()

# AVOID — unreal.EditorLevelLibrary is deprecated by Unreal itself.
# (VibeUE no longer ships its own execute_python, so there is no plugin-side blocklist
#  stopping you; the deprecation is the engine's.)
# unreal.EditorLevelLibrary.get_all_level_actors()
```

`EditorToolset.LevelToolset` and `EditorAppToolset.GetSelectedActors` / `GetVisibleActors` are the
tool-side equivalents when you want actors rather than Blueprints.

## Finding Blueprint assets

```python
ar = unreal.AssetRegistryHelpers.get_asset_registry()
assets = ar.get_assets_by_path('/Game', recursive=True)
print(assets[0].package_name)   # /Game/Blueprints/BP_MyActor
print(assets[0].package_path)   # /Game/Blueprints
# WRONG — object_path does not exist on AssetData
```

`EditorToolset.AssetToolset.FindAssets` is the tool-side search, and `Exists` is the cheap presence
check. Both are async-capable — use `await vibeue.exec_tool_await(...)` or
`vibeue.exec_tool_collect(...)` for `FindAssets`.

## Inspecting Blueprint components

### ✅ `ActorToolset.GetComponents` on the CDO *does* enumerate the SCS

The old warning that CDO component enumeration comes back empty is true only of
**`K2_GetComponentsByClass`** in raw Python: SCS-added components are node templates, not instances
on the class default object, so that call returns `[]` and the loop body never runs.

`ActorToolset.GetComponents` does not have that problem. It detects a Blueprint actor and switches to
`SubobjectDataSubsystem` (`ActorToolset.cpp:497-519`), so passing it the Blueprint's CDO lists the
SCS:

```python
cdo   = vibeue.exec_tool(BT, "GetDefaultObject", {"blueprint": ref("/Game/Blueprints/BP_MyActor")})
comps = vibeue.exec_tool(AT, "GetComponents", {"actor": cdo})          # SCS templates included
mesh  = vibeue.exec_tool(AT, "GetComponents", {"actor": cdo,
                                               "componentType": {"refPath": "/Script/Engine.SkeletalMeshComponent"}})
```

Then write to the handle it gave you:

```python
vibeue.exec_tool(OT, "SetProperties", {"instance": mesh[0], "values": json.dumps({
    "SkeletalMeshAsset": {"refPath": "/Game/X/SK_Windmill.SK_Windmill"},
    "AnimationMode": "AnimationBlueprint",
    "AnimClass": {"refPath": "/Game/X/ABP_Windmill.ABP_Windmill_C"}})})
```

An asset value is its **object path**, a class value is the **`_C` path**, and an enum value is the
entry name. Read it back with `GetProperties` to confirm.

### Raw-Python CDO reads

Still fine, and still the only route when no tool is loaded. CDO `set_editor_property` writes are
allowed — it is a legitimate way to set a Blueprint's defaults — but `ObjectToolset.SetProperties`
is the better path because it validates against the property's own schema.

```python
import unreal

bp = unreal.EditorAssetLibrary.load_asset("/Game/Blueprints/BP_MyActor")
# unreal.load_asset() can return None on a fresh session; EditorAssetLibrary.load_asset() is reliable
gc  = bp.generated_class()
cdo = unreal.get_default_object(gc)
print(cdo.get_editor_property("initial_life_span"))
```

These do **not** work: `bp.get_editor_property('simple_construction_script')` (protected),
`scs.get_all_nodes()` (crash), `BlueprintEditorLibrary.get_blueprint_component_names()` (absent),
`bp.parent_class` / `bp.get_editor_property('parent_class')` (not exposed — use
`BlueprintToolset.GetParent`, or the asset tag below).

## Getting the parent class

`BlueprintToolset.GetParent(Blueprint)` is the direct answer. The asset-registry route is still
useful when you want it **without loading the asset**:

```python
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for a in ar.get_assets_by_path("/Game/Blueprints", False):
    if "BP_MyActor" in str(a.asset_name):
        print(a.get_tag_value("ParentClass"))        # e.g. "/Script/Engine.Actor"
        print(a.get_tag_value("NativeParentClass"))  # native C++ parent
        break
```

`BlueprintToolset.SetParent(Blueprint, ParentClass)` reparents.

## Listing Blueprints by class

```python
# ARFilter properties can't be set after construction; the asset_class_names kwarg doesn't exist.
ar = unreal.AssetRegistryHelpers.get_asset_registry()
bp_class = unreal.TopLevelAssetPath("/Script/Engine", "Blueprint")
assets = ar.get_assets_by_class(bp_class, True)   # True = include derived
for a in assets:
    print(f"{a.package_path}/{a.asset_name}")
```

`ObjectToolset.SearchSubclasses(BaseClass, ClassName)` is the tool-side way to find **classes**
(including Blueprint-generated ones) under a base, which is usually what you actually want before an
`AddObjectVariable` or a `Create`.

## Reading functions, events and variables

The old `list_functions` / `list_overridable_functions` / `get_variable_info` family is gone, and the
replacements return **much smaller** structs — do not expect the old field sets.

| Old | New | What you get |
|---|---|---|
| `list_functions(bp)` → `BlueprintFunctionInfo` | `BlueprintToolset.ListFunctions(Blueprint)` | `FBlueprintFunctionInfo` — `name`, `description`, `is_implemented` |
| `list_overridable_functions(bp)` → `OverridableFunctionInfo` | `BlueprintToolset.ListFunctions` / `ListEvents` — both list local **plus inheritable** members (parent chain and implemented interfaces), and `is_implemented` is the old `already_overridden` | same three fields; **no `owner_class`, `is_event_style`, `is_native_event`, `parameters` or `return_type`** |
| `override_function(bp, name)` | `AddEvent` (void/latent → event in the EventGraph) or `AddFunctionGraph` (returns a value → its own graph), **named after the inherited member** | the node / graph; both idempotent |
| `list_variables` / `get_variable_info` | `BlueprintToolset.ListVariables(Blueprint, Scope, Graph, bIncludeInherited)` | `FBlueprintVariableInfo` — **only `name` and `writable`** |
| `get_node_pins` / `get_node_details` | `BlueprintToolset.GetNodeInfos` / `GetNodeTypePins`, or `BlueprintDslToolset.Schema` | `FBlueprintNodeInfo`: `type_id`, `node` (handle), `position`, `input_pins`, `output_pins` — **no `node_id`** |

So a variable's **type, default, category, tooltip, read-only and replication** are four different
lookups now, not one struct:

- type and default → `ObjectToolset.GetPropertySchema` / `GetProperties` on the CDO
- category → `GetVariableCategory`
- tooltip → `GetVariableTooltip`
- read-only → `GetVariableReadOnly`; replication → `GetVariableReplication`

`ListVariables`' `Scope` is `MEMBER` when unset, and `MEMBER` leaves out inherited variables unless
`bIncludeInherited` is set. `LOCAL` **requires** `Graph`; the other scopes refuse it.

"What can I override?" is therefore: `ListEvents` for the event-style members and `ListFunctions`
for the function-style ones, filtered to `is_implemented == False`. Which of the two a member
appears in is what tells you whether taking it needs `AddEvent` or `AddFunctionGraph` — there is no
`is_event_style` flag any more. `ListCompatibleEventFunctions(Node)` is a **different** question:
the functions a given `CreateDelegate` node can bind to, not what the class can override.

### Python naming still strips the boolean `b`

Where a struct does reach Python, UE drops the lowercase `b` prefix from booleans: `bWritable` is
`writable`, `bIsPure` is `is_pure`, `bSuccess` is `success`. Never write `b_writable` — it raises
`AttributeError`.

The exception is **string-keyed** property access: `ObjectToolset.GetProperties` /
`SetProperties` and `get_editor_property` take the **native C++ name**, so booleans keep the
prefix there — `"bReplicates"`, `"bCanBeDamaged"`, `"bCanEverAffectNavigation"`. A stripped name
is simply not found.

Likewise, UE Python collapses `bool Func(..., Out&)` into a **single** return value. VibeUE's
`analyze_graph_layout` has two out-strings and so comes back as a 2-tuple; a method with one
out-param returns that value (or `None`) and **never** a `(success, value)` pair.

## Material parameters

```python
import unreal
mat = unreal.load_asset('/Game/Materials/M_MyMaterial')
print(unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat))
```

`EditorToolset.MaterialToolset` / `MaterialInstanceToolset` are the tool-side surface — see the
`materials` skill.

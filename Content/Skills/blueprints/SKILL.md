---
name: blueprints
display_name: Blueprint System
description: Create and modify Blueprint assets, variables, functions, components, event dispatchers, and interfaces. Use when the user asks to create a Blueprint/BP, add a variable or component to a Blueprint, make an event dispatcher/delegate, implement a Blueprint interface, or inspect a Blueprint's class/components. For node-level graph wiring, also load blueprint-graphs.
vibeue_classes:
  - BlueprintService
  - AssetDiscoveryService
unreal_classes:
  - EditorAssetLibrary
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

> **For node-level graph editing** (adding nodes, connecting pins, wiring logic, timers, layout), load the `blueprint-graphs` skill.

## Critical Rules

### ⚠️ Engine `BlueprintTools` args are `{refPath}` objects, NOT strings

Every UObject/UClass argument to the engine `BlueprintTools` toolset (`blueprint`, `asset_type`,
`graph`, `parent_class`, …) is a **reference object** `{"refPath": "<object path>"}`, and these tools
**return** the same shape. Passing a plain string fails with
`"<x> is not a valid object path for property '<arg>'"`. This rule is **only** for the engine
`BlueprintTools` `call_tool` path — VibeUE's own `unreal.BlueprintService.*` Python methods still take
plain string paths.

The `refPath` must be the **full object path** — `/Game/Dir/Asset.Asset` (the asset name repeated
after the dot). A bare **package** path (`/Game/Dir/Asset`, no `.Asset`) is rejected. Two safe ways
to get one:

```python
# (a) Reuse what create/load returns — it is already a {refPath} object:
bp_ref = call_tool("create", "editor_toolset.toolsets.blueprint.BlueprintTools",
                   {"folder_path": "/Game/BP", "asset_name": "BP_X",
                    "asset_type": {"refPath": "/Script/Engine.Actor"}})["returnValue"]

# (b) Build one from a package path you already have:
def bp_ref_of(p):  # "/Game/Dir/BP_X" -> {"refPath": "/Game/Dir/BP_X.BP_X"}
    name = p.rsplit("/", 1)[-1].split(".")[0]
    return {"refPath": p if "." in p.rsplit("/", 1)[-1] else f"{p}.{name}"}
```

All the `arguments={"blueprint": path, ...}` / `{"blueprint": bp, ...}` examples in the sub-docs are
shorthand — wrap the path with `bp_ref_of(...)` (or reuse the returned ref) when you actually call.
On success these tools return `{"returnValue": null}`; an error comes back as a `Parameter error`.

### ⚠️ Creating a Blueprint — engine `BlueprintTools.create`

Blueprint basics now live on the engine's `BlueprintTools` toolset (VibeUE extends the native MCP
endpoint). Create a Blueprint with `call_tool`, not `unreal.BlueprintService.create_blueprint` (that
method was cut):

```python
# CORRECT — folder_path + asset_name are strings; asset_type is a {refPath} class reference.
result = call_tool(
    tool_name="create",
    toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
    arguments={"folder_path": "/Game/Blueprints", "asset_name": "BP_MyActor",
               "asset_type": {"refPath": "/Script/Engine.Actor"}},
)
# -> {"returnValue": {"refPath": "/Game/Blueprints/BP_MyActor.BP_MyActor"}}
bp_ref = result["returnValue"]   # reuse this {refPath} object for later BlueprintTools calls
```

The folder and asset name are passed separately (`folder_path` + `asset_name`) — do not jam a full
asset path into one argument. `asset_type` is the parent **class** as a `{refPath}`:
`/Script/Engine.Actor`, `/Script/Engine.Pawn`, `/Script/Engine.Character`, or a Blueprint's generated
class `"/Game/.../BP_Base.BP_Base_C"`.

### Blueprint Types in add_variable — engine `BlueprintTools.add_variable`

Adding a variable is also an engine `BlueprintTools` call now (`unreal.BlueprintService.add_variable`
was cut). Use `call_tool` with `blueprint`, `name`, `type_name` (and `add_object_variable` /
`add_struct_variable` for object- and struct-typed variables):

```python
# CORRECT — blueprint is a {refPath} object; name/type_name are strings
call_tool(
    tool_name="add_variable",
    toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
    arguments={"blueprint": {"refPath": "/Game/BP_MyActor.BP_MyActor"}, "name": "Health", "type_name": "float"},
)
```

When the variable's type is a **Blueprint class**, use the Blueprint asset name directly in
`type_name`. Do **not** add `_C`, and do **not** construct a generated class path — just use the name
(`"BP_Enemy"`, or the full `"/Game/Blueprints/BP_Enemy"`). Appending `_C` (`"BP_Enemy_C"`) or guessing
a generated class path is **wrong**; the type system resolves Blueprint names automatically via asset
search.

Engine structs need the `F` prefix: color variables are `"FLinearColor"` / `"FColor"` —
plain `"LinearColor"` / `"Color"` is not resolved. Unsure of a type string? Use the surviving
`unreal.BlueprintService.search_variable_types("Linear")` to look it up.

**Supported primitive `type_name` values (exact):** `bool`, `int`, `float`, `byte`, `name`, `string`,
`text`, `Vector`, `Rotator`, `Transform`, `Vector2D`, `LinearColor`. Use `int` (**not** `int32`) and
`float` (**not** `double`) — `add_variable` rejects `int32`/`double` with
*"Unknown type 'int32'. Supported: …"*. For other engine structs (e.g. `FColor`, `FGameplayTag`) use
`add_struct_variable` with `struct_type`; for object/class refs use `add_object_variable` with
`object_class` as a `{refPath}`:

```python
# Object-reference variable — object_class is a {refPath} to the class
call_tool("add_object_variable", "editor_toolset.toolsets.blueprint.BlueprintTools",
    {"blueprint": {"refPath": "/Game/BP_MyActor.BP_MyActor"}, "name": "DeathFX",
     "object_class": {"refPath": "/Script/Niagara.NiagaraSystem"}})
```

### ⚠️ Adding a function graph — engine `BlueprintTools.add_function_graph`

Creating a function graph moved to the engine toolset (`create_function` / `add_function` on
`BlueprintService` were cut). Use `call_tool` with `blueprint` + `graph_name`:

```python
call_tool(
    tool_name="add_function_graph",
    toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
    arguments={"blueprint": {"refPath": "/Game/Blueprints/BP_MyActor.BP_MyActor"}, "graph_name": "DoThing"},
)
```

Refining the graph afterward still uses the surviving VibeUE delta methods
(`add_function_parameter`, `add_function_local_variable`, `get_function_parameters`,
`get_function_info`, `override_function`, `open_function_graph`).

### ⚠️ Method Name Gotchas

| WRONG | CORRECT |
|-------|---------|
| `get_component_info(path, name)` | `get_component_info(type)` - takes ONLY type! |

### ⚠️ Property Name Gotchas

| WRONG | CORRECT |
|-------|---------|
| `info.inputs` | `info.input_parameters` |
| `var.name` | `var.variable_name` |

### ⚠️ Python Naming: Boolean `b` Prefix Is Stripped

UE Python **strips the lowercase `b` prefix** from boolean properties. Always use the name without it:

| C++ name | Python name |
|---|---|
| `bAlreadyOverridden` | `already_overridden` |
| `bIsEventStyle` | `is_event_style` |
| `bIsNativeEvent` | `is_native_event` |
| `bEnabled` | `enabled` |

Do **not** use `b_already_overridden`, `b_is_event_style`, etc. — these will raise `AttributeError`.

**Exception:** the string-based CDO accessors `get_property` / `set_property` take the **native C++
property name**, so booleans KEEP the `b` prefix there (`"bReplicates"`, not `"replicates"`). See the
next section.

### ⚠️ CDO Properties: `get_property` / `set_property` (replication, lifespan, etc.)

Use these to read/write Class Default Object properties like replication or initial lifespan:

```python
import unreal
bp = "/Game/Blueprints/TestActor"

# READ — returns a single value: str, or None if the property wasn't found. NOT a (success, value) tuple!
value = unreal.BlueprintService.get_property(bp, "bReplicates")   # "True" / "False" (a STRING)
life  = unreal.BlueprintService.get_property(bp, "InitialLifeSpan")  # "5.000000"

# WRONG — raises ValueError: too many values to unpack
# success, value = unreal.BlueprintService.get_property(bp, "bReplicates")

# WRITE — values are strings; returns bool
unreal.BlueprintService.set_property(bp, "bReplicates", "True")

# Compile via the engine BlueprintTools toolset (compile_blueprint was cut from BlueprintService):
# call_tool(tool_name="compile_blueprint",
#           toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
#           arguments={"blueprint": {"refPath": "/Game/Blueprints/TestActor.TestActor"}})
unreal.EditorAssetLibrary.save_asset(bp)
```

Rules:
- Returns **str or None** — UE Python collapses `bool Func(..., Out&)` into one return value. This
  applies to every `get_*_info` method too (`get_variable_info`, `get_function_info`,
  `get_component_info`, `get_node_details`): they return the info struct or `None`, never a tuple.
- Property names are the **native C++ names**: `bReplicates`, `InitialLifeSpan`, `bCanBeDamaged` —
  the `b` prefix is NOT stripped here (a stripped name returns `None`).
- Values read back as strings: compare `value == "True"`, not `value is True`.

### ⚠️ Info Struct Fields (don't guess — these are the complete lists)

`get_blueprint_info(path)` → **BlueprintDetailedInfo**: `blueprint_name`, `blueprint_path`,
`parent_class`, `is_widget_blueprint`, `variables`, `functions`, `components`.

| Struct | Fields |
|---|---|
| `BlueprintFunctionInfo` (from `list_functions`) | `function_name`, `return_type`, `parameters`, `is_override`, `is_pure` — there is **no `function_type`** and **no `input_parameters`** (that's the Detailed struct) |
| `BlueprintFunctionDetailedInfo` (from `get_function_info`) | `function_name`, `graph_guid`, `input_parameters`, `output_parameters`, `local_variables`, `is_override`, `is_pure`, `node_count` — there is **no `return_type`**; return values are entries in `output_parameters` (each param: `parameter_name`, `parameter_type`, `is_output`, `is_reference`, `default_value`) |
| `BlueprintVariableInfo` | `variable_name`, `variable_type`, `category`, `is_public`, `is_exposed`, `default_value` |
| `BlueprintVariableDetailedInfo` (from `get_variable_info`) | `variable_name`, `variable_type`, `type_path`, `category`, `default_value`, `tooltip`, `is_array`, `is_set`, `is_map`, `is_instance_editable`, `is_blueprint_read_only`, `is_expose_on_spawn`, `is_expose_to_cinematics`, `is_private`, `replication_condition` — booleans use the `is_` prefix (**`instance_editable` alone raises AttributeError**) |
| `BlueprintLocalVariableInfo` (in `BlueprintFunctionDetailedInfo.local_variables`) | `variable_name`, `friendly_name`, `variable_type`, `display_type`, `default_value`, `category`, `guid`, `is_const`, `is_reference`, `is_array`, `is_set`, `is_map` — local variables are NOT parameters: there is **no `parameter_name`** |
| `BlueprintPinInfo` (from `get_node_pins` / `node.pins`) | `pin_name`, `pin_type`, `is_input`, `is_connected`, `default_value` — direction is the single `is_input` bool; there is **no `is_output`** (use `not pin.is_input`) |
| `BlueprintNodeTypeInfo` (from `discover_nodes`) | `display_name`, `category`, `spawner_key`, `node_class`, `tooltip`, `is_pure`, `is_latent`, `keywords` — the description text is `tooltip`, **not `description`** |
| `BlueprintComponentInfo` | `component_name`, `component_class`, `attach_parent`, `is_root_component`, `is_scene_component`, `is_inherited`, `children` |
| `BlueprintGraphInfo` (from `list_graphs`) | `graph_name`, `graph_kind`, `node_count` |
| `BlueprintNodeInfo` (from `get_nodes_in_graph`) | `node_id`, `node_type`, `node_title`, `pos_x`, `pos_y`, `pin_names`, `pins` |
| `BlueprintCompileResult` | `success`, `num_errors`, `num_warnings`, `errors`, `warnings` |
| `ComponentDetailedInfo` (from `get_component_info`) | `name`, `display_name`, `class_path`, `category`, `parent_class`, `is_scene_component`, `is_primitive_component`, `property_count`, `function_count` — there is **no `description`** |
| `ComponentTypeInfo` (from `get_available_components`) | `name`, `display_name`, `class_path`, `category`, `base_class`, `is_scene_component`, `is_primitive_component`, `is_abstract` |
| `ComponentPropertyInfo` (from `get_all_component_properties`) | `property_name`, `property_type`, `category`, `value`, `is_editable`, `is_inherited` |

### Component API quick reference (all on `unreal.BlueprintService`)

These signatures are complete — no need to `discover_python_class` them again:

| Method | Notes |
|---|---|
| `get_available_components(search_filter="", max_results=50)` → `[ComponentTypeInfo]` | Discover addable component types by partial name match |
| `get_component_info(component_type)` → `ComponentDetailedInfo` or `None` | Takes ONLY the type name — no blueprint path |
| `list_components(bp)` / `get_component_hierarchy(bp)` → `[BlueprintComponentInfo]` | Both return the same flat list — use `attach_parent` / `children` / `is_root_component` fields to render the tree |
| `component_exists(bp, name)` → `bool` | Fast idempotency check before adding |
| `add_component(bp, type, name, parent_name="")` → `bool` | Compiles inline. See root behavior below |
| `remove_component(bp, name, remove_children=True)` → `bool` | Removes children too by default |
| `reparent_component(bp, name, new_parent)` → `bool` | |
| `set_root_component(bp, name)` → `bool` | New root must be a SceneComponent. Old auto-generated DefaultSceneRoot is removed; an old user-created root and any other root-level scene components become children of the new root |
| `get_component_property(bp, comp, prop)` → `str` or `None` | |
| `set_component_property(bp, comp, prop, value)` → `bool` | `value` is a STRING — see formats below |
| `get_all_component_properties(bp, comp, include_inherited=True)` → `[ComponentPropertyInfo]` | `list_component_properties` is an alias |
| `compare_components(bp_a, comp_a, bp_b, comp_b)` → `str` or `None` | Diff as text; same or different blueprints |
| `set_collision_settings(bp, comp, collision_enabled, object_type, collision_profile, channel_responses)` → `bool` | Collision lives in `BodyInstance` — `set_component_property` cannot reach it |

**Root component behavior**: components added with no `parent_name` go to root level. The first
scene component added this way replaces the auto-generated DefaultSceneRoot and becomes the root;
later parentless scene components become floating siblings, NOT children of the root. To build a
hierarchy, pass `parent_name` when adding, or use `reparent_component` / `set_root_component`.

### ⚠️ Property Value String Formats (`set_component_property`)

Values are strings in UE export-text syntax. Check `property_type` via
`get_all_component_properties` before guessing a struct format:

| Property type | Format example |
|---|---|
| `float` / `int32` / `bool` | `"5000.0"`, `"25"`, `"true"` |
| `FVector` / `FRotator` | `"(X=0,Y=0,Z=50)"`, `"(Pitch=0,Yaw=90,Roll=0)"` |
| **`FColor`** (e.g. light `LightColor`) | `"(R=255,G=127,B=0,A=255)"` — **integer bytes 0–255** |
| **`FLinearColor`** | `"(R=1.0,G=0.5,B=0.0,A=1.0)"` — floats 0–1 |
| Enum (e.g. `IntensityUnits`) | `"Candelas"` |

❌ Common mistake: `LightColor` is `FColor`, so passing LinearColor-style floats like
`"(R=1,G=0.5,B=0)"` silently produces a nearly-black light (R=1 of 255), not orange.

### ⚠️ StateTree Dispatcher Variable Type

To add a StateTree delegate dispatcher variable to a Blueprint task (e.g. `STT_*`):

```python
# Use type "FStateTreeDelegateDispatcher" — NOT "EventDispatcher".
# add_variable is the engine BlueprintTools toolset call (BlueprintService.add_variable was cut):
call_tool(
    tool_name="add_variable",
    toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
    arguments={"blueprint": bp_path, "name": "FinishRotatingDispatcher", "type_name": "FStateTreeDelegateDispatcher"},
)
```

`"EventDispatcher"` is a Blueprint-only concept and is not a valid type string. The correct type
for StateTree delegate transitions is the USTRUCT `FStateTreeDelegateDispatcher`.
After adding this variable, use `bind_transition_to_delegate` on the StateTree to link it to a transition.

### Event Dispatchers (regular Blueprint multicast delegates)

Use the dedicated dispatcher API — **NOT** `add_variable` — to create the multicast delegates that appear in the Blueprint editor's *Event Dispatchers* section:

| Method | What it does |
|---|---|
| `add_event_dispatcher(bp_path, name)` | Adds the dispatcher (member variable + signature graph). Skeleton is recompiled inline; the dispatcher is callable immediately. |
| `add_event_dispatcher_parameter(bp_path, name, param_name, param_type, is_array=False, container_type="")` | Adds a parameter to the dispatcher's signature. Subscribers receive these as inputs. |
| `add_call_delegate_node(bp_path, graph, name, pos_x, pos_y)` | Spawns the "Call <Name>" broadcast node on a `UK2Node_CallDelegate`. Wire the broadcast into its `execute` pin. |
| `remove_event_dispatcher(bp_path, name)` | Removes the dispatcher and its signature graph. |

```python
import unreal

bp = "/Game/StateTree/BP_Cube"

# 1. Create the dispatcher (no compile needed before placing nodes — skeleton is regenerated inline)
unreal.BlueprintService.add_event_dispatcher(bp, "FinishedLooking")

# 2. (Optional) Add parameters to the signature
unreal.BlueprintService.add_event_dispatcher_parameter(bp, "FinishedLooking", "Direction", "FRotator")

# 3. Spawn the Call node and wire something into its execute pin
call_id = unreal.BlueprintService.add_call_delegate_node(bp, "EventGraph", "FinishedLooking", 1400, -700)
unreal.BlueprintService.connect_nodes(bp, "EventGraph", timeline_id, "Finished", call_id, "execute")

# 4. Compile (engine BlueprintTools toolset) + save
# call_tool(tool_name="compile_blueprint",
#           toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools",
#           arguments={"blueprint": bp})
unreal.EditorAssetLibrary.save_asset(bp)
```

**Pins on the Call node:** `execute` (input exec), `then` (output exec), `self` (input target — defaulted to Self), plus one input pin per signature parameter.

**Subscribing to it elsewhere:**
- **Preferred** — `add_delegate_bind_on_variable(other_bp, graph, variable_name, "FinishedLooking", x, y)` when the dispatcher lives on the class of an existing variable (e.g. `Cube : BP_Cube_C`). Mirrors `add_function_call_on_variable`: derives the owner class from the variable's type, creates the bind node + Get, and auto-wires Target. Pair with `add_custom_event_node` and wire `CustomEvent.OutputDelegate → Bind.Delegate`.
- **Lower-level** — `add_delegate_bind_node(other_bp, graph, "<owner class>", "FinishedLooking", x, y)` when you have no variable to derive from. `<owner class>` accepts `"Self"`, native class names (with/without U/A prefix), Blueprint asset paths (`/Game/.../BP_Cube`), or short BP names with/without `_C` (`BP_Cube`, `BP_Cube_C`). You wire the Target pin yourself.

**Common mistakes:**

- ❌ `add_variable(bp, "FinishedLooking", "EventDispatcher")` — `"EventDispatcher"` is not a real type string; use `add_event_dispatcher` instead.
- ❌ Treating the dispatcher like a regular variable (`add_get_variable_node` on it produces a delegate-getter, not a broadcast). The broadcast node is `UK2Node_CallDelegate` and is created only by `add_call_delegate_node`.
- ❌ Skipping the compile before saving the asset. The skeleton compiles inline on dispatcher creation, but the final asset still needs the engine `BlueprintTools.compile_blueprint` toolset call before `save_asset` to ensure the generated class is up-to-date.

### Blueprint Interfaces (implement / remove)

Add or remove a Blueprint Interface on a Blueprint class — the equivalent of *Class Settings → Interfaces → Add* in the editor.

| Method | What it does |
|---|---|
| `add_interface(bp_path, interface)` | Implements the interface on the Blueprint. No-op (returns `True`) if it is already implemented. Marks the Blueprint structurally modified and recompiles it inline. |
| `remove_interface(bp_path, interface)` | Removes the interface from the Blueprint. Marks structurally modified and recompiles inline. |

```python
import unreal

bp = "/Game/Blueprints/BP_Player"

# Prefer the FULL interface asset path — most reliable resolution
unreal.BlueprintService.add_interface(bp, "/Game/Interfaces/BPI_Interactable")

# Short name also works IF the interface asset is already loaded in the editor
unreal.BlueprintService.add_interface(bp, "BPI_Interactable")

# Persist (both methods compile inline, but still save the asset)
unreal.EditorAssetLibrary.save_asset(bp)

# Remove it later
unreal.BlueprintService.remove_interface(bp, "BPI_Interactable")
unreal.EditorAssetLibrary.save_asset(bp)
```

**Notes & gotchas:**

- **Prefer the full asset path** (e.g. `/Game/Interfaces/BPI_Interactable`). Short-name resolution only finds interface Blueprints that are *already loaded* in the editor; a full path always loads and resolves.
- Both methods **recompile the Blueprint inline** (`CompileBlueprint`) but do **not** save it — call `EditorAssetLibrary.save_asset(bp)` afterward to persist.
- `add_interface` is idempotent — calling it for an interface that is already implemented returns `True` without duplicating it.
- After adding an interface, its functions appear as overridable events/functions; use `override_function(bp_path, function_name)` to implement them in the graph.
- Both return `bool` (`True` on success). A `False` from `add_interface` means either the interface path could not be resolved, or it resolved to an asset that is **not a Blueprint Interface** (e.g. a normal Blueprint or one merely parented to `UInterface`) — check the log and pass a real Blueprint Interface asset path.
- The interface must be a true Blueprint Interface (`BPTYPE_Interface`), created with `BlueprintInterfaceFactory`. `add_interface` validates this and returns `False` for non-interface classes rather than letting the compile fail.

---

## Task Index

| Task | Workflow | Sample script (run via `execute_python_code`) |
|------|----------|-----------------------------------------------|
| Create a Blueprint + variables | `workflows.md` → Create a Blueprint | `scripts/create_blueprint.txt` |
| Add components + set properties | `workflows.md` → Add components | `scripts/add_component.txt` |
| Add an event dispatcher (delegate) | `workflows.md` → Event dispatcher | `scripts/event_dispatcher.txt` |
| Implement a Blueprint interface | `workflows.md` → Implement an interface | `scripts/add_interface.txt` |
| Inspect a Blueprint (components, parent class) | `introspection.md` | — |
| Node-level graph editing | load the **`blueprint-graphs`** skill | — |

## Sub-docs

- **`workflows.md`** — step-by-step create/component/dispatcher/interface workflows with copy-paste Python.
- **`introspection.md`** — what Blueprint introspection works/doesn't in UE 5.7 (CDO reads, parent class via
  asset tags, listing BPs by class, getting level actors).

## Verification

After any edit: compile via the engine `BlueprintTools.compile_blueprint` toolset call
(`call_tool(tool_name="compile_blueprint", toolset_name="editor_toolset.toolsets.blueprint.BlueprintTools", arguments={"blueprint": path})`)
and check the result's `success` / `num_errors`, then `unreal.EditorAssetLibrary.save_asset(path)`.
Don't claim success until compile reports zero errors.

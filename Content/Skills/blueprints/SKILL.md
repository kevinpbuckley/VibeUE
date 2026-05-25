---
name: blueprints
display_name: Blueprint System
description: Create and modify Blueprint assets, variables, functions, and components
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

# Blueprint System Skill

> **For node-level graph editing** (adding nodes, connecting pins, wiring logic, timers, layout), load the `blueprint-graphs` skill.

## Critical Rules

### ⚠️ `create_blueprint()` Signature

```python
# CORRECT - Three separate arguments: name, parent, folder
unreal.BlueprintService.create_blueprint("BP_MyActor", "Actor", "/Game/Blueprints")

# WRONG - Path as first argument
unreal.BlueprintService.create_blueprint("/Game/Blueprints/BP_MyActor", "Actor")
```

**Returns**: Full asset path like `/Game/Blueprints/BP_MyActor.BP_MyActor`

### Blueprint Types in add_variable

When adding a variable whose type is a Blueprint class, use the Blueprint asset name directly.
Do **not** add `_C`, do **not** construct a generated class path — just use the name:

```python
# CORRECT — Blueprint asset name, short or full path
unreal.BlueprintService.add_variable("/Game/BP_MyActor", "Target", "BP_Enemy")
unreal.BlueprintService.add_variable("/Game/BP_MyActor", "Target", "/Game/Blueprints/BP_Enemy")

# WRONG — do not guess generated class names or append _C
unreal.BlueprintService.add_variable("/Game/BP_MyActor", "Target", "BP_Enemy_C")
unreal.BlueprintService.add_variable("/Game/BP_MyActor", "Target", "/Game/Blueprints/BP_Enemy_C")
```

The type system resolves Blueprint names automatically via asset search.

### ⚠️ Method Name Gotchas

| WRONG | CORRECT |
|-------|---------|
| `add_function()` | `create_function()` |
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

### ⚠️ StateTree Dispatcher Variable Type

To add a StateTree delegate dispatcher variable to a Blueprint task (e.g. `STT_*`):

```python
# Use type "FStateTreeDelegateDispatcher" — NOT "EventDispatcher"
unreal.BlueprintService.add_variable(bp_path, "FinishRotatingDispatcher", "FStateTreeDelegateDispatcher")
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

# 4. Compile + save
unreal.BlueprintService.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_asset(bp)
```

**Pins on the Call node:** `execute` (input exec), `then` (output exec), `self` (input target — defaulted to Self), plus one input pin per signature parameter.

**Subscribing to it elsewhere:**
- **Preferred** — `add_delegate_bind_on_variable(other_bp, graph, variable_name, "FinishedLooking", x, y)` when the dispatcher lives on the class of an existing variable (e.g. `Cube : BP_Cube_C`). Mirrors `add_function_call_on_variable`: derives the owner class from the variable's type, creates the bind node + Get, and auto-wires Target. Pair with `add_custom_event_node` and wire `CustomEvent.OutputDelegate → Bind.Delegate`.
- **Lower-level** — `add_delegate_bind_node(other_bp, graph, "<owner class>", "FinishedLooking", x, y)` when you have no variable to derive from. `<owner class>` accepts `"Self"`, native class names (with/without U/A prefix), Blueprint asset paths (`/Game/.../BP_Cube`), or short BP names with/without `_C` (`BP_Cube`, `BP_Cube_C`). You wire the Target pin yourself.

**Common mistakes:**

- ❌ `add_variable(bp, "FinishedLooking", "EventDispatcher")` — `"EventDispatcher"` is not a real type string; use `add_event_dispatcher` instead.
- ❌ Treating the dispatcher like a regular variable (`add_get_variable_node` on it produces a delegate-getter, not a broadcast). The broadcast node is `UK2Node_CallDelegate` and is created only by `add_call_delegate_node`.
- ❌ Skipping `compile_blueprint` before saving the asset. The skeleton compiles inline on dispatcher creation, but the final asset still needs `compile_blueprint` before `save_asset` to ensure the generated class is up-to-date.

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

## Workflows

### Create Blueprint with Variables

```python
import unreal

existing = unreal.AssetDiscoveryService.find_asset_by_path("/Game/BP_Player")
if not existing:
    path = unreal.BlueprintService.create_blueprint("Player", "Character", "/Game/")
    unreal.BlueprintService.add_variable(path, "Health", "float", "100.0")
    unreal.BlueprintService.add_variable(path, "IsAlive", "bool", "true")
    unreal.BlueprintService.compile_blueprint(path)
    unreal.EditorAssetLibrary.save_asset(path)
```

### Add Component with Properties

```python
import unreal

bp_path = "/Game/BP_Enemy"

unreal.BlueprintService.add_component(bp_path, "StaticMeshComponent", "BodyMesh")
unreal.BlueprintService.add_component(bp_path, "PointLightComponent", "Glow", "BodyMesh")  # Child
unreal.BlueprintService.compile_blueprint(bp_path)

unreal.BlueprintService.set_component_property(bp_path, "BodyMesh", "bVisible", "true")
unreal.BlueprintService.set_component_property(bp_path, "Glow", "Intensity", "5000.0")
unreal.EditorAssetLibrary.save_asset(bp_path)
```

---

## Blueprint Introspection — What Works and What Doesn't (UE 5.7)

### Getting Level Actors

```python
# CORRECT
actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_subsys.get_all_level_actors()

# WRONG — deprecated, blocked by VibeUE in UE 5.7
unreal.EditorLevelLibrary.get_all_level_actors()
```

### Finding Blueprint Assets

```python
# CORRECT — use package_name or package_path
ar = unreal.AssetRegistryHelpers.get_asset_registry()
assets = ar.get_assets_by_path('/Game', recursive=True)
print(assets[0].package_name)   # /Game/Blueprints/BP_MyActor
print(assets[0].package_path)   # /Game/Blueprints

# WRONG — object_path does not exist on AssetData in UE 5.7
assets[0].object_path  # AttributeError
```

### Inspecting Blueprint Components

Direct Blueprint introspection APIs are largely missing or protected in UE 5.7 Python:

| What you might try | Result |
|---|---|
| `bp.get_editor_property('simple_construction_script')` | Blocked — protected property |
| `bp.simple_construction_script` | AttributeError — not exposed |
| `scs.get_all_nodes()` | Runtime crash ("^") |
| `BlueprintEditorLibrary.get_blueprint_component_names()` | Doesn't exist in UE 5.7 |
| `SubobjectDataSubsystem.k2_gather_subobject_data_for_blueprint(bp)` | Returns handles but `SubobjectData` exposes no properties from Python |
| `bp.get_editor_property('parent_class')` | RuntimeError — `parent_class` not an editor property on Blueprint |
| `bp.parent_class` | AttributeError — not exposed |

**Working pattern — CDO (preferred, no spawn needed):**

```python
import unreal

bp = unreal.EditorAssetLibrary.load_asset("/Game/Blueprints/BP_MyActor")
# Note: unreal.load_asset() returns None on a fresh session; use EditorAssetLibrary.load_asset()

gc = bp.generated_class()
cdo = unreal.get_default_object(gc)   # module-level call; read-only is allowed

for comp in cdo.get_components_by_class(unreal.ActorComponent):
    print(f"{comp.get_class().get_name()}: {comp.get_name()}")
```

CDO modification is still blocked — `unreal.get_default_object(gc).set_editor_property(...)` raises `PYTHON_UNSAFE_CODE`.

**Getting parent class — use asset registry tag:**

```python
import unreal

ar = unreal.AssetRegistryHelpers.get_asset_registry()
assets = ar.get_assets_by_path("/Game/Blueprints", False)
for a in assets:
    if "BP_MyActor" in str(a.asset_name):
        print(a.get_tag_value("ParentClass"))        # e.g. "/Script/Engine.Actor"
        print(a.get_tag_value("NativeParentClass"))  # same, native C++ parent
        break
```

**Listing all Blueprints by class — use `get_assets_by_class`:**

```python
import unreal

# ARFilter properties cannot be set after construction — don't use ARFilter for class filtering.
# asset_class_names kwarg does not exist. Use get_assets_by_class instead:
ar = unreal.AssetRegistryHelpers.get_asset_registry()
bp_class = unreal.TopLevelAssetPath("/Script/Engine", "Blueprint")
assets = ar.get_assets_by_class(bp_class, True)   # True = include derived classes
for a in assets:
    print(f"{a.package_path}/{a.asset_name}")
```

### Material Parameters

```python
# Works cleanly
import unreal
mat = unreal.load_asset('/Game/Materials/M_MyMaterial')
names = unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)
print(names)
```

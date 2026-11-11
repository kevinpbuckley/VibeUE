---
name: blueprint-tasks
description: Create and edit STT_* StateTree Blueprint Tasks - discover parent class, create via the engine BlueprintFactory, ReceiveLatentTick / ReceiveLatentEnterState event functions, registered task type names, Epic's add_blueprint_* tools for Blueprint nodes, and where to read tree structure now that get_state_tree_info is gone
---

This sub-doc continues from skill.md → "Creating & Editing StateTree Blueprint Tasks".

## Creating & Editing StateTree Blueprint Tasks

StateTree tasks can be written as Blueprints (`STT_*` assets, parent class `StateTreeTaskBlueprintBase`).
**Load the `blueprints` skill before doing any of the following** — `StateTreeService` cannot
edit Blueprint internals:

- Creating a new STT Blueprint
- Adding variables or components
- Overriding functions (`GetDescription`, `ReceiveLatentTick`, `ReceiveLatentEnterState`, etc.)
- Adding nodes or wiring pins in a function graph

**Always discover the exact class name first**, then create with the engine's
`BlueprintFactory` (`BlueprintService.create_blueprint` was removed in the Epic-overlap
consolidation — Blueprint creation is engine-side now).

### Discovery Workflow

```python
# Step 1: Discover available StateTree base classes
result = search_symbols("unreal", name_filter="StateTreeTask")
# Review returned names — look for the blueprint-safe base class
# Typical result: "StateTreeTaskBlueprintBase" (short name used directly below)

# Step 2: Create with the engine BlueprintFactory, ParentClass = the class from Step 1
factory = unreal.BlueprintFactory()
factory.set_editor_property("ParentClass", unreal.StateTreeTaskBlueprintBase)
bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "STT_MyTask", "/Game/StateTree", unreal.Blueprint, factory)
assert bp, "create_asset returned None — folder invalid or an asset with that name already exists"
print(f"Created: {bp.get_path_name()}")
```

`unreal.<ClassName>` must exist for the discovered short name (it does for any loaded class);
if Python raises AttributeError, the plugin that defines the class is not loaded.

### ⚠️ Never Guess the Parent Class — Discover First

```python
# WRONG — guessing; a plain Actor parent is not usable as a StateTree task
factory.set_editor_property("ParentClass", unreal.Actor)

# CORRECT — discover the exact class first, then set it as ParentClass
# search_symbols("unreal", name_filter="StateTreeTask") → confirms "StateTreeTaskBlueprintBase"
factory.set_editor_property("ParentClass", unreal.StateTreeTaskBlueprintBase)
```

### StateTree Task Blueprint Event Functions

When adding event nodes to a `StateTreeTaskBlueprintBase` Blueprint, use **only** the non-deprecated
function names. The deprecated versions (`ReceiveTick`, `ReceiveEnterState`) have a different
signature and cause compile errors.

| Purpose | Correct Function | WRONG (deprecated) |
|---------|------------------|--------------------|
| Tick every frame | `ReceiveLatentTick` | ~~`ReceiveTick`~~ |
| On state enter | `ReceiveLatentEnterState` | ~~`ReceiveEnterState`~~ |
| On state exit | `ReceiveExitState` | — |
| On state completed | `ReceiveStateCompleted` | — |

`BlueprintService.add_event_node` was removed; events are `BlueprintToolsets.BlueprintToolset.AddEvent`.
The event *names* below are the part that matters and they have not changed.

```
# bp_path = "/Game/StateTree/STT_MyTask"

# Tick — MUST be ReceiveLatentTick, NOT the deprecated ReceiveTick
call_tool AddEvent {"blueprint": {"refPath": "<bp_path>"}, "eventName": "ReceiveLatentTick"}

# Enter State — the latent form, not ReceiveEnterState
call_tool AddEvent {"blueprint": {"refPath": "<bp_path>"}, "eventName": "ReceiveLatentEnterState"}

# Exit State
call_tool AddEvent {"blueprint": {"refPath": "<bp_path>"}, "eventName": "ReceiveExitState"}

call_tool CompileBlueprint {"blueprint": {"refPath": "<bp_path>"}}
call_tool SaveAsset        {"asset": {"refPath": "<bp_path>"}}        # EditorToolset.AssetToolset
```

`ListCompatibleEventFunctions` lists what a given Blueprint will accept, which beats typing an
event name from memory. Run `describe_toolset BlueprintToolsets.BlueprintToolset` for AddEvent's
exact argument names and whether it takes a position.

The `ReceiveLatentTick` node pins:
- **DeltaTime** (float, output) — elapsed frame time

The `ReceiveLatentEnterState` node pins:
- **Transition** (FStateTreeTransitionResult, output) — data about the entering transition

### After Creation: Find the Registered Task Type Name

Blueprint tasks register under a `_C`-suffixed name. `get_available_task_types()` — a VibeUE
survivor — returns plain strings:

```python
for t in unreal.StateTreeService.get_available_task_types():
    if "MyTask" in t:
        print(t)  # e.g. "STT_MyTask_C"
```

### ⚠️ Use Epic's Blueprint Variant, Not the Struct One

Adding a node is Epic's job now, and Epic ships a **dedicated Blueprint variant for every node
kind.** For an `STT_*` / `STE_*` / `STC_*` Blueprint node use these, through `call_tool`:

| Blueprint node | Epic tool | NOT the struct variant |
|---|---|---|
| `STT_*` state task | `add_blueprint_task` | ~~`add_task`~~ |
| `STT_*` global task | `add_blueprint_global_task` | ~~`add_global_task`~~ |
| `STE_*` evaluator | `add_blueprint_evaluator` | ~~`add_evaluator`~~ |
| `STC_*` enter condition | `add_blueprint_enter_condition` | ~~`add_enter_condition`~~ |
| Blueprint consideration | `add_blueprint_consideration` | ~~`add_consideration`~~ |

The struct variants take a `UScriptStruct`, so they cannot express a Blueprint class. They all live
on `toolset_name="state_tree_toolset.state_tree.StateTreeToolset"`; run `describe_toolset` on it for
what each `add_blueprint_*` expects rather than guessing an argument list.

Two things that have not changed:

- **Compile the Blueprint first.** A Blueprint that is edited but not compiled, or that has compile
  errors, is not a usable node class. A copied `SKEL_<Name>_C` skeleton name resolves to the
  compiled `<Name>_C` class; other internal names (`REINST_`, `TRASHCLASS_`) are not usable.
- **Never pass `StateTreeBlueprintTaskWrapper` to an adder,** and never report it as the user-level
  outcome. It is the internal wrapper struct. Report the Blueprint task's own name.

(VibeUE's binding tools, by contrast, accept the wrapper name as one of several aliases for an
existing node — see `api-reference.md`.)

### Reading a Tree's Structure and Node Fields

`get_state_tree_info` is gone, and with it the `FStateTreeInfo` / `FStateTreeStateInfo` /
`FStateTreeNodeInfo` / `FStateTreeTransitionInfo` payloads — no surviving VibeUE tool returns them.
Read the same facts from the engine instead:

| What you used to read off the info structs | Where it comes from now |
|---|---|
| The state hierarchy (`all_states`, `child_paths`) | Epic `get_root_states`, `get_all_states`, `get_children` |
| A node's display text (`name`) | Epic `get_node_description` |
| A node's property values, and the task/condition/consideration/evaluator fields | Epic `get_node_instance_data` — the returned keys *are* the property names |
| A state's `tag`, `description`, `weight`, `tasks_completion`, `state_type`, `selection_behavior`, `enabled` | `ObjectToolset.GetProperties` on the `UStateTreeState` (keys `Tag`, `Description`, `Weight`, `TasksCompletion`, `Type`, `SelectionBehavior`, `bEnabled`) |
| A state's transitions and their conditions / operands | `ObjectToolset.GetProperties` on the state, key `Transitions` |
| `root_parameters` | `ObjectToolset.GetProperties` on the editor data, key `RootParameters` |
| The context actor class | `ObjectToolset.GetProperties` on the editor data for `Schema`, then on that schema object |
| A transition's event payload fields | VibeUE `get_transition_event_payload_property_names` (still a VibeUE tool) |
| A transition condition's properties | VibeUE `get_transition_condition_property_names` (still a VibeUE tool) |

`b_expanded` has no read path at all any more: `bExpanded` is invisible to the toolset, and VibeUE's
`set_state_expanded` is write-only.

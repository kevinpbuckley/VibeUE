---
name: blueprint-tasks
description: Create and edit STT_* StateTree Blueprint Tasks - discover parent class, create via the engine BlueprintFactory, ReceiveLatentTick / ReceiveLatentEnterState event functions, registered task type names, and extended FStateTree* info fields
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
result = discover_python_module("unreal", name_filter="StateTreeTask")
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
# discover_python_module("unreal", name_filter="StateTreeTask") → confirms "StateTreeTaskBlueprintBase"
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

```python
import unreal

bp_path = "/Game/StateTree/STT_MyTask"

# Add Tick event — MUST use ReceiveLatentTick, NOT ReceiveTick
tick_id = unreal.BlueprintService.add_event_node(bp_path, "EventGraph", "ReceiveLatentTick", 0, 0)
print(f"Tick node: {tick_id}")

# Add Enter State event
enter_id = unreal.BlueprintService.add_event_node(bp_path, "EventGraph", "ReceiveLatentEnterState", 0, 200)
print(f"Enter node: {enter_id}")

# Add Exit State event
exit_id = unreal.BlueprintService.add_event_node(bp_path, "EventGraph", "ReceiveExitState", 0, 400)
print(f"Exit node: {exit_id}")

unreal.BlueprintEditorLibrary.compile_blueprint(unreal.EditorAssetLibrary.load_asset(bp_path))
unreal.EditorAssetLibrary.save_asset(bp_path)
```

The `ReceiveLatentTick` node pins:
- **DeltaTime** (float, output) — elapsed frame time

The `ReceiveLatentEnterState` node pins:
- **Transition** (FStateTreeTransitionResult, output) — data about the entering transition

### After Creation: Find the Registered Task Type Name

Blueprint tasks register under a `_C`-suffixed name. Use `get_available_task_types()` to find it:

```python
types = unreal.StateTreeService.get_available_task_types()
for t in types:
    if "MyTask" in t.type_name:
        print(t.type_name)  # e.g. "STT_MyTask_C"
```

### Blueprint Task Add Rule (No Wrapper-First Guidance)

- Prefer adding blueprint tasks by their registered type name (for example, `STT_Rotate_C`) via `StateTreeService.add_task`.
- Do not present `StateTreeBlueprintTaskWrapper` as the user-level task outcome when a named blueprint task type exists.
- If wrapper internals are used by the service implementation, keep that internal and report the added task as the blueprint task name.

### Extended Info Available in get_state_tree_info

`FStateTreeInfo` now includes `root_parameters` (list of `FStateTreeParameterInfo`).

`FStateTreeStateInfo` now includes:
- `tag` (str) — gameplay tag string, empty if none
- `description` (str) — editor description
- `weight` (float) — utility weight
- `tasks_completion` (str) — "Any" or "All"
- `b_has_custom_tick_rate` (bool)
- `custom_tick_rate` (float)
- `required_event_tag` (str) — required event tag to enter, empty if none
- `enter_condition_operands` (list of str) — "Copy", "And", or "Or" per enter condition
- `considerations` (list of `FStateTreeNodeInfo`) — utility AI considerations; each node's `struct_type` is the full struct name (e.g. `"StateTreeConstantConsideration"`)

`FStateTreeNodeInfo` now includes:
- `considered_for_completion` (bool) — whether task contributes to state completion (tasks only)
- `operand` (str) — "Copy", "And", or "Or" (conditions only)

`FStateTreeTransitionInfo` now includes:
- `index` (int) — zero-based index within the state's transitions array
- `delay_transition` (bool); read-back also exposes `enabled` (bool)
- `delay_duration` (float)
- `delay_random_variance` (float)
- `required_event_tag` (str)
- `event_payload_struct` (str) — payload struct type name (e.g. "FStartChasingPayload"), empty if none
- `conditions` (list of `FStateTreeNodeInfo`)
- `condition_operands` (list of str)

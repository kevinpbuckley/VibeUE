---
name: blueprint-tasks
description: Create and edit STT_* StateTree Blueprint Tasks - discover parent class, create_blueprint, ReceiveLatentTick / ReceiveLatentEnterState event functions, registered task type names, and extended FStateTree* info fields
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

**Always discover the exact class name first** — the same discovery pattern works for both
`create_blueprint` and `reparent_blueprint`.

### Discovery Workflow

```python
# Step 1: Discover available StateTree base classes
result = discover_python_module("unreal", name_filter="StateTreeTask")
# Review returned names — look for the blueprint-safe base class
# Typical result: "StateTreeTaskBlueprintBase" (short name used directly below)

# Step 2: Use the exact name from discovery — create_blueprint resolves it via object search
path = unreal.BlueprintService.create_blueprint(
    "STT_MyTask",               # blueprint name
    "StateTreeTaskBlueprintBase",  # exact name from Step 1
    "/Game/StateTree"           # destination folder
)
assert path, "create_blueprint returned empty — class name not found or plugin not loaded"
print(f"Created: {path}")
```

The short class name (e.g. `"StateTreeTaskBlueprintBase"`) is resolved via a full object search
across all loaded modules — the same string works for both `create_blueprint` and `reparent_blueprint`.
If `create_blueprint` returns an empty string, the class was not found.

### ⚠️ Never Guess the Parent Class — Discover First

```python
# WRONG — guessing; creates a plain Actor, not usable as a StateTree task
unreal.BlueprintService.create_blueprint("STT_MyTask", "Actor", "/Game/StateTree")

# CORRECT — discover exact name, then pass it
# discover_python_module("unreal", name_filter="StateTreeTask") → confirms "StateTreeTaskBlueprintBase"
unreal.BlueprintService.create_blueprint("STT_MyTask", "StateTreeTaskBlueprintBase", "/Game/StateTree")
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

unreal.BlueprintService.compile_blueprint(bp_path)
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

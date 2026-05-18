---
name: state-trees
display_name: StateTree Behavior
description: Create, inspect, and edit StateTree assets for AI behavior, game logic, and character state machines
vibeue_classes:
  - StateTreeService
unreal_classes:
  - UStateTree
  - UStateTreeEditorData
  - UStateTreeState
  - FStateTreeTransition
  - FStateTreeEditorNode
keywords:
  - statetree
  - state tree
  - state machine
  - behavior
  - ai
  - transitions
  - evaluator
  - task
  - condition
  - color
  - theme color
  - theme
  - expand
  - collapse
  - blueprint task
  - stt
  - override function
  - get description
  - override
---

# StateTree Skill

StateTree is Unreal Engine's hierarchical state machine system for AI behavior and game logic.
This skill covers creating and editing StateTree assets via `unreal.StateTreeService`.

## ⚠️ StateTree Blueprint Tasks Require the Blueprints Skill

**StateTree Tasks prefixed `STT_` (e.g. `STT_Rotate`, `STT_Move`) are Blueprint assets.**
They are edited with `BlueprintService`, not `StateTreeService`.

**Always load the `blueprints` skill** when the user asks to:
- Add or edit variables on an STT task
- Override functions (`GetDescription`, `ReceiveLatentTick`, `ReceiveLatentEnterState`, etc.)
- Add Blueprint nodes or connect pins inside an STT task graph
- Inspect or modify STT task logic

**Also load the `blueprint-graphs` skill** when the task involves node wiring, timers, custom events,
pin names, or EventGraph layout inside an `STT_*` Blueprint.

```
StateTreeService  → edits the StateTree ASSET (states, tasks list, transitions, parameters)
BlueprintService  → edits the STT Blueprint CONTENT (variables, function graphs, node wiring)
```

Use `manage_skills(action='load', skill_name='blueprints')` before writing any code that
touches an `STT_*` Blueprint's internals.

If the request mentions timers, delayed completion, event callbacks, or screenshots of Blueprint graphs,
also load:

```python
manage_skills(action='load', skill_name='blueprint-graphs')
```

## ⚠️ STT Graph Editing Rules

When editing `STT_*` Blueprint task graphs:

1. Use `override_function()` for StateTree task events like `ReceiveLatentEnterState`.
2. Find the created event node by its **graph title** such as `Event EnterState`, not by the raw function name.
3. For node wiring, timer callbacks, and custom events, follow the detailed workflow in the `blueprint-graphs` skill.
4. For non-blocking waits, prefer `Set Timer by Event`, not `Delay`.
5. If the requested callback is a custom event, the callback must exist as a real `Custom Event` node in `EventGraph`, not as a `Create Event` delegate node plus a separate function graph.
6. Use `FinishTask` with pin name `bSucceeded`.
7. After wiring, always inspect `get_nodes_in_graph()`, `get_connections()`, and `compile_blueprint(...).success` before claiming success.
8. For complex graphs, create and verify one node at a time before creating the next node.
9. If any node-create step fails, stop, audit the graph, clean up orphaned nodes with `delete_node()` / `disconnect_pin()`, and only then retry.

### STT Strict Graph Build Mode

For `STT_*` task graphs built from screenshots or multi-node requests, use this execution order:

1. Create one node.
2. Confirm its GUID exists in a fresh `get_nodes_in_graph()` result.
3. Inspect its pins with `get_node_pins()`.
4. Only then create the next node.
5. After all nodes are proven present, connect one wire at a time and verify each new edge appears in `get_connections()`.

Do not batch unresolved function-call nodes into a single tool call. If the graph depends on nodes such as `Get Actor Location`, `Set Actor Location`, or `VInterp To`, discover the exact node first through the Blueprint graph workflow instead of guessing the underlying function name.

### STT Completion Contract

For `STT_*` graph edits, do not report success until the output explicitly shows:

1. `Event EnterState` exists.
2. `Set Timer by Event` exists.
3. The callback event node exists in a fresh node listing and is the requested node type.
4. The expected execution and delegate connections exist.
5. `Finish Task.bSucceeded` is set correctly.
6. Compile succeeds.

If compile succeeds but any of the checks above fail, the graph is still wrong.
If compile succeeds but some required node IDs were empty during creation, the graph is still wrong.

## Key Concepts

| Concept | Description |
|---------|-------------|
| **StateTree Asset** | The `.uasset` file containing the tree definition |
| **Subtree / Root State** | Top-level state (usually named "Root") — created with empty `parent_path` |
| **State** | A node in the tree; can have tasks, enter conditions, transitions, and child states |
| **Task** | Logic that runs while a state is active. Can be a C++ struct (`FStateTreeDelayTask`) or a **Blueprint asset** (`STT_MyTask`) |
| **Blueprint Task** | An `STT_*` Blueprint asset extending `StateTreeTaskBlueprintBase` — edited with `BlueprintService` |
| **Evaluator** | Global computation that runs every tick; provides data to all states |
| **Global Task** | Task that runs as long as the StateTree is active |
| **Transition** | Rule that moves execution from one state to another |
| **Compile** | Converts editor data to runtime format — **required after any structural change** |

## State Paths

States are addressed with `/`-separated paths starting from the subtree name:

```
Root                → top-level subtree
Root/Walking        → child of Root named Walking
Root/Walking/Idle   → child of Walking named Idle
```

## Workflow

```python
import unreal

# 1. Create the asset
unreal.StateTreeService.create_state_tree("/Game/AI/MyBehavior")

# 2. Build hierarchy (empty parent_path = new top-level subtree)
unreal.StateTreeService.add_state("/Game/AI/MyBehavior", "", "Root")
unreal.StateTreeService.add_state("/Game/AI/MyBehavior", "Root", "Idle")
unreal.StateTreeService.add_state("/Game/AI/MyBehavior", "Root", "Walking")
unreal.StateTreeService.add_state("/Game/AI/MyBehavior", "Root", "Attacking")

# 3. Add tasks
unreal.StateTreeService.add_task("/Game/AI/MyBehavior", "Root/Idle", "FStateTreeDelayTask")

# 4. Add transitions
unreal.StateTreeService.add_transition(
    "/Game/AI/MyBehavior", "Root/Idle",
    "OnStateCompleted", "GotoState", "Root/Walking")

unreal.StateTreeService.add_transition(
    "/Game/AI/MyBehavior", "Root/Walking",
    "OnStateCompleted", "Succeeded")

# 5. Compile — always required after changes
result = unreal.StateTreeService.compile_state_tree("/Game/AI/MyBehavior")
if not result.success:
    print("Errors:", result.errors)

# 6. Save
unreal.StateTreeService.save_state_tree("/Game/AI/MyBehavior")

# 7. Select the last state you modified so the user can see it
unreal.VibeUEService.manage_asset(action="open", asset_path="/Game/AI/MyBehavior")
unreal.StateTreeService.set_state_expanded("/Game/AI/MyBehavior", "Root", True)
unreal.StateTreeService.select_state("/Game/AI/MyBehavior", "Root/Walking")  # select whichever state you just edited
```

## Sub-docs available

This skill's larger reference material has been split into sibling files. Load them with
`manage_skills(action='load', skill_name='<name>')` when you need the detail:

- **`api-reference`** — `StateTreeService` API for asset/state discovery, asset creation, state management (add, type, move, remove, enable, select), and per-state tasks (add, inspect, set property — including the deterministic property pattern and `FStateTreeDebugTextTask` notes).
- **`api-bindings`** — Evaluators & global tasks (add, inspect, bind), transitions (all triggers/types/priorities plus the full `OnDelegate` workflow), compile & save, setting the context actor class, property bindings (task → context / root parameter / global task / evaluator), assigning a StateTree to a `StateTreeComponent` (use `StateTreeRef` not `StateTree`), theme colors, expand/collapse, and the "service first vs `execute_python_code`" guidance.
- **`event-payloads`** — Sending a StateTree event with a struct payload from a Blueprint: the `Make <FMyStruct>` → `Make Instanced Struct` → `Make State Tree Event` → `Send State Tree Event` chain, `build_graph` pattern, pin names to verify, and common mistakes.
- **`changelog`** — Phase 1 & 2 new methods: state properties (`set_selection_behavior`, `rename_state`, `set_state_tag`, `set_state_weight`, `set_task_considered_for_completion`), root parameters (`get_root_parameters`, `add_or_update_root_parameter`, `remove`/`rename_root_parameter`), per-instance overrides on placed actors (`get_component_state_tree_path`, `set_component_parameter_override`), transition editing (`update_transition`, `move_transition`), extended task management (`remove_task`, `move_task`, `set_task_enabled`), Utility AI considerations (Constant, FloatInput, EnumInput — add/inspect/set/bind), conditions (enter & transition — add/remove/operand/set/bind), and extended evaluator/global task management.
- **`blueprint-tasks`** — Creating & editing `STT_*` StateTree Blueprint Tasks: discovery workflow for the parent class, `create_blueprint` usage, correct event functions (`ReceiveLatentTick`, `ReceiveLatentEnterState`, `ReceiveExitState`, `ReceiveStateCompleted`), finding the registered `_C` task type name after creation, and the extended fields available on `FStateTreeInfo` / `FStateTreeStateInfo` / `FStateTreeNodeInfo` / `FStateTreeTransitionInfo`.

## COMMON_MISTAKES

### ⚠️ "Color" Means Theme Color, Not Materials

When a user asks to rename, change, or list "colors" on a StateTree, they mean **theme colors** —
the editor-only color labels in the StateTree's global color table. Do NOT load the materials skill
or look for material parameters. Use `get_theme_colors`, `set_state_theme_color`, and `rename_theme_color`.

### ⚠️ Blueprint Global Tasks — Use Name/Path, Not Wrapper Type

`add_global_task` supports Blueprint tasks by name, asset path, or `_C` generated class — the same
forms as `add_evaluator`. Do NOT pass `"StateTreeBlueprintTaskWrapper"` directly; that's an internal
struct name and won't resolve to the correct Blueprint class.

```python
# WRONG — passes the raw wrapper type; the specific Blueprint class won't be set
unreal.StateTreeService.add_global_task(st_path, "StateTreeBlueprintTaskWrapper")

# CORRECT — any of these forms work
unreal.StateTreeService.add_global_task(st_path, "STT_PatrolManagement")
unreal.StateTreeService.add_global_task(st_path, "STT_PatrolManagement_C")
unreal.StateTreeService.add_global_task(st_path, "/Game/StateTree/Tasks/STT_PatrolManagement")
```

When inspecting or binding, use the task's display name as it appears in `get_state_tree_info().global_tasks[N].name`
(e.g. `"STT PatrolManagement"` — no underscore, no `_C`).

### ⚠️ Blueprint Task Struct Name in get_task_property_names / bind_task_property_to_context

Blueprint tasks are stored internally as `StateTreeBlueprintTaskWrapper`. When calling
`get_task_property_names`, `bind_task_property_to_context`, `set_task_property_value`, or any
other task API, you can use **any** of these names — they all resolve to the same node:

- `"STT_Rotate_C"` — the Blueprint generated class name
- `"STT_Rotate"` — the Blueprint name without `_C` suffix
- `"STT Rotate"` — the display name shown in the editor
- `"StateTreeBlueprintTaskWrapper"` — the raw struct type

`get_state_tree_info()` shows tasks as `"STT Rotate (StateTreeBlueprintTaskWrapper)"` — use
either the name part or the struct_type part.

### ⚠️ Binding Fails When Context Actor Class Is Not Set

`bind_task_property_to_context` returns `False` when the StateTree has no context actor class.
The error message will say "Context 'Actor' not found — this StateTree has NO context actor class set."

**Diagnosis:** Call `get_state_tree_info()` and check `context_actor_class`. If it's empty, no
context bindings can work.

**Fix:** Call `set_context_actor_class()` with the appropriate Blueprint actor path BEFORE binding.

```python
# WRONG — binding with no context actor class set (will always fail)
unreal.StateTreeService.bind_task_property_to_context(st_path, "Root", "STT_Rotate_C", "Cube", "Actor", "")

# CORRECT — set context first, then bind
info = unreal.StateTreeService.get_state_tree_info(st_path)
if not info.context_actor_class:
    unreal.StateTreeService.set_context_actor_class(st_path, "/Game/Blueprints/BP_Cube")
unreal.StateTreeService.bind_task_property_to_context(st_path, "Root", "STT_Rotate_C", "Cube", "Actor", "")
```

### ⚠️ Forgetting to Compile

Every structural change (add state, add task, add transition) requires recompilation.
Always call `compile_state_tree()` before `save_state_tree()`.

```python
# WRONG — changes not compiled
unreal.StateTreeService.add_state(path, "Root", "MyState")
unreal.StateTreeService.save_state_tree(path)  # saves uncompiled tree

# CORRECT
unreal.StateTreeService.add_state(path, "Root", "MyState")
result = unreal.StateTreeService.compile_state_tree(path)
if result.success:
    unreal.StateTreeService.save_state_tree(path)
```

### ⚠️ Root State Must Be Created First

You cannot add child states before creating the root subtree.

```python
# WRONG — Root doesn't exist yet
unreal.StateTreeService.add_state(path, "Root", "Idle")

# CORRECT
unreal.StateTreeService.add_state(path, "", "Root")   # create Root first
unreal.StateTreeService.add_state(path, "Root", "Idle")
```

### ⚠️ Empty parentPath Creates a New Subtree

Passing an empty `parent_path` always creates a **new top-level subtree**, not a child of Root.

```python
# Creates a second top-level subtree named "Idle" (NOT under Root)
unreal.StateTreeService.add_state(path, "", "Idle")   # ← wrong

# Add Idle under Root
unreal.StateTreeService.add_state(path, "Root", "Idle")  # ← correct
```

### ⚠️ GotoState Requires targetPath

Transition type "GotoState" requires a valid `target_path`. Other types do not.

```python
# WRONG — missing target for GotoState
unreal.StateTreeService.add_transition(path, "Root/Idle", "OnStateCompleted", "GotoState")

# CORRECT
unreal.StateTreeService.add_transition(path, "Root/Idle", "OnStateCompleted", "GotoState", "Root/Walking")
```

### ⚠️ NEVER Use remove_state + add_state to "Move" a State

**This is destructive and can silently drop the original state's identity and editor data.**

When asked to move a StateTree state under a different parent, always use `move_state`.
`move_state` reparents the existing `UStateTreeState` in-place, preserving its children, tasks,
transitions, bindings, and per-state metadata.

`remove_state` followed by `add_state` creates a different state object. Any data attached to the
original state can be lost or detached from the new copy.

```python
# WRONG — destroys the original state object and recreates a lookalike
unreal.StateTreeService.remove_state(path, "Root/Idle")
unreal.StateTreeService.add_state(path, "Root/Peaceful", "Idle")

# CORRECT — reparent the existing state in-place
unreal.StateTreeService.move_state(path, "Root/Idle", "Root/Peaceful")
```

If `move_state` fails, stop and inspect the tree state. Do NOT fall back to remove+add as a workaround.

### ⚠️ NEVER Use remove_transition + add_transition to "Update" a Transition

**This is destructive and silently deletes all conditions on the transition.**

When asked to change a transition's target, trigger, or type, always use `update_transition`.
`update_transition` edits the existing `FStateTreeTransition` in-place — all conditions are preserved.

`remove_transition` followed by `add_transition` creates a fresh transition with **no conditions**.
Any `StateTreeObjectIsValidCondition` or other conditions that were wired up will be permanently lost.

```python
# WRONG — destroys all conditions on the transition
unreal.StateTreeService.remove_transition(path, "Root", 0)
unreal.StateTreeService.add_transition(path, "Root", "OnTick", "GotoState", "Root/Chasing")

# CORRECT — update in-place, conditions are preserved
unreal.StateTreeService.update_transition(path, "Root", 0, target_path="Root/Chasing")
```

**If `update_transition` returns `True` but the change doesn't appear in get_state_tree_info:**
- Compile and check again — the in-memory state is updated before compile
- Do NOT fall back to remove+add as a workaround

```python
# Verify the update was applied AFTER compiling
result = unreal.StateTreeService.update_transition(path, "Root", 0, target_path="Root/Chasing")
print(f"Update result: {result}")  # True = written to editor data

compile = unreal.StateTreeService.compile_state_tree(path)
print(f"Compile: {compile.success}")

info = unreal.StateTreeService.get_state_tree_info(path)
for s in info.all_states:
    if s.path == "Root":
        for t in s.transitions:
            print(f"  Trans: Trigger={t.trigger}, Target={t.target_state_name}, Conditions={[(c.name, c.struct_type) for c in t.conditions]}")
```

### ⚠️ add_transition Rejects Duplicates — Do NOT Retry Blindly

`add_transition` returns `False` if an identical transition (same trigger, type, and target) already
exists on the state. If compilation fails after adding a transition, do **NOT** call `add_transition`
again with different parameters — the first transition is still in memory. Instead:

1. **Remove** the failed transition with `remove_transition` first
2. **Then** add the corrected one

```python
# WRONG — retrying add_transition without removing the previous attempt
unreal.StateTreeService.add_transition(path, "Root", "OnTick", "NextSelectableState")  # compile fails
unreal.StateTreeService.add_transition(path, "Root", "OnTick", "GotoState", "Root/Idle")  # now 2 transitions!

# CORRECT — remove the failed one first, then add the corrected version
unreal.StateTreeService.add_transition(path, "Root", "OnTick", "NextSelectableState")  # compile fails
unreal.StateTreeService.remove_transition(path, "Root", 0)  # clean up
unreal.StateTreeService.add_transition(path, "Root", "OnTick", "GotoState", "Root/Idle")  # now only 1
```

### ⚠️ Use `StateTreeRef` Not `StateTree` on StateTreeComponent

`StateTreeComponent` has two related properties. The editor Details panel reads `StateTreeRef`.
Setting `StateTree` silently succeeds but the value does not appear in the editor.

```python
# WRONG — Details panel still shows None
unreal.BlueprintService.set_component_property(bp, "StateTree", "StateTree", st_path)

# CORRECT
unreal.BlueprintService.set_component_property(bp, "StateTree", "StateTreeRef", st_path)
```

### ⚠️ Task Struct Names Include "F" Prefix

```python
# WRONG
unreal.StateTreeService.add_task(path, "Root/Idle", "StateTreeDelayTask")

# CORRECT
unreal.StateTreeService.add_task(path, "Root/Idle", "FStateTreeDelayTask")
```

### ⚠️ Never Guess Task Property Names or Value Formats

`set_task_property_value` silently returns `False` when the property name or value format is wrong. Prefer the detailed result API and always:

1. Read `task.struct_type` from `get_state_tree_info()` to get the exact struct name
2. Inspect the struct's actual properties via `get_task_property_names()` before calling a setter
3. If duplicate task structs exist on the same state, pass `task_match_index` explicitly
4. Check the result object or read the value back before compiling

```python
# WRONG — guessing property names and ignoring the bool result
unreal.StateTreeService.set_task_property_value(path, "Root", "FStateTreeDebugTextTask", "Color", "(R=1.0,...)")
unreal.StateTreeService.compile_state_tree(path)  # compiles even if nothing changed

# CORRECT — inspect first, then use the detailed result API
props = unreal.StateTreeService.get_task_property_names(path, "Root", "FStateTreeDebugTextTask")
for p in props:
    print(f"{p.name}: {p.type} = {p.current_value}")  # exact names + correct value format

result = unreal.StateTreeService.set_task_property_value_detailed(
    path, "Root", "FStateTreeDebugTextTask", "BindableText", "Hello from Root")
assert result.success, result.error_message
```

### ⚠️ Condition Properties That Require Bindings (e.g. "Object")

Conditions like `StateTreeObjectIsValidCondition` have properties that **must be bound**
to context data or event payload — setting a string value won't work. Use `bind_transition_condition_property_to_context`,
`bind_enter_condition_property_to_context`, or `bind_transition_condition_property_to_event_payload` instead of `set_*_condition_property_value`.

```python
# WRONG — trying to set "Object" as a string value (will fail or compile error)
unreal.StateTreeService.set_transition_condition_property_value(
    path, "Root", 0, "StateTreeObjectIsValidCondition", "Object", "/Game/SomeActor")

# CORRECT — bind it to the context actor's property
unreal.StateTreeService.bind_transition_condition_property_to_context(
    path, "Root", 0, "StateTreeObjectIsValidCondition", "Object", "Actor", "TargetPawn")

# CORRECT — bind it to the transition's event payload property
# (when the transition has a RequiredEvent with a PayloadStruct like FStartChasingPayload)
# First inspect the payload fields instead of guessing the path.
payload_props = unreal.StateTreeService.get_transition_event_payload_property_names(path, "Root", 0)
for p in payload_props:
    print(f"{p.name}: {p.type} = {p.current_value!r}")

# The bind helper accepts friendly field names and resolves them to the reflected path.
unreal.StateTreeService.bind_transition_condition_property_to_event_payload(
    path, "Root", 0, "StateTreeObjectIsValidCondition", "Object", "TargetPawn")
```

### ⚠️ Bool Properties Drop the `b` Prefix in Python

UE Python bindings strip the `b` prefix from bool UPROPERTY names and convert to snake_case.

```python
# C++ field:     bSuccess    → Python: result.success
# C++ field:     bEnabled    → Python: state_info.enabled
# C++ field:     bIsCompiled → Python: info.is_compiled

result = unreal.StateTreeService.compile_state_tree(path)
print(result.success)   # NOT result.b_success or result.bSuccess
```

### ⚠️ Use `ReceiveLatentTick`, Not `ReceiveTick` on StateTree Tasks

`ReceiveTick` is **deprecated** on `StateTreeTaskBlueprintBase`. Using it causes compile errors:
> `Cannot override 'StateTreeTaskBlueprintBase::ReceiveTick' — declared with a different signature`

```python
bp_path = "/Game/StateTree/STT_MyTask"

# WRONG — deprecated, will fail to compile
unreal.BlueprintService.add_event_node(bp_path, "EventGraph", "ReceiveTick", 0, 0)

# CORRECT — new Tick event without return value
unreal.BlueprintService.add_event_node(bp_path, "EventGraph", "ReceiveLatentTick", 0, 0)

# WRONG — deprecated Enter State
unreal.BlueprintService.add_event_node(bp_path, "EventGraph", "ReceiveEnterState", 0, 0)

# CORRECT — new Enter State event without return value
unreal.BlueprintService.add_event_node(bp_path, "EventGraph", "ReceiveLatentEnterState", 0, 0)
```

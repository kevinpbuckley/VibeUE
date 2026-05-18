---
name: api-bindings
description: StateTreeService API for evaluators, global tasks, transitions, OnDelegate workflow, compile/save, context actor class, property bindings, StateTreeComponent assignment, theme colors, expand/collapse
---

This sub-doc continues from skill.md → "API Reference". For asset creation, state management, and per-state tasks see `api-reference.md`.

### Evaluators & Global Tasks

**Evaluators** run every tick and feed computed data to all states (read-only output).
**Global Tasks** run for the entire lifetime of the StateTree (full task lifecycle: EnterState, Tick, ExitState).

Both support C++ structs and Blueprint assets.

#### Evaluators

```python
# Find available evaluator types (includes both struct and Blueprint evaluator types)
types = unreal.StateTreeService.get_available_evaluator_types()

# Add global evaluator by struct name (runs every tick, data available to all states)
unreal.StateTreeService.add_evaluator("/Game/AI/MyBehavior", "FMyCustomEvaluator")

# Add Blueprint evaluator by name, path, or generated class name (all three work)
unreal.StateTreeService.add_evaluator("/Game/AI/MyBehavior", "STE_PatrolPointManagement")
unreal.StateTreeService.add_evaluator("/Game/AI/MyBehavior", "/Game/StateTree/Evaluators/STE_PatrolPointManagement")
unreal.StateTreeService.add_evaluator("/Game/AI/MyBehavior", "STE_PatrolPointManagement_C")
```

#### Global Tasks

```python
# Find available task types (use this to get the exact registered name for Blueprint tasks)
types = unreal.StateTreeService.get_available_task_types()
for t in types:
    print(t)  # e.g. "STT_PatrolManagement_C"

# Add a C++ struct global task
unreal.StateTreeService.add_global_task("/Game/AI/MyBehavior", "FStateTreeDelayTask")

# Add a Blueprint global task — supports the same name forms as add_evaluator:
# name, full asset path, or _C generated class name
unreal.StateTreeService.add_global_task("/Game/AI/MyBehavior", "STT_PatrolManagement")
unreal.StateTreeService.add_global_task("/Game/AI/MyBehavior", "/Game/StateTree/Tasks/STT_PatrolManagement")
unreal.StateTreeService.add_global_task("/Game/AI/MyBehavior", "STT_PatrolManagement_C")
```

#### ⚠️ Always Check Before Adding — Global Tasks Accumulate

```python
import unreal

st_path = "/Game/AI/MyBehavior"

# Check existing global tasks before adding
info = unreal.StateTreeService.get_state_tree_info(st_path)
existing_global = [t.name for t in info.global_tasks]
print(f"Existing global tasks: {existing_global}")

if "STT PatrolManagement" not in existing_global:
    result = unreal.StateTreeService.add_global_task(st_path, "STT_PatrolManagement")
    print(f"Added global task: {result}")
else:
    print("Global task already present, skipping")
```

#### Binding Global Task Properties

Global tasks support the same property binding patterns as per-state tasks. Use `bind_global_task_property_to_root_parameter` or `bind_global_task_property_to_context`.

```python
import unreal

st_path = "/Game/StateTree/ST_Cube"

# Inspect what properties the global task exposes
props = unreal.StateTreeService.get_global_task_property_names(st_path, "STT_PatrolManagement")
for p in props:
    print(f"  {p.name}: {p.type} = {p.current_value!r}")

# Set a property value directly
unreal.StateTreeService.set_global_task_property_value(st_path, "STT_PatrolManagement", "PatrolTag", "Patrol1")

# Bind a global task property to a root parameter
unreal.StateTreeService.bind_global_task_property_to_root_parameter(
    st_path,
    "STT_PatrolManagement",   # task name (Blueprint name, _C class, or display name)
    "PatrolTag",              # task property to bind
    "PatrolTag"               # root parameter name
)

# Bind a global task property to the context Actor
unreal.StateTreeService.bind_global_task_property_to_context(
    st_path,
    "STT_PatrolManagement",
    "ActorRef",               # task property to bind
    "Actor",                  # context name
    ""                        # context property path (empty = whole object)
)

# Bind a state task property to a property produced by a global task
unreal.StateTreeService.bind_task_property_to_global_task_property(
    st_path,
    "Peaceful/Patrol",        # state path containing the task to update
    "STT_MoveToPatrolPoint",  # state task name
    "PatrolPointManager",     # target property on the state task
    "STT_PatrolManagement",   # source global task name
    "PatrolPointManager"      # source property on the global task
)
```

#### Full Add + Bind Workflow

```python
import unreal

st_path = "/Game/StateTree/ST_Cube"

# 1. Check if already present
info = unreal.StateTreeService.get_state_tree_info(st_path)
existing = [t.name for t in info.global_tasks]

if "STT PatrolManagement" not in existing:
    ok = unreal.StateTreeService.add_global_task(st_path, "STT_PatrolManagement")
    print(f"add_global_task: {ok}")
    assert ok, "add_global_task returned False"

# 2. Inspect properties
props = unreal.StateTreeService.get_global_task_property_names(st_path, "STT_PatrolManagement")
for p in props:
    print(f"  {p.name}: {p.type} = {p.current_value!r}")

# 3. Bind to root parameter (if PatrolTag root param exists)
unreal.StateTreeService.bind_global_task_property_to_root_parameter(
    st_path, "STT_PatrolManagement", "PatrolTag", "PatrolTag")

# 4. Compile and save
result = unreal.StateTreeService.compile_state_tree(st_path)
assert result.success, result.errors
unreal.StateTreeService.save_state_tree(st_path)
print("Done")
```

### Transitions

```python
# Triggers:
#   OnStateCompleted   — after tasks complete (success or failure)
#   OnStateSucceeded   — only on task success
#   OnStateFailed      — only on task failure
#   OnTick             — every tick (use with conditions)
#   OnEvent            — on gameplay event
#   OnDelegate         — when a task's FStateTreeDelegateDispatcher fires
#                        (requires bind_transition_to_delegate after setting trigger)

# Transition types:
#   GotoState          — go to a specific state (requires target_path)
#   Succeeded          — complete this state as succeeded
#   Failed             — complete this state as failed
#   NextState          — go to the next sibling state
#   NextSelectableState — go to the next eligible sibling

# Priorities: Low, Normal (default), Medium, High, Critical

# GotoState example
unreal.StateTreeService.add_transition(
    "/Game/AI/MyBehavior", "Root/Idle",
    "OnStateCompleted", "GotoState", "Root/Walking", "Normal")

# Complete state on failure
unreal.StateTreeService.add_transition(
    "/Game/AI/MyBehavior", "Root/Walking",
    "OnStateFailed", "Failed")

# Loop back to same state
unreal.StateTreeService.add_transition(
    "/Game/AI/MyBehavior", "Root/Attacking",
    "OnStateSucceeded", "GotoState", "Root/Attacking")
```

### OnDelegate Transitions — Full Workflow

`OnDelegate` transitions fire when a task's `FStateTreeDelegateDispatcher` property broadcasts.
This requires **three steps**: add the dispatcher variable, set the trigger, bind the transition.

#### ⚠️ Expected Compile Error — Do NOT Revert

After calling `update_transition(trigger="OnDelegate")`, compiling will produce:

```
"<StateName> On Delegate Transition to '<TargetState>' requires to be bound to some delegate dispatcher."
```

**This is expected.** The binding step (`bind_transition_to_delegate`) hasn't been done yet.
Do NOT revert the trigger back to `OnStateCompleted` — continue with the workflow below.

#### Step-by-Step

```python
import unreal

bp_path = "/Game/StateTree/Tasks/STT_Rotate"
st_path = "/Game/StateTree/ST_Cube"
state_path = "Root/Rotating"
transition_index = 0  # from get_state_tree_info

# Step 1: Add a FStateTreeDelegateDispatcher variable to the Blueprint task
if not unreal.BlueprintService.variable_exists(bp_path, "FinishRotatingDispatcher"):
    result = unreal.BlueprintService.add_variable(bp_path, "FinishRotatingDispatcher", "FStateTreeDelegateDispatcher")
    assert result, "Failed to add FinishRotatingDispatcher variable"
    unreal.BlueprintService.compile_blueprint(bp_path)
    unreal.EditorAssetLibrary.save_asset(bp_path)

# Step 2: Set the transition trigger to OnDelegate
result = unreal.StateTreeService.update_transition(st_path, state_path, transition_index, trigger="OnDelegate")
assert result, "update_transition failed"

# Step 3: Bind the transition to the dispatcher property
result = unreal.StateTreeService.bind_transition_to_delegate(
    st_path, state_path, transition_index,
    "STT_Rotate",                  # task name (display name, Blueprint name, or struct type)
    "FinishRotatingDispatcher"     # the FStateTreeDelegateDispatcher variable name
)
assert result, "bind_transition_to_delegate failed"

# Step 4: Compile — should now succeed with no delegate errors
compile_result = unreal.StateTreeService.compile_state_tree(st_path)
assert compile_result.success, compile_result.errors
unreal.StateTreeService.save_state_tree(st_path)
```

#### Firing the Dispatcher from the Blueprint Task

In `STT_Rotate`'s Blueprint graph, call the dispatcher to trigger the transition:

```python
# The dispatcher is called like a function in Blueprint — add a "Call FinishRotatingDispatcher" node
# connected to whatever execution flow should end the state (e.g. after a timer, animation, etc.)
```

#### Notes

- `FStateTreeDelegateDispatcher` is a USTRUCT — use type string `"FStateTreeDelegateDispatcher"` with `add_variable`.
- The dispatcher variable must be on the task that is **in the same state** as the `OnDelegate` transition.
- After `bind_transition_to_delegate`, the compile error about the missing binding will resolve.

### Compile & Save

```python
# Always compile after structural changes
result = unreal.StateTreeService.compile_state_tree("/Game/AI/MyBehavior")
# result.success   → bool
# result.errors      → list of strings
# result.warnings    → list of strings

# Save to disk
unreal.StateTreeService.save_state_tree("/Game/AI/MyBehavior")
```

### Setting the Context Actor Class

```python
# Pass the Blueprint ASSET path (no _C suffix) — StateTreeService resolves the generated class.
unreal.StateTreeService.set_context_actor_class("/Game/AI/ST_MyBehavior", "/Game/Blueprints/BP_MyActor")
unreal.StateTreeService.compile_state_tree("/Game/AI/ST_MyBehavior")
unreal.StateTreeService.save_state_tree("/Game/AI/ST_MyBehavior")
```

### Property Bindings (Binding Task Properties to Context or Parameters)

Bindings connect a task's property to a **context object** (e.g. the Actor running the StateTree) or
a **root parameter**. This is how tasks access external data at runtime.

#### ⚠️ CRITICAL: Context Must Be Set Before Binding

`bind_task_property_to_context` will **fail silently** if the StateTree has no context actor class.
Check `get_state_tree_info().context_actor_class` first — if it's empty, call `set_context_actor_class`
before attempting any context binding.

#### Full Binding Workflow

```python
import unreal

st_path = "/Game/StateTree/ST_Cube"
state_path = "Root"
# For Blueprint tasks, use the Blueprint class name (STT_Rotate_C or STT_Rotate)
# OR "StateTreeBlueprintTaskWrapper" — both work.
task_struct = "STT_Rotate_C"

# Step 1: Check if context actor class is set
info = unreal.StateTreeService.get_state_tree_info(st_path)
print(f"Context Actor Class: {info.context_actor_class}")

# Step 2: If empty, SET IT FIRST — this is the #1 reason bindings fail
if not info.context_actor_class:
    unreal.StateTreeService.set_context_actor_class(st_path, "/Game/Blueprints/BP_Cube")
    print("Set context actor class")

# Step 3: Discover bindable properties on the task
props = unreal.StateTreeService.get_task_property_names(st_path, state_path, task_struct)
for p in props:
    print(f"  {p.name}: {p.type} = {p.current_value!r}")

# Step 4: Bind task property to the context Actor (whole object)
#   - context_name="Actor" matches the first context descriptor (default)
#   - context_property_path="" means bind the entire actor reference
result = unreal.StateTreeService.bind_task_property_to_context(
    st_path, state_path, task_struct,
    "Cube",           # task property to bind
    "Actor",          # context name
    ""                # context property path (empty = whole object)
)
print(f"Bind result: {result}")

# Step 5: Compile and save
compile_result = unreal.StateTreeService.compile_state_tree(st_path)
assert compile_result.success, compile_result.errors
unreal.StateTreeService.save_state_tree(st_path)
```

#### Binding to a Root Parameter

```python
# Bind a task property to a root parameter (no context actor class needed)
unreal.StateTreeService.bind_task_property_to_root_parameter(
    st_path, state_path, task_struct,
    "Duration",        # task property
    "IdlingTime"       # root parameter name
)

# Bind a task property to a property exposed by a global task
unreal.StateTreeService.bind_task_property_to_global_task_property(
    st_path, state_path, task_struct,
    "PatrolPointManager",     # task property
    "STT_PatrolManagement",   # global task name
    "PatrolPointManager"      # global task property
)
```

#### Binding Evaluator Properties

Evaluators are global (not tied to a state), so there is no `state_path` parameter.

```python
# Bind an evaluator property to a root parameter
unreal.StateTreeService.bind_evaluator_property_to_root_parameter(
    st_path,
    "STE_PatrolPointManagement",  # evaluator struct name (or Blueprint wrapper name)
    "PatrolTag",                  # evaluator property to bind
    "PatrolTag"                   # root parameter name
)

# Bind an evaluator property to context data
unreal.StateTreeService.bind_evaluator_property_to_context(
    st_path,
    "STE_PatrolPointManagement",  # evaluator struct name
    "ActorRef",                   # evaluator property to bind
    "Actor",                      # context name
    ""                            # context property path (empty = whole object)
)

# Unbind an evaluator property
unreal.StateTreeService.unbind_evaluator_property(
    st_path,
    "STE_PatrolPointManagement",
    "PatrolTag"
)
```

### Assigning a StateTree to a StateTreeComponent on a Blueprint

`StateTreeComponent` has **two** properties that look related — only `StateTreeRef` is shown in the
editor Details panel. Always set `StateTreeRef`, never `StateTree`.

```python
# WRONG — sets the internal TObjectPtr; the Details panel still shows None
unreal.BlueprintService.set_component_property(bp_path, "StateTree", "StateTree", st_path)

# CORRECT — sets the FStateTreeReference struct that the editor reads
unreal.BlueprintService.set_component_property(bp_path, "StateTree", "StateTreeRef", st_path)
unreal.BlueprintService.compile_blueprint(bp_path)
unreal.EditorAssetLibrary.save_asset(bp_path)
```

### Theme Colors (Global Color Table)

StateTree assets have a **global color table** — named color entries that can be assigned to states
for visual organization in the editor. These are NOT material colors or rendering colors.

When a user says "color", "rename color", "change color", or "theme color" in a StateTree context,
they mean **StateTree theme colors** (editor-only visual labels), not material parameters.

**List all theme colors:**
```python
colors = unreal.StateTreeService.get_theme_colors("/Game/AI/ST_MyBehavior")
for c in colors:
    print(f"{c.display_name}: R={c.color.r}, G={c.color.g}, B={c.color.b} — used by: {[s for s in c.used_by_states]}")
```

**Set a state's theme color (creates the color entry if it doesn't exist):**
```python
color = unreal.LinearColor(r=0.2, g=0.6, b=1.0, a=1.0)
unreal.StateTreeService.set_state_theme_color("/Game/AI/ST_MyBehavior", "Root/Idle", "Idle", color)
```

**Rename a theme color entry (preserves all state references):**
```python
unreal.StateTreeService.rename_theme_color("/Game/AI/ST_MyBehavior", "Default Color", "Active")
```

**Workflow:** Always call `get_theme_colors` first to see what exists before renaming or modifying.

### Expand / Collapse States

Control whether states are expanded or collapsed in the editor tree view:

```python
# Collapse a state in the editor
unreal.StateTreeService.set_state_expanded("/Game/AI/ST_MyBehavior", "Root/Walking", False)

# Expand a state
unreal.StateTreeService.set_state_expanded("/Game/AI/ST_MyBehavior", "Root/Walking", True)
```

The current expand/collapse state is also returned in `get_state_tree_info` results via `b_expanded`.

### Advanced Editor Config (Use service first)

Use `unreal.StateTreeService` for StateTree asset edits first. The service layer now covers:

- List, set, and rename theme colors (`get_theme_colors`, `set_state_theme_color`, `rename_theme_color`)
- Expand/collapse states in the editor tree view (`set_state_expanded`)
- Configure state descriptions
- Add/edit StateTree parameters and default values
- Bind task properties (e.g. debug text bindable text, delay duration bindings)
- Set the context actor class

Reserve `execute_python_code` for Blueprint or level-instance operations outside the StateTree asset itself, such as:

- Adding Blueprint variables or components
- Making Blueprint properties instance-editable
- Overriding StateTree component data on placed actors

Recommended pattern:

1. Use `unreal.StateTreeService` methods for structure, descriptions, colors, parameters, property edits, bindings, compile, and save.
2. Use `execute_python_code` only for Blueprint or level-instance work that sits outside the StateTree asset.

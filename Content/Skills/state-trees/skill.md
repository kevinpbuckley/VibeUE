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

### STT Completion Contract

For `STT_*` graph edits, do not report success until the output explicitly shows:

1. `Event EnterState` exists.
2. `Set Timer by Event` exists.
3. The callback event node exists in a fresh node listing and is the requested node type.
4. The expected execution and delegate connections exist.
5. `Finish Task.bSucceeded` is set correctly.
6. Compile succeeds.

If compile succeeds but any of the checks above fail, the graph is still wrong.

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
```

## API Reference

### Discovery

```python
# List all StateTrees under a directory
paths = unreal.StateTreeService.list_state_trees("/Game")          # → ["/Game/AI/MyBehavior", ...]
paths = unreal.StateTreeService.list_state_trees("/Game/AI")       # → narrowed search

# Get full structural info
info = unreal.StateTreeService.get_state_tree_info("/Game/AI/MyBehavior")
# info.asset_name, info.schema_class, info.context_actor_class, info.is_compiled
# info.context_actor_class → path of the context actor class (empty if not set!)
# info.evaluators       → list of FStateTreeNodeInfo
# info.global_tasks     → list of FStateTreeNodeInfo
# info.all_states       → list of FStateTreeStateInfo (flattened)

# Each FStateTreeStateInfo has:
#   .name, .path, .state_type, .selection_behavior, .enabled
#   .theme_color (display name of assigned color, empty if none)
#   .b_expanded (whether state is expanded in editor tree view)
#   .tasks, .enter_conditions, .transitions, .child_paths
```

### Asset Creation

```python
# Create a new StateTree
unreal.StateTreeService.create_state_tree("/Game/AI/MyBehavior")
```

### State Management

```python
# Add a top-level subtree (equivalent to Root)
unreal.StateTreeService.add_state("/Game/AI/MyBehavior", "", "Root")

# Add child states
unreal.StateTreeService.add_state("/Game/AI/MyBehavior", "Root", "Idle")
unreal.StateTreeService.add_state("/Game/AI/MyBehavior", "Root", "Idle", "State")  # explicit type

# State types: "State" (default), "Group", "Subtree", "Linked", "LinkedAsset"
unreal.StateTreeService.add_state("/Game/AI/MyBehavior", "Root", "BehaviorGroup", "Group")

# Remove a state (also removes children)
unreal.StateTreeService.remove_state("/Game/AI/MyBehavior", "Root/Walking")

# Enable/disable
unreal.StateTreeService.set_state_enabled("/Game/AI/MyBehavior", "Root/Idle", True)

# Theme colors — list, set, rename (see Theme Colors section below for details)
colors = unreal.StateTreeService.get_theme_colors("/Game/AI/MyBehavior")
unreal.StateTreeService.set_state_theme_color("/Game/AI/MyBehavior", "Root/Idle", "Idle", unreal.LinearColor(r=0.2, g=0.6, b=1.0, a=1.0))
unreal.StateTreeService.rename_theme_color("/Game/AI/MyBehavior", "Default Color", "Active")

# Expand/collapse states in editor tree view
unreal.StateTreeService.set_state_expanded("/Game/AI/MyBehavior", "Root/Walking", False)  # collapse
unreal.StateTreeService.set_state_expanded("/Game/AI/MyBehavior", "Root/Walking", True)   # expand
```

### Tasks

```python
# Find available task types
types = unreal.StateTreeService.get_available_task_types()

# Add a task to a state
unreal.StateTreeService.add_task("/Game/AI/MyBehavior", "Root/Idle", "FStateTreeDelayTask")
unreal.StateTreeService.add_task("/Game/AI/MyBehavior", "Root", "FStateTreeRunSubtreeTask")
```

#### ⚠️ Always check before adding — tasks accumulate and don't auto-deduplicate

```python
import unreal

st_path = "/Game/AI/MyBehavior"
state_path = "Root/Idle"
task_struct = "FStateTreeDelayTask"

# WRONG — adds a duplicate if task already exists
unreal.StateTreeService.add_task(st_path, state_path, task_struct)

# CORRECT — check first
info = unreal.StateTreeService.get_state_tree_info(st_path)
for state in info.all_states:
    if state.path == state_path:
        existing = [t.struct_type for t in state.tasks]
        print(f"Existing tasks: {existing}")
        if "StateTreeDelayTask" not in existing:
            unreal.StateTreeService.add_task(st_path, state_path, task_struct)
            print("ADDED task")
        else:
            print("Task already exists, skipping add")
```

#### `StateTreeDebugTextTask` in UE5.7

In UE5.7, `FStateTreeDebugTextTask` exposes editable properties across both the task node struct and the task instance data. Common properties include:
- `Text` (FString)
- `TextColor` (FColor)
- `FontScale` (float)
- `Offset` (FVector) and dotted child paths like `Offset.Z`
- `bEnabled` (bool)
- `BindableText` (FString)
- `ReferenceActor` (TObjectPtr<AActor>)

Always call `get_task_property_names` first and use the exact returned property names. Do not guess aliases like `Color` when the real property name is `TextColor`.

```python
import unreal

st_path = "/Game/AI/MyBehavior"
props = unreal.StateTreeService.get_task_property_names(st_path, "Root", "FStateTreeDebugTextTask")
for p in props:
    print(f"  {p.name}: {p.type} = {p.current_value!r}")
# Example UE5.7 output:
#   Text: FString = ""
#   TextColor: FColor = (B=255,G=255,R=255,A=255)
#   FontScale: float = 1.000000
#   Offset: FVector = (X=0.000000,Y=0.000000,Z=0.000000)
#   Offset.Z: double = 0.000000
#   bEnabled: bool = True
#   ReferenceActor: TObjectPtr<AActor> = None
#   BindableText: FString = ""

# Set the display text and text color
result = unreal.StateTreeService.set_task_property_value_detailed(
    st_path, "Root", "FStateTreeDebugTextTask", "Text", "Hello from Root")
assert result.success, result.error_message

result = unreal.StateTreeService.set_task_property_value_detailed(
    st_path, "Root", "FStateTreeDebugTextTask", "TextColor", "(R=255,G=105,B=180,A=255)")
assert result.success, result.error_message
result = unreal.StateTreeService.compile_state_tree(st_path)
assert result.success
unreal.StateTreeService.save_state_tree(st_path)
```

### Setting Task Properties — Deterministic Pattern

Use the service first. Do not guess property names, and do not target duplicate tasks implicitly.

```python
import unreal

st_path = "/Game/AI/MyBehavior"
state_path = "Root"

# Step 1: Inspect the exact tasks on the state and count duplicate struct matches.
info = unreal.StateTreeService.get_state_tree_info(st_path)
matching_tasks = []
for state in info.all_states:
    if state.path == state_path:
        running_index_by_struct = {}
        for task in state.tasks:
            struct_type = task.struct_type
            match_index = running_index_by_struct.get(struct_type, 0)
            running_index_by_struct[struct_type] = match_index + 1
            print(f"Task match {match_index}: {task.name} ({struct_type})")
            if struct_type == "FStateTreeDebugTextTask":
                matching_tasks.append(match_index)

task_match_index = matching_tasks[-1] if matching_tasks else -1
assert task_match_index != -1, "Root has no FStateTreeDebugTextTask"

# Step 2: Discover valid property paths for that exact task match.
props = unreal.StateTreeService.get_task_property_names(
    st_path, state_path, "FStateTreeDebugTextTask", task_match_index)
for p in props:
    print(f"  {p.name}: {p.type} = {p.current_value!r}")
# Step 3: Set a property using the detailed result API.
set_result = unreal.StateTreeService.set_task_property_value_detailed(
    st_path, state_path, "FStateTreeDebugTextTask",
    "Text", "Hello from Root", task_match_index)

assert set_result.success, set_result.error_message
print(f"Previous value: {set_result.previous_value!r}")
print(f"New value: {set_result.new_value!r}")

# Step 4: Compile and save.
compile_result = unreal.StateTreeService.compile_state_tree(st_path)
assert compile_result.success, compile_result.errors
unreal.StateTreeService.save_state_tree(st_path)
print("Done")
```

For nested struct properties, use the exact dotted path returned by `get_task_property_names`
(for example `Offset.Z`) instead of inventing it.

### Evaluators & Global Tasks

```python
# Find available evaluator types
types = unreal.StateTreeService.get_available_evaluator_types()

# Add global evaluator (runs every tick, data available to all states)
unreal.StateTreeService.add_evaluator("/Game/AI/MyBehavior", "FMyCustomEvaluator")

# Add global task (runs while tree is active)
unreal.StateTreeService.add_global_task("/Game/AI/MyBehavior", "FStateTreeDelayTask")
```

### Transitions

```python
# Triggers:
#   OnStateCompleted   — after tasks complete (success or failure)
#   OnStateSucceeded   — only on task success
#   OnStateFailed      — only on task failure
#   OnTick             — every tick (use with conditions)
#   OnEvent            — on gameplay event

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

## COMMON_MISTAKES

### ⚠️ "Color" Means Theme Color, Not Materials

When a user asks to rename, change, or list "colors" on a StateTree, they mean **theme colors** —
the editor-only color labels in the StateTree's global color table. Do NOT load the materials skill
or look for material parameters. Use `get_theme_colors`, `set_state_theme_color`, and `rename_theme_color`.

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

## New Methods (Phase 1 & 2)

### State Properties

```python
# Set how children of a state are selected
unreal.StateTreeService.set_selection_behavior(path, "Root", "TrySelectChildrenInOrder")
# Options: "None", "TryEnterState", "TrySelectChildrenInOrder", "TrySelectChildrenAtRandom",
#          "TrySelectChildrenWithHighestUtility", "TrySelectChildrenAtRandomWeightedByUtility",
#          "TryFollowTransitions"

# Set task completion mode for a state
unreal.StateTreeService.set_tasks_completion(path, "Root/Idle", "Any")  # "Any" or "All"

# Rename a state
unreal.StateTreeService.rename_state(path, "Root/OldName", "NewName")

# Set a gameplay tag on a state (tag must exist in the project tag table)
unreal.StateTreeService.set_state_tag(path, "Root/Idle", "AI.State.Idle")
unreal.StateTreeService.set_state_tag(path, "Root/Idle", "")  # clear the tag

# Set utility weight (for Utility-based selection behaviors)
unreal.StateTreeService.set_state_weight(path, "Root/Idle", 2.5)

# Set whether a task's completion contributes to the owning state's completion
unreal.StateTreeService.set_task_considered_for_completion(path, "Root/Idle", "FStateTreeDelayTask", 0, True)
```

### Parameters (Root Property Bag)

```python
# Get all root parameters with name, type, and current default value
params = unreal.StateTreeService.get_root_parameters(path)
for p in params:
    print(f"{p.name}: {p.type} = {p.default_value!r}")

# Also returned in get_state_tree_info:
info = unreal.StateTreeService.get_state_tree_info(path)
for p in info.root_parameters:
    print(f"{p.name}: {p.type} = {p.default_value!r}")

# Add or update a root parameter of any primitive type
unreal.StateTreeService.add_or_update_root_parameter(path, "my_float", "Float", "3.14")
unreal.StateTreeService.add_or_update_root_parameter(path, "my_bool", "Bool", "true")
unreal.StateTreeService.add_or_update_root_parameter(path, "my_int", "Int32", "42")
unreal.StateTreeService.add_or_update_root_parameter(path, "my_string", "String", "Hello")
# Type options: "Bool", "Int32", "Int64", "Float", "Double", "Name", "String", "Text"

# Remove a root parameter by name
unreal.StateTreeService.remove_root_parameter(path, "my_float")

# Rename a root parameter (reads current value, removes old, adds under new name)
unreal.StateTreeService.rename_root_parameter(path, "old_name", "new_name")
```

### Per-Instance Parameter Overrides (Level Actors)

StateTree parameters defined on the asset are the *defaults*. Placed actors have their own
`StateTreeComponent` instance that can override each parameter value independently.

**⚠️ LOAD THIS SKILL FIRST** — Do not attempt raw discovery of StateTreeComponent parameters.
`set_component_parameter_override` handles type resolution automatically.

#### Full Discovery Workflow

When the user asks to set StateTree parameters on placed actors, follow this order:

```python
import unreal

# Step 1: Find the exact actor names in the level
actors = unreal.ActorService.list_level_actors()
for a in actors:
    print(f"{a.name}: {a.class_name}")
# Look for actors whose class_name contains your Blueprint (e.g. "BP_Cube_C")

# Step 2: Find the linked StateTree asset path for the actor
st_path = unreal.StateTreeService.get_component_state_tree_path("bp_cube1")
print(f"StateTree asset: {st_path}")
# Returns something like: /Game/StateTree/ST_Cube

# Step 3: Discover what parameters are available (names + types live in the asset)
params = unreal.StateTreeService.get_root_parameters(st_path)
for p in params:
    print(f"{p.name}: {p.type} = {p.default_value!r}")
# Output example: IdlingTime: Float = '2.0'   RotatingTime: Float = '1.0'

# Step 4: Set per-instance overrides — type resolved automatically from the linked StateTree
unreal.StateTreeService.set_component_parameter_override("bp_cube1", "IdlingTime", "3.0")
unreal.StateTreeService.set_component_parameter_override("bp_cube1", "RotatingTime", "1.5")
unreal.StateTreeService.set_component_parameter_override("bp_cube2", "IdlingTime", "1.0")
unreal.StateTreeService.set_component_parameter_override("bp_cube2", "RotatingTime", "4.0")

# Step 5: Save the level to persist overrides
unreal.EditorLoadingAndSavingUtils.save_current_level()
```

#### Read Back Current Instance Values

```python
# Get current override values on a placed actor's StateTreeComponent
overrides = unreal.StateTreeService.get_component_parameter_overrides("bp_cube1")
for p in overrides:
    print(f"{p.name}: {p.type} = {p.default_value!r}")
```

**Important notes:**
- The actor name must match the in-level instance label (e.g. `bp_cube1`), NOT the Blueprint
  class name. Use `ActorService.list_level_actors()` to discover exact names.
- Parameter names are defined in the StateTree asset, NOT as Blueprint variables. Always use
  `get_component_state_tree_path` + `get_root_parameters` to discover available names and types.
- Value format is identical to `add_or_update_root_parameter`: `"3.14"`, `"true"`, `"Hello"`.

### Transition Editing

```python
# Transitions are indexed 0-based within a state's transitions list.
# Use get_state_tree_info to find the index field on each FStateTreeTransitionInfo.

# Update an existing transition (pass empty string for fields you don't want to change)
unreal.StateTreeService.update_transition(
    path, "Root/Idle", 0,
    trigger="OnStateCompleted",       # empty string = no change
    transition_type="GotoState",
    target_path="Root/Walking",
    priority="Normal",
    b_set_enabled=True, b_enabled=True,
    b_set_delay=True, b_delay_transition=True, delay_duration=1.5, delay_random_variance=0.5
)

# Remove a transition by index
unreal.StateTreeService.remove_transition(path, "Root/Idle", 0)

# Reorder a transition (move from index 2 to index 0)
unreal.StateTreeService.move_transition(path, "Root/Idle", 2, 0)
```

### Task Management (Extended)

```python
# Remove a task from a state by struct type name
unreal.StateTreeService.remove_task(path, "Root/Idle", "FStateTreeDelayTask")
unreal.StateTreeService.remove_task(path, "Root/Idle", "FStateTreeDelayTask", 1)  # second match

# Move a task to a different index within the state's Tasks array
unreal.StateTreeService.move_task(path, "Root/Idle", "FStateTreeDelayTask", 0, 2)  # move from 0 to 2

# Enable or disable a task without removing it
unreal.StateTreeService.set_task_enabled(path, "Root/Idle", "FStateTreeDelayTask", 0, False)
```

### Conditions

```python
# Discover available condition struct names
cond_types = unreal.StateTreeService.get_available_condition_types()

# Add an enter condition to a state
unreal.StateTreeService.add_enter_condition(path, "Root/Idle", "FStateTreeCommonConditionBase")

# Remove an enter condition by index
unreal.StateTreeService.remove_enter_condition(path, "Root/Idle", 0)

# Set the And/Or operand on a condition (first must be "Copy")
unreal.StateTreeService.set_enter_condition_operand(path, "Root/Idle", 0, "Copy")
unreal.StateTreeService.set_enter_condition_operand(path, "Root/Idle", 1, "And")

# Inspect properties on an enter condition
props = unreal.StateTreeService.get_enter_condition_property_names(path, "Root/Idle", "FMyCondition")
for p in props:
    print(f"{p.name}: {p.type} = {p.current_value!r}")

# Set a property on an enter condition
unreal.StateTreeService.set_enter_condition_property_value(path, "Root/Idle", "FMyCondition", "Threshold", "5.0")

# Add a condition to a transition (by transition index)
unreal.StateTreeService.add_transition_condition(path, "Root/Idle", 0, "FMyCondition")

# Remove a condition from a transition
unreal.StateTreeService.remove_transition_condition(path, "Root/Idle", 0, 0)  # transition 0, condition 0

# Set operand on a transition condition
unreal.StateTreeService.set_transition_condition_operand(path, "Root/Idle", 0, 1, "Or")

# Inspect properties on a transition condition
props = unreal.StateTreeService.get_transition_condition_property_names(path, "Root/Idle", 0, "FMyCondition")

# Set a property on a transition condition
unreal.StateTreeService.set_transition_condition_property_value(
    path, "Root/Idle", 0, "FMyCondition", "Threshold", "3.0")
```

### Evaluator & Global Task Management (Extended)

```python
# Remove a global evaluator
unreal.StateTreeService.remove_evaluator(path, "FMyEvaluator")
unreal.StateTreeService.remove_evaluator(path, "FMyEvaluator", 1)  # second match

# Inspect properties on a global evaluator
props = unreal.StateTreeService.get_evaluator_property_names(path, "FMyEvaluator")
for p in props:
    print(f"{p.name}: {p.type} = {p.current_value!r}")

# Set a property on a global evaluator
unreal.StateTreeService.set_evaluator_property_value(path, "FMyEvaluator", "SomeProperty", "42.0")

# Remove a global task
unreal.StateTreeService.remove_global_task(path, "FMyGlobalTask")

# Inspect properties on a global task
props = unreal.StateTreeService.get_global_task_property_names(path, "FMyGlobalTask")

# Set a property on a global task
unreal.StateTreeService.set_global_task_property_value(path, "FMyGlobalTask", "SomeProperty", "Hello")
```

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

`FStateTreeNodeInfo` now includes:
- `b_considered_for_completion` (bool) — whether task contributes to state completion (tasks only)
- `operand` (str) — "Copy", "And", or "Or" (conditions only)

`FStateTreeTransitionInfo` now includes:
- `index` (int) — zero-based index within the state's transitions array
- `b_delay_transition` (bool)
- `delay_duration` (float)
- `delay_random_variance` (float)
- `required_event_tag` (str)
- `conditions` (list of `FStateTreeNodeInfo`)
- `condition_operands` (list of str)

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
---

# StateTree Skill

StateTree is Unreal Engine's hierarchical state machine system for AI behavior and game logic.
This skill covers creating and editing StateTree assets via `unreal.StateTreeService`.

## Key Concepts

| Concept | Description |
|---------|-------------|
| **StateTree Asset** | The `.uasset` file containing the tree definition |
| **Subtree / Root State** | Top-level state (usually named "Root") — created with empty `parent_path` |
| **State** | A node in the tree; can have tasks, enter conditions, transitions, and child states |
| **Task** | Logic that runs while a state is active (e.g. move, delay, play animation) |
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
# info.asset_name, info.schema_class, info.is_compiled
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

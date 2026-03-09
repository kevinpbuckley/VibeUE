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
```

### Tasks

```python
# Find available task types
types = unreal.StateTreeService.get_available_task_types()

# Add a task to a state
unreal.StateTreeService.add_task("/Game/AI/MyBehavior", "Root/Idle", "FStateTreeDelayTask")
unreal.StateTreeService.add_task("/Game/AI/MyBehavior", "Root", "FStateTreeRunSubtreeTask")
```

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

### Advanced Editor Config (Use execute_python_code)

Use `unreal.StateTreeService` for structure + compile/save. For transcript-level editor workflows
(S1 L5 -> S2 L4), use `execute_python_code` for advanced edits that are not currently first-class service methods:

- Configure state descriptions and theme colors
- Add/edit StateTree parameters and default values
- Bind task properties (e.g. debug text bindable text, delay duration bindings)
- Configure StateTree component overrides on Blueprint instances

Recommended pattern:

1. Use `unreal.StateTreeService` methods (`create_state_tree`, `add_state`, `add_task`, `add_transition`, `compile_state_tree`, `save_state_tree`) for structure.
2. Use `execute_python_code` for advanced editor metadata, component wiring, and bindings.

## COMMON_MISTAKES

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

### ⚠️ Bool Properties Drop the `b` Prefix in Python

UE Python bindings strip the `b` prefix from bool UPROPERTY names and convert to snake_case.

```python
# C++ field:     bSuccess    → Python: result.success
# C++ field:     bEnabled    → Python: state_info.enabled
# C++ field:     bIsCompiled → Python: info.is_compiled

result = unreal.StateTreeService.compile_state_tree(path)
print(result.success)   # NOT result.b_success or result.bSuccess
```

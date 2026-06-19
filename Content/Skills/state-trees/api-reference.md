---
name: api-reference
description: StateTreeService API reference - discovery, asset creation, state management, and tasks (per-state task add/inspect/set property patterns)
---

This sub-doc continues from skill.md → "API Reference". For evaluators, global tasks, transitions, bindings, theme colors, and editor utilities see `api-bindings.md`.

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
# info.root_parameters  → list of FStateTreeParameterInfo  (NOT .root_parameter_names)
# info.last_compile_status, info.asset_path
#
# Each FStateTreeParameterInfo: .name, .type, .default_value  (NOT .current_value)
# Each FStateTreeThemeColorInfo: .display_name, .color, .used_by_states
#   (NOT .referencing_state_paths)

# Each FStateTreeStateInfo has:
#   .name, .path, .state_type, .selection_behavior, .enabled
#   .theme_color (display name of assigned color, empty if none)
#   .tasks, .enter_conditions, .transitions, .child_paths
# NOTE: Do NOT access .expanded — it may not be exposed depending on the
#       compiled plugin version. Use set_state_expanded() directly instead.
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

# Change type of an existing state: "State", "Group", "Subtree"
unreal.StateTreeService.set_state_type("/Game/AI/MyBehavior", "Peaceful", "Subtree")

# Linked type — links to another subtree in the same tree
unreal.StateTreeService.set_linked_subtree("/Game/AI/MyBehavior", "Root/Extension", "Peaceful")

# LinkedAsset type — links to a different StateTree asset
unreal.StateTreeService.set_linked_asset("/Game/AI/MyBehavior", "Root/Extension", "/Game/AI/OtherBehavior")

# Move a state in-place to a new parent. This preserves the original state object and its data.
unreal.StateTreeService.move_state("/Game/AI/MyBehavior", "Root/Idle", "Root/BehaviorGroup")

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

#### Editor State Selection

Use `select_state` to highlight a state in the StateTree editor panel (equivalent to clicking it).

**Trigger:** If the user asks to "focus", "view", "open", or "select" a state — they all mean the same thing. Use this workflow for all of them.

**Also:** After ANY modification to a state (add task, add transition, set property, etc.), always call `select_state` on the state you just changed so the user can see the result in the editor.

```python
import unreal

# Open the asset first (if not already open) via the engine AssetTools toolset:
#   call_tool(toolset="AssetTools", tool="OpenAsset", args={"asset_path": "/Game/AI/ST_Cube"})

# Expand parents so the state is visible
unreal.StateTreeService.set_state_expanded("/Game/AI/ST_Cube", "Root", True)

# Select the state — highlights it in the editor panel
unreal.StateTreeService.select_state("/Game/AI/ST_Cube", "Root/Idle")
```

`select_state` calls `FStateTreeViewModel::SetSelection` via `UStateTreeEditingSubsystem`, which is exactly what the editor does when the user clicks a state node. The asset must already be open in an editor tab.

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

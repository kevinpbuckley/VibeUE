---
name: changelog
description: New StateTreeService methods added in Phase 1 and Phase 2 - state properties, root parameters, per-instance overrides, transition editing, extended task management, utility considerations, conditions, evaluator/global task management, and extended FStateTreeInfo/StateInfo/NodeInfo/TransitionInfo fields
---

This sub-doc continues from skill.md → "New Methods (Phase 1 & 2)".

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
    event_tag="",                     # gameplay tag for OnEvent trigger
    event_payload_struct="",          # e.g. "FStartChasingPayload", "None" to clear
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

### Utility AI Considerations

Considerations drive **utility-based state selection**. When a parent state has `SelectionBehavior` set to
`TrySelectChildrenWithHighestUtility` or `TrySelectChildrenAtRandomWeightedByUtility`, each child state
has a `Considerations` array. Each consideration computes a 0–1 score; scores are multiplied together
to produce the child's final utility score.

#### The three built-in consideration types

| Short alias | Full struct name | Purpose |
|---|---|---|
| `"Constant"` | `FStateTreeConstantConsideration` | Static score (0–1 float) — set via `Constant` property |
| `"FloatInput"` | `FStateTreeFloatInputConsideration` | Score driven by a bound float — `Input` property must be **bound** to context or a root parameter |
| `"EnumInput"` | `FStateTreeEnumInputConsideration` | Score driven by an enum value — `Input` property must be **bound** |

Short aliases (`"Constant"`, `"FloatInput"`, `"EnumInput"`) and the full `F`-prefixed struct names are
both accepted by all consideration methods.

#### Setting up a Utility parent state

```python
import unreal

st_path = "/Game/AI/MyBehavior"

# Step 1: Set the parent's selection behavior to utility-based
unreal.StateTreeService.set_selection_behavior(st_path, "Root", "TrySelectChildrenWithHighestUtility")

# Step 2: Add a Constant consideration to each child state (score = 1.0 means equal weight by default)
unreal.StateTreeService.add_consideration(st_path, "Root/Idle", "Constant")
unreal.StateTreeService.add_consideration(st_path, "Root/Walking", "Constant")

# Step 3: Set the Constant value (clamp 0–1)
unreal.StateTreeService.set_consideration_property_value(st_path, "Root/Idle", "Constant", "Constant", "0.3")
unreal.StateTreeService.set_consideration_property_value(st_path, "Root/Walking", "Constant", "Constant", "0.7")

# Step 4: Compile and save
result = unreal.StateTreeService.compile_state_tree(st_path)
assert result.success, result.errors
unreal.StateTreeService.save_state_tree(st_path)
```

#### Adding and inspecting considerations

```python
import unreal

st_path = "/Game/AI/MyBehavior"
state_path = "Root/Chasing"

# Discover all registered consideration struct names
types = unreal.StateTreeService.get_available_consideration_types()
for t in types:
    print(t)  # e.g. FStateTreeConstantConsideration, FStateTreeFloatInputConsideration, ...

# Add a FloatInput consideration (Input property must later be bound)
unreal.StateTreeService.add_consideration(st_path, state_path, "FloatInput")

# Inspect properties on the just-added consideration (MatchIndex -1 = last match)
props = unreal.StateTreeService.get_consideration_property_names(st_path, state_path, "FloatInput")
for p in props:
    print(f"{p.name}: {p.type} = {p.current_value!r}")
# Typical FloatInput output:
#   Input: float = 0.000000          ← bindable; must be bound at runtime
#   Interval: FFloatInterval          ← remaps Input range to [0,1]
#   Interval.Min: float = 0.000000
#   Interval.Max: float = 1.000000

# Remove a consideration by 0-based index
unreal.StateTreeService.remove_consideration(st_path, state_path, 0)
```

#### Check before adding — considerations accumulate

```python
import unreal

st_path = "/Game/AI/MyBehavior"
state_path = "Root/Idle"

# CORRECT — check first
info = unreal.StateTreeService.get_state_tree_info(st_path)
for state in info.all_states:
    if state.path == state_path:
        existing = [c.struct_type for c in state.considerations]
        print(f"Existing considerations: {existing}")
        if not any("Constant" in s for s in existing):
            unreal.StateTreeService.add_consideration(st_path, state_path, "Constant")
            print("Added Constant consideration")
        else:
            print("Already has a Constant consideration, skipping")
```

#### Setting the Constant property

```python
import unreal

st_path = "/Game/AI/MyBehavior"

# Constant consideration — set the score directly (clamp 0–1)
unreal.StateTreeService.set_consideration_property_value(
    st_path, "Root/Idle", "Constant", "Constant", "1.0")

# Multiple Constant considerations on the same state — use MatchIndex
# MatchIndex 0 = first, 1 = second, -1 = last
unreal.StateTreeService.set_consideration_property_value(
    st_path, "Root/Idle", "Constant", "Constant", "0.5", 0)  # first Constant
unreal.StateTreeService.set_consideration_property_value(
    st_path, "Root/Idle", "Constant", "Constant", "0.9", 1)  # second Constant
```

#### Binding FloatInput or EnumInput to context or a root parameter

A `FloatInput` or `EnumInput` consideration's `Input` property must be **bound** — you cannot set
a raw float value on it. Use the binding methods instead:

```python
import unreal

st_path = "/Game/AI/MyBehavior"
state_path = "Root/Chasing"

# Bind FloatInput.Input to a root parameter named "ThreatLevel"
unreal.StateTreeService.bind_consideration_property_to_root_parameter(
    st_path, state_path, "FloatInput", "Input", "ThreatLevel")

# Bind FloatInput.Input to a context actor property
unreal.StateTreeService.bind_consideration_property_to_context(
    st_path, state_path, "FloatInput", "Input",
    "Actor",           # context name
    "HealthPercent"    # context property path
)

# Unbind a consideration property
unreal.StateTreeService.unbind_consideration_property(
    st_path, state_path, "FloatInput", "Input")
```

#### ⚠️ FloatInput / EnumInput Require a Bound Input — They Cannot Be Compiled Without One

Adding a `FloatInput` or `EnumInput` consideration without binding `Input` will cause a **compile
error**. Always bind `Input` before compiling.

```python
# WRONG — FloatInput.Input is unbound; compile will fail
unreal.StateTreeService.add_consideration(st_path, state_path, "FloatInput")
unreal.StateTreeService.compile_state_tree(st_path)  # ERROR: Input is unbound

# CORRECT — bind first, then compile
unreal.StateTreeService.add_consideration(st_path, state_path, "FloatInput")
unreal.StateTreeService.bind_consideration_property_to_root_parameter(
    st_path, state_path, "FloatInput", "Input", "ThreatLevel")
unreal.StateTreeService.compile_state_tree(st_path)
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

# Bind an enter condition property to context data (e.g. bind "Object" to Actor.TargetPawn)
unreal.StateTreeService.bind_enter_condition_property_to_context(
    path, "Root/Idle", "StateTreeObjectIsValidCondition", "Object",
    "Actor", "TargetPawn")

# Bind an enter condition property to a property exposed by a global task
unreal.StateTreeService.bind_enter_condition_property_to_global_task_property(
    path, "Peaceful/Patrol", "StateTreeObjectIsValidCondition", "Object",
    "STT_PatrolManagement", "PatrolPointManager")

# Bind a transition condition property to context data
unreal.StateTreeService.bind_transition_condition_property_to_context(
    path, "Root/Idle", 0, "StateTreeObjectIsValidCondition", "Object",
    "Actor", "TargetPawn")

# Leave ContextPropertyPath empty to bind the whole context object
unreal.StateTreeService.bind_transition_condition_property_to_context(
    path, "Root", 0, "StateTreeObjectIsValidCondition", "Object", "Actor")

# Bind a transition condition property to the transition's event payload
# (transition must have RequiredEvent.PayloadStruct set)
payload_props = unreal.StateTreeService.get_transition_event_payload_property_names(path, "Root/Chasing", 0)
for p in payload_props:
    print(f"{p.name}: {p.type} = {p.current_value!r}")

unreal.StateTreeService.bind_transition_condition_property_to_event_payload(
    path, "Root/Chasing", 0, "StateTreeObjectIsValidCondition", "Object", "TargetPawn")
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

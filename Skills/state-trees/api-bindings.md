---
name: api-bindings
description: The 24 surviving StateTreeService binding tools and the workflows around them - evaluators and global tasks, transitions and the OnDelegate workflow, the two-call context actor class, bindings to context / root parameters / global tasks / evaluators, reading bindings back, StateTreeComponent assignment, theme colors, expand/collapse
---

This sub-doc is the binding guide. For the full inventory (VibeUE's 45 survivors, Epic's tools, the
`UStateTreeState` property keys and the removed-tool table) see `api-reference.md`; for the ownership
split, the two transition rules and the gotchas see `SKILL.md`.

**All 24 `bind_*` / `unbind_*` tools survive** — they are `unreal.StateTreeService` calls inside
`execute_python`. What changed around them: **adding** nodes, reading node properties, compiling and
saving are now Epic's, through `call_tool`, with these `toolset_name` strings:

```python
ST    = "state_tree_toolset.state_tree.StateTreeToolset"   # Epic's StateTree toolset (Python)
OBJ   = "EditorToolset.ObjectToolset"                      # C++
ASSET = "EditorToolset.AssetToolset"                       # C++
```

The tool names below are verified; **run `describe_toolset` before relying on an exact parameter
list.** Epic arguments are written `{...}` wherever the spelling was not verified. `SKILL.md`
explains how a toolset's registered name is derived, if you need another one.

### Evaluators & Global Tasks

**Evaluators** run every tick and feed computed data to all states (read-only output).
**Global Tasks** run for the entire lifetime of the StateTree (full task lifecycle: EnterState, Tick, ExitState).

Both support C++ structs and Blueprint assets — **adding them is Epic's job now**, with a separate
tool for the Blueprint variant.

#### Evaluators

```python
# Find available evaluator types — VibeUE (nothing in the engine surface enumerates node structs)
types = unreal.StateTreeService.get_available_evaluator_types()

# Add one — Epic. add_evaluator takes the struct; add_blueprint_evaluator takes the STE_* Blueprint.
call_tool(tool_name="add_evaluator",           toolset_name=ST, arguments={...})  # FMyCustomEvaluator
call_tool(tool_name="add_blueprint_evaluator", toolset_name=ST, arguments={...})  # STE_PatrolPointManagement
# Remove one: remove_evaluator
```

#### Global Tasks

```python
# Find available task types — VibeUE. Also gives the registered _C name for Blueprint tasks.
types = unreal.StateTreeService.get_available_task_types()
for t in types:
    print(t)  # e.g. "FStateTreeDelayTask", "STT_PatrolManagement_C"

# Add one — Epic
call_tool(tool_name="add_global_task",           toolset_name=ST, arguments={...})  # FStateTreeDelayTask
call_tool(tool_name="add_blueprint_global_task", toolset_name=ST, arguments={...})  # STT_PatrolManagement
# Remove one: remove_global_task
```

Never pass `"StateTreeBlueprintTaskWrapper"` to an adder — that is the internal wrapper struct. For
an `STT_*` Blueprint, use `add_blueprint_global_task`.

#### ⚠️ Always Check Before Adding — Global Tasks Accumulate

Nothing de-duplicates for you, so read the editor data's global tasks first:
`get_editor_data` (Epic) gives you the editor data object, `ObjectToolset.GetProperties` on it reads
the global task list, and `get_node_description` (Epic) gives each node's display text.

```python
# 1. Read what is already there
call_tool(tool_name="get_editor_data", toolset_name=ST,  arguments={...})
call_tool(tool_name="GetProperties",   toolset_name=OBJ, arguments={...})

# 2. Only then add — and only if it is missing
call_tool(tool_name="add_blueprint_global_task", toolset_name=ST, arguments={...})
```

#### Binding Global Task Properties

Global tasks support the same property binding patterns as per-state tasks. The bindings are VibeUE
(`bind_global_task_property_to_root_parameter`, `bind_global_task_property_to_context`); reading and
writing a global task's property *values* is Epic's `get_node_instance_data` /
`set_node_instance_data`.

```python
import unreal

st_path = "/Game/StateTree/ST_Cube"

# Inspect what properties the global task exposes — Epic. The keys ARE the property names.
#   call_tool(tool_name="get_node_instance_data", toolset_name=ST, arguments={...})
# Set a property value directly — Epic.
#   call_tool(tool_name="set_node_instance_data", toolset_name=ST, arguments={...})

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

Four steps, and only step 3 is VibeUE.

```python
# 1. Check whether it is already present — Epic
call_tool(tool_name="get_editor_data", toolset_name=ST,  arguments={...})
call_tool(tool_name="GetProperties",   toolset_name=OBJ, arguments={...})

# 2. Add it if missing, then inspect its properties — Epic
call_tool(tool_name="add_blueprint_global_task", toolset_name=ST, arguments={...})
call_tool(tool_name="get_node_instance_data",    toolset_name=ST, arguments={...})
```

```python
# 3. Bind to a root parameter — VibeUE, inside execute_python
import unreal
st_path = "/Game/StateTree/ST_Cube"
unreal.StateTreeService.bind_global_task_property_to_root_parameter(
    st_path, "STT_PatrolManagement", "PatrolTag", "PatrolTag")
```

```python
# 4. Compile, then save — Epic. Epic's writes dirty the package but never compile.
call_tool(tool_name="compile",   toolset_name=ST,    arguments={...})
call_tool(tool_name="SaveAsset", toolset_name=ASSET, arguments={...})
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
#   GotoState          — go to a specific state (the element's target State must be set, in the
#                        SAME write that adds the element — Rule 1)
#   Succeeded          — complete this state as succeeded
#   Failed             — complete this state as failed
#   NextState          — go to the next sibling state
#   NextSelectableState — go to the next eligible sibling

# Priorities: Low, Normal (default), Medium, High, Critical
```

Those triggers, types and priorities are unchanged — what changed is how a transition is written.
A transition is an element of the state's `Transitions` array, so adding one is a single
`ObjectToolset.SetProperties` that **appends one element carrying trigger, type and target state
together**:

```python
# Add — ONE write, full element (Rule 1 in SKILL.md). An element whose target State.ID is invalid
# gets its Trigger silently overwritten to OnStateCompleted and is retargeted at the root state.
call_tool(tool_name="SetProperties", toolset_name=OBJ, arguments={...})  # append to Transitions

# Remove — drop that one element
call_tool(tool_name="SetProperties", toolset_name=OBJ, arguments={...})

# Retarget / change trigger / change type — change THAT element's fields, in place.
# Never drop-and-append (it mints a new ID and loses the element's conditions), and never
# rewrite the whole array (Rule 2: it regenerates every ID and orphans every binding).
call_tool(tool_name="SetProperties", toolset_name=OBJ, arguments={...})
```

```python
# Reorder — VibeUE. No toolset emits EPropertyChangeType::ArrayMove, so this is the only safe way.
unreal.StateTreeService.move_transition("/Game/AI/MyBehavior", "Root/Idle", 1, 0)
```

### OnDelegate Transitions — Full Workflow

`OnDelegate` transitions fire when a task's `FStateTreeDelegateDispatcher` property broadcasts.
This requires **three steps**: add the dispatcher variable, set the trigger, bind the transition.

#### ⚠️ Expected Compile Error — Do NOT Revert

After setting that transition element's `Trigger` to `OnDelegate`, compiling will produce:

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
transition_index = 0  # index within the state's Transitions array (read it with ObjectToolset.GetProperties)

# Step 1: Add the FStateTreeDelegateDispatcher member variable the binding needs.
# VibeUE's add_member_variable is gone, and so is the reason it existed: Epic's
# BlueprintToolsets.BlueprintToolset ships AddVariable for basic types, AddObjectVariable for
# object types and AddStructVariable for structs, so a struct variable is now engine work.
# ListVariables is the existence check (variable_exists is gone too).
#
#   call_tool ListVariables      {"blueprint": {"refPath": bp_path}}
#   call_tool AddStructVariable  {"blueprint": {"refPath": bp_path},
#                                 "name": "FinishRotatingDispatcher",
#                                 "structType": {"refPath": "/Script/StateTreeModule.StateTreeDelegateDispatcher"}}
#   call_tool CompileBlueprint   {"blueprint": {"refPath": bp_path}}
#   call_tool SaveAsset          {"assetPath": bp_path}               # EditorToolset.AssetToolset
#
# Parameters are blueprint, name, structType, options. Check structType's exact form with
# get_symbol name="BlueprintToolsets.BlueprintToolset.AddStructVariable" before writing it.

# Step 2: Set that transition element's Trigger to OnDelegate — Epic ObjectToolset.
# Change the ONE element's field in place; do not drop-and-re-add it and do not rewrite the array.
#   call_tool(tool_name="SetProperties", toolset_name=OBJ, arguments={...})  # Transitions[0].Trigger

# Step 3: Bind the transition to the dispatcher property — VibeUE
result = unreal.StateTreeService.bind_transition_to_delegate(
    st_path, state_path, transition_index,
    "STT_Rotate",                  # task name (display name, Blueprint name, or struct type)
    "FinishRotatingDispatcher"     # the FStateTreeDelegateDispatcher variable name
)
assert result, "bind_transition_to_delegate failed"

# Step 4: Compile, then save — Epic. Should now succeed with no delegate errors.
#   call_tool(tool_name="compile",   toolset_name=ST,    arguments={...})
#   call_tool(tool_name="SaveAsset", toolset_name=ASSET, arguments={...})
```

#### Firing the Dispatcher from the Blueprint Task

In `STT_Rotate`'s Blueprint graph, call the dispatcher to trigger the transition:

```python
# The dispatcher is called like a function in Blueprint — add a "Call FinishRotatingDispatcher" node
# connected to whatever execution flow should end the state (e.g. after a timer, animation, etc.)
```

#### Notes

- `FStateTreeDelegateDispatcher` is a USTRUCT — use type string `"FStateTreeDelegateDispatcher"` with `add_member_variable`.
- `bind_transition_to_delegate` matches the task by its **registered class name** (`STT_Rotate_C`) — the Blueprint asset path that `add_task` accepts is NOT accepted here (live-verified).
- The dispatcher variable must be on the task that is **in the same state** as the `OnDelegate` transition.
- After `bind_transition_to_delegate`, the compile error about the missing binding will resolve.

### Compile & Save

Both are Epic's, in that order. Epic's writes mark the package dirty but never compile, so a save
without a compile persists an uncompiled tree.

```python
call_tool(tool_name="compile",   toolset_name=ST,    arguments={...})  # check the result before saving
call_tool(tool_name="SaveAsset", toolset_name=ASSET, arguments={...})
```

A batch of Epic edits is not one undo step — wrap it in your own `unreal.ScopedEditorTransaction` if
the user should be able to undo the whole thing at once.

### Setting the Context Actor Class — TWO calls

`set_context_actor_class` is gone, and the replacement is **not** one property write. The class lives
on the schema subobject, not on the editor data:

1. `ObjectToolset.GetProperties` on the **editor data** for `Schema` → take the schema subobject's
   `refPath`.
2. `ObjectToolset.SetProperties` on **that schema object** with `ContextActorClass`.

```python
call_tool(tool_name="GetProperties", toolset_name=OBJ, arguments={...})  # editor data -> Schema refPath
call_tool(tool_name="SetProperties", toolset_name=OBJ, arguments={...})  # schema object -> ContextActorClass
call_tool(tool_name="compile",       toolset_name=ST,  arguments={...})
```

Writing a nested `Schema.ContextActorClass` key on the editor data instead **silently does not
work**: the editor data becomes the notify object, so
`UStateTreeComponentSchema::PostEditChangeChainProperty` never fires and `ContextDataDescs` never
rebuilds — and every context binding keeps failing.

### Property Bindings (Binding Task Properties to Context or Parameters)

Bindings connect a task's property to a **context object** (e.g. the Actor running the StateTree) or
a **root parameter**. This is how tasks access external data at runtime.

#### ⚠️ CRITICAL: Context Must Be Set Before Binding

`bind_task_property_to_context` will **fail silently** if the StateTree has no context actor class.
Read the editor data's `Schema` with `ObjectToolset.GetProperties` first — that read is also the
diagnosis — and if no class is set, do the two-call dance above before attempting any context
binding.

#### Full Binding Workflow

```python
# Step 1: Is a context actor class set? Read the editor data's Schema — Epic.
call_tool(tool_name="GetProperties", toolset_name=OBJ, arguments={...})

# Step 2: If not, set it on the SCHEMA OBJECT — Epic. The #1 reason bindings fail.
call_tool(tool_name="SetProperties", toolset_name=OBJ, arguments={...})  # ContextActorClass

# Step 3: Discover the task's properties — Epic. The returned keys ARE the property names.
call_tool(tool_name="get_node_instance_data", toolset_name=ST, arguments={...})
```

```python
# Step 4: Bind the task property to the context Actor — VibeUE, inside execute_python
import unreal

st_path = "/Game/StateTree/ST_Cube"
state_path = "Root"
# For Blueprint tasks, use the Blueprint class name (STT_Rotate_C or STT_Rotate)
# OR "StateTreeBlueprintTaskWrapper" — both work.
task_struct = "STT_Rotate_C"

#   - context_name="Actor" matches the first context descriptor (default)
#   - context_property_path="" means bind the entire actor reference
result = unreal.StateTreeService.bind_task_property_to_context(
    st_path, state_path, task_struct,
    "Cube",           # task property to bind
    "Actor",          # context name
    ""                # context property path (empty = whole object)
)
print(f"Bind result: {result}")
```

```python
# Step 5: Compile, then save — Epic
call_tool(tool_name="compile",   toolset_name=ST,    arguments={...})
call_tool(tool_name="SaveAsset", toolset_name=ASSET, arguments={...})
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

#### Binding FROM Evaluator Outputs (issue #556)

Tasks and conditions can read data an evaluator produces — the evaluator-source mirror of the
`*_to_global_task_property` family:

```python
# State task property <- evaluator output
unreal.StateTreeService.bind_task_property_to_evaluator_property(
    st_path, "Peaceful/Patrol", "STT_MoveToPatrolPoint",
    "PatrolPointManager",           # target property on the state task
    "STE_PatrolPointManagement",    # source evaluator name
    "PatrolPointManager")           # source property on the evaluator

# Enter condition property <- evaluator output
unreal.StateTreeService.bind_enter_condition_property_to_evaluator_property(
    st_path, "Peaceful/Patrol", "STC_CanPatrol", "bAllowed",
    "STE_PatrolPointManagement", "bHasPoints")

# Transition condition property <- evaluator output (transition_index after state_path)
unreal.StateTreeService.bind_transition_condition_property_to_evaluator_property(
    st_path, "Peaceful/Patrol", 0, "STC_CanPatrol", "bAllowed",
    "STE_PatrolPointManagement", "bHasPoints")
```

#### Binding Evaluator Properties (evaluator as TARGET)

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

#### Reading Bindings (engine-native on 5.9)

VibeUE's `get_bindings` is **gone on the 5.9 branch**: UE 5.9 reads bindings natively through
`StateTreeEditorData`, and its `FStateTreeBindingInfo` carries the typed addresses VibeUE's
flattened string form could not. Use the engine API instead.

```python
st = unreal.load_asset("/Game/AI/ST_Zombie_AI")
ed = unreal.StateTreeEditorData.get_editor_data(st)   # raises nothing; returns None if absent

# Every binding in the asset
for b in ed.get_bindings():
    print(b.target.owner.name, b.target.path, "<-", b.source.owner.name, b.source.path,
          b.target.usage, b.source.compatibility)

# Only bindings whose TARGET lives in one state -- the same filter get_bindings used to offer.
# ed.get_bindings(state) takes the state OBJECT. Enumerate states with Epic's get_all_states /
# get_root_states / get_children through call_tool. There is NO `unreal.StateTreeToolset`, so
# never write unreal.StateTreeToolset.get_all_states(st).
chase_state = ...   # the UStateTreeState for "Chase", from Epic's get_all_states (call_tool)
is_dest_bound = any(b.target.path == "Destination" for b in ed.get_bindings(chase_state))
```

Fields of the engine's `FStateTreeBindingInfo`:
- `binding` -- the stored `FStateTreePropertyPathBinding`
- `source` / `target` -- `FStateTreeBindingEndpoint`, each with `owner_id`, `binding_path` (typed
  address you can pass straight back into `get_binding` and the binding edits), `path` (internal
  property names; empty addresses the whole owner), `owner` (`FStateTreeBindableStructDesc`),
  `usage`, and `compatibility`
- `completion_condition` -- set only for a whole-task source bound to a delegate listener

`ed.get_binding(target_path, found)` returns one exact target binding. For an **output** binding the
source address is the *destination* of the copy, the inverse of an ordinary source -> target read.

VibeUE's two surviving property readers — `get_transition_condition_property_names` and
`get_transition_event_payload_property_names` — also report bindings per property via `p.is_bound`
and `p.bound_to` (e.g. `"RootParameter:Parameters.IdlingTime"`). Only the exact bound path is marked;
a child of a bound struct property (`Offset.Z` under a bound `Offset`) reads `is_bound == False`.
Array-element bindings (`Arr[2]`) are listed by the engine's `ed.get_bindings()` but never mark
`is_bound`, because these readers do not descend into arrays. An output binding's `bound_to` ends in
` (output)`: that property writes the named source.

```python
for p in unreal.StateTreeService.get_transition_condition_property_names(
        st_path, "Root/Idle", 0, "StateTreeObjectIsValidCondition"):
    print(p.name, p.is_bound, p.bound_to)
# Object True ContextData:Actor.TargetPawn
```

For task, evaluator, global task, consideration and enter condition properties, read the values with
Epic's `get_node_instance_data` and the bindings with the engine-native `ed.get_bindings()` above.

### Assigning a StateTree to a StateTreeComponent on a Blueprint

`StateTreeComponent` has **two** properties that look related — only `StateTreeRef` is shown in the
editor Details panel. Always set `StateTreeRef`, never `StateTree`.

`BlueprintService.set_component_property` was removed. Resolve the component template through the
engine, then write the property with `ObjectToolset.SetProperties`:

```
call_tool GetDefaultObject  {"blueprint": {"refPath": "<BP path>"}}       # BlueprintToolsets.BlueprintToolset
call_tool GetComponents     {"actor": {"refPath": "<CDO refPath>"}}       # EditorToolset.ActorToolset
call_tool GetPropertySchema {"instance": {"refPath": "<component refPath>"}, "root": "StateTreeRef"}
call_tool SetProperties     {"instance": {"refPath": "<component refPath>"}, "values": "{...}"}
call_tool CompileBlueprint  {"blueprint": {"refPath": "<BP path>"}}       # BlueprintToolsets.BlueprintToolset
call_tool SaveAsset         {"assetPath": "<BP path>"}                    # EditorToolset.AssetToolset
```

`StateTreeRef` is an `FStateTreeReference` struct, not a bare asset path -- read its shape with
`GetPropertySchema` before writing. The rule itself is unchanged: write `StateTreeRef`, never
`StateTree`, or the Details panel shows None.

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

`set_state_expanded` is write-only. `bExpanded` is a bare `UPROPERTY(meta = (ExcludeFromHash))` with
no `CPF_Edit` and no `CPF_BlueprintVisible`, so the toolset refuses it for read *and* write and it is
invisible even to `ListProperties` — there is no read path for it any more.

### Which door for which edit

| Edit | Door |
|---|---|
| States, node lists (tasks / conditions / considerations / evaluators / global tasks), node property values, compile | **Epic `StateTreeToolset`** via `call_tool` |
| State name, description, tag, type, selection behavior, weight, enabled, links; transitions; the context actor class; saving | **Epic `ObjectToolset` / `AssetToolset`** via `call_tool` |
| Asset creation, all property bindings, node-*type* discovery, theme colors, root parameters, per-instance overrides, `move_transition`, transition conditions, task flags, select/expand | **VibeUE `unreal.StateTreeService`** inside `execute_python` |
| Blueprint variables and components, making properties instance-editable, level-actor work | **`BlueprintService`** and the engine Blueprint tools — outside this skill, load `blueprints` |

Recommended pattern:

1. Structure first, through Epic's toolset; read back what you wrote with `get_node_instance_data`
   or `ObjectToolset.GetProperties` rather than assuming.
2. Then VibeUE for parameters, bindings and colors.
3. Then Epic `compile`, then `AssetToolset.SaveAsset`.
4. `select_state` last, so the user sees what changed.

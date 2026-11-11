---
name: state-trees
display_name: StateTree Behavior
description: Author StateTree assets on UE 5.9's split ownership - Epic's StateTreeToolset owns structural authoring (states, tasks, enter conditions, considerations, evaluators, node data, compile) and ObjectToolset/AssetToolset own state properties, transitions and saving, while VibeUE's StateTreeService keeps asset creation, all property bindings, node-type discovery, theme colors and root parameters. Use when the user asks to build a StateTree, add states/tasks/transitions, bind task properties, set up Utility AI considerations, or wire StateTree delegate transitions.
vibeue_classes:
  - StateTreeService
unreal_classes:
  - StateTreeToolset
  - ObjectToolset
  - AssetToolset
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

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "ai-and-navigation"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# StateTree Skill

StateTree is Unreal Engine's hierarchical state machine system for AI behavior and game logic.

**On 5.9 this domain is split.** Epic's `StateTreeToolset` owns structural authoring — states, tasks,
enter conditions, considerations, evaluators, node data and `compile`. Epic's `ObjectToolset` /
`AssetToolset` own state properties, transitions and saving. VibeUE's `StateTreeService` kept the
**45** tools the engine surface cannot reach: asset creation, all property bindings, node-*type*
discovery, theme colors, root parameters and a few otherwise unreachable flags.

**53 `StateTreeService` tools were removed on the `5-9` branch** (98 → 45). If you remember a call
that is not in the ownership table below, look it up in "Migration: the 53 removed VibeUE tools"
further down — every removed tool has a replacement.

## Who owns what in 5.9

**Two call styles. Never mix them up.**

| Who | How you call it | Names look like |
|---|---|---|
| Epic's `StateTreeToolset` (a **Python** toolset) | `call_tool` **only** | snake_case — `add_child_state`, `set_node_instance_data`, `compile` |
| Epic's `ObjectToolset` / `AssetToolset` | `call_tool` **only** | PascalCase — `GetProperties`, `SetProperties`, `SaveAsset` |
| VibeUE's 45 survivors | `unreal.StateTreeService.<snake_case>()` inside `execute_python` | `create_state_tree`, `bind_task_property_to_context` |

> ⚠️ **`unreal.StateTreeToolset.add_task(...)` does not exist.** Epic's toolsets are not `unreal.*`
> Python APIs — they are reachable *only* through `call_tool`. The mirror of that mistake is just as
> wrong: no VibeUE survivor is a `call_tool` tool.

The three `toolset_name` strings this skill uses, verified against the engine:

```python
ST    = "state_tree_toolset.state_tree.StateTreeToolset"   # Epic's StateTree toolset (Python)
OBJ   = "EditorToolset.ObjectToolset"                      # C++
ASSET = "EditorToolset.AssetToolset"                       # C++
```

Enable the **StateTreeToolset** plugin if `list_toolsets` does not show `ST`. The tool *names* in
this skill are verified, but **run `describe_toolset` before relying on an exact parameter list** —
Epic arguments are written `{...}` here wherever the spelling was not verified, and a confidently
wrong argument list is worse than a placeholder.

### Deriving a toolset's registered name

This is the single most-guessed-wrong string in the pack set, so derive it instead of guessing.
`FFunctionLibraryToolset::GetToolsetClassName` builds `<qualifier>.<ClassName>`:

- **C++ toolset** — the qualifier is the **module**: `EditorToolset.ObjectToolset`,
  `EditorToolset.AssetToolset`, `AIModuleToolset.BehaviorTreeToolset`.
- **Python toolset** — the class's package is its script path with `_PY` appended. The function
  strips everything up to and including the `Python` segment, drops `_PY`, and dot-joins the rest,
  then appends the class name. So `/StateTreeToolset/Python/state_tree_toolset/state_tree_PY` plus
  class `StateTreeToolset` gives `state_tree_toolset.state_tree.StateTreeToolset`. The engine's own
  worked example: `/ToolsetRegistry/Python/toolset_registry/tests/demo_toolset_PY` →
  `toolset_registry.tests.demo_toolset`.

A Python toolset's name therefore has a **module segment before the class name** — `state_tree.`
here. Dropping it (`state_tree_toolset.StateTreeToolset`) is the usual mistake.

| Want | Call |
|---|---|
| Create the StateTree asset | **VibeUE** `create_state_tree` (Epic deliberately has no asset creation) |
| Add / move / remove states | **Epic** `add_child_state`, `add_sub_tree`, `move_child_state`, `move_sub_tree`, `remove_child_state`, `remove_sub_tree` |
| Add / move / remove tasks, enter conditions, considerations, evaluators, global tasks | **Epic** `add_task`, `add_enter_condition`, `add_consideration`, `add_evaluator`, `add_global_task`, `move_task`, `remove_task`, `remove_enter_condition`, `remove_consideration`, `remove_evaluator`, `remove_global_task` |
| The same, for an `STT_*` **Blueprint** node | **Epic** `add_blueprint_task`, `add_blueprint_enter_condition`, `add_blueprint_consideration`, `add_blueprint_evaluator`, `add_blueprint_global_task` |
| Read the tree | **Epic** `get_editor_data`, `get_root_states`, `get_all_states`, `get_children`, `get_node_description` |
| Read / write a node's properties | **Epic** `get_node_instance_data` / `set_node_instance_data` |
| State name, description, tag, type, selection behavior, weight, enabled, links | **Epic** `ObjectToolset.SetProperties` on the `UStateTreeState` (key table in Group C below) |
| Transitions — add, remove, retarget | **Epic** `ObjectToolset.SetProperties` on the state's `Transitions` — **read the two transition rules first** |
| Reorder a transition | **VibeUE** `move_transition` |
| Which node *types* exist | **VibeUE** `get_available_task_types`, `get_available_condition_types`, `get_available_consideration_types`, `get_available_evaluator_types` |
| Property bindings (24 `bind_*` / `unbind_*` tools) | **VibeUE** — see `api-bindings.md` |
| Theme colors | **VibeUE** `get_theme_colors`, `set_state_theme_color`, `rename_theme_color` |
| Root parameters | **VibeUE** `add_or_update_root_parameter`, `remove_root_parameter`, `rename_root_parameter` |
| Per-instance parameter override on a placed actor | **VibeUE** `set_component_parameter_override` |
| Transition conditions (add / inspect / set / bind) | **VibeUE** `add_transition_condition`, `get_transition_condition_property_names`, `set_transition_condition_property_value`, the `bind_transition_condition_*` family |
| Task flags the engine cannot path-address | **VibeUE** `set_task_enabled`, `set_task_considered_for_completion` |
| Select / expand a state in the editor | **VibeUE** `select_state`, `set_state_expanded` |
| Compile | **Epic** `compile` |
| Save | **Epic** `AssetToolset.SaveAsset` (after `compile`) |
| Find StateTree assets / available schemas | **Epic** `AssetToolset.FindAssets` filtered to class `StateTree` / `ObjectToolset.SearchSubclasses` on `UStateTreeSchema` |
| Refresh the open StateTree editor | **Epic** `request_ui_refresh` |

## 🚨 The two transition rules — read before touching `Transitions`

These are the two most expensive mistakes on the 5.9 StateTree surface. Both live in
`UStateTreeState::PostEditChangeChainProperty`.

### Rule 1 — a new transition must carry its target state in the SAME write

Appending to `Transitions` with `ObjectToolset.SetProperties` is how transitions are added now. The
engine mints the new `Transition.ID` on the `ArrayAdd` notification — and in that same handler, if
the new element's `State.ID` is invalid it **overwrites `Trigger` to `OnStateCompleted`**, retargets
`State` to the root state, and trips an `ensureMsgf`:

> "Set transition trigger but did not set target state. Trigger will be overwritten."

`HandleArrayAdds` converts the inserted element's JSON in the same call, so **pass the full element —
trigger, type and target state together — in one `SetProperties`.** Do **not** add an empty
transition and then fill it in.

### Rule 2 — NEVER rewrite the whole `Transitions` array

Add and remove are safe *only* because the engine applies them incrementally: an add mints only the
new element's GUID, and a remove is diffed against the engine's `PreEditTransitions` copy, so every
existing ID survives. `EditorBindings` is keyed on those IDs.

A wholesale clear-then-re-add regenerates every ID and **silently orphans every binding on the
state.** Nothing errors — the bindings are simply gone.

To reorder, use VibeUE's surviving `move_transition`. Nothing in any toolset emits
`EPropertyChangeType::ArrayMove`, so a same-size rewrite would never notify the transition ViewModel
either.

```python
# Reorder — VibeUE, inside execute_python
unreal.StateTreeService.move_transition("/Game/AI/ST_Guard", "Root/Patrol", 2, 0)
```

## Other gotchas on the engine path

**(1) Epic's tools are not individually transacted.** Epic's own source says so: *"The wrappers below
do not open undo/transaction scopes of their own. Callers batching multiple edits should wrap them in
their own transactional context."* VibeUE's removed tools each opened an `FScopedTransaction`, so a
batch no longer gets one undo step for free — open your own scope
(`unreal.ScopedEditorTransaction` in Python) when a batch should undo as one user action.

**(4) Setting the context actor class takes TWO calls.** `ObjectToolset.GetProperties` on the editor
data for `Schema` to get the schema subobject's `refPath`, then `ObjectToolset.SetProperties` on
**that schema object** with `ContextActorClass`. A nested `Schema.ContextActorClass` key would make
the editor data the notify object, so `UStateTreeComponentSchema::PostEditChangeChainProperty` would
not fire and `ContextDataDescs` would never rebuild. Context bindings keep failing until this is set.

**(5) `Weight` lost its clamp.** VibeUE's `SetStateWeight` applied `FMath::Max(0, w)`. `ClampMin=0` is
only metadata and the JSON import path does not enforce it, so a negative weight now sticks. Clamp it
yourself before writing it.

**(6) Epic's writes mark the package dirty but do not compile.** Run Epic `compile`, then
`AssetToolset.SaveAsset`.

**(7) Setting `Type` has side effects, by design.** `PostEditChangeChainProperty` resets `Tasks` for a
`Group`, forces `SelectionBehavior` to `TryEnterState` and clears `Tasks` for `Linked`/`LinkedAsset`,
clears `LinkedSubtree`/`LinkedAsset` when leaving those types, and flips `Parameters.bFixedLayout`.
This is a feature of going through the engine: VibeUE used to hand-mirror these fixups and could
drift.

(Gotchas 2 and 3 are the two transition rules above.)

## Migration: the 53 removed VibeUE tools → what to call now

Everything in a left-hand column **no longer exists**. When you meet one in legacy code, an old
script or your own memory, replace it with the right-hand side.

### Group A — Epic `StateTreeToolset` (`call_tool`, snake_case)

| Removed VibeUE tool | Replacement |
|---|---|
| `AddState` | `add_child_state` (child of a state) / `add_sub_tree` (new root subtree) |
| `MoveState` | `move_child_state` / `move_sub_tree` |
| `RemoveState` | `remove_child_state` / `remove_sub_tree` |
| `AddTask` | `add_task` (struct task) / `add_blueprint_task` (`STT_*` Blueprint) |
| `MoveTask` | `move_task` |
| `RemoveTask` | `remove_task` |
| `AddEnterCondition` | `add_enter_condition` / `add_blueprint_enter_condition` |
| `RemoveEnterCondition` | `remove_enter_condition` |
| `AddConsideration` | `add_consideration` / `add_blueprint_consideration` |
| `RemoveConsideration` | `remove_consideration` |
| `AddEvaluator` | `add_evaluator` / `add_blueprint_evaluator` |
| `RemoveEvaluator` | `remove_evaluator` |
| `AddGlobalTask` | `add_global_task` / `add_blueprint_global_task` |
| `RemoveGlobalTask` | `remove_global_task` |
| `CompileStateTree` | `compile` |
| `RefreshEditor` | `request_ui_refresh` |
| `GetStateTreeInfo` | `get_editor_data` + `get_root_states` / `get_all_states` / `get_children` |
| `GetTaskPropertyNames` | `get_node_instance_data` (the returned keys *are* the property names) |
| `GetTaskPropertyValue` | `get_node_instance_data` |
| `SetTaskPropertyValue` | `set_node_instance_data` |
| `SetTaskPropertyValueDetailed` | `set_node_instance_data` |
| `GetConsiderationPropertyNames` | `get_node_instance_data` |
| `SetConsiderationPropertyValue` | `set_node_instance_data` |
| `GetEnterConditionPropertyNames` | `get_node_instance_data` |
| `SetEnterConditionPropertyValue` | `set_node_instance_data` |
| `GetEvaluatorPropertyNames` | `get_node_instance_data` |
| `SetEvaluatorPropertyValue` | `set_node_instance_data` |
| `GetGlobalTaskPropertyNames` | `get_node_instance_data` |
| `SetGlobalTaskPropertyValue` | `set_node_instance_data` |

Node-data reads/writes come in three pairs; pick by what you are addressing:

- `get/set_node_instance_data` — the node's **instance** struct (the per-node values shown in the
  details panel). This is the default.
- `get/set_node_template_data` — the node's **template** struct (`FStateTreeEditorNode.Node`).
- `get/set_node_execution_runtime_data` — execution-time data.

`get_node_instance_object` / `get_node_execution_runtime_object` return the UObject form for
Blueprint-based nodes. `get_node_description` returns the node's display text.

### Group B — Epic `AssetToolset` / `ObjectToolset`

| Removed VibeUE tool | Replacement |
|---|---|
| `SaveStateTree` | `compile` first, then `AssetToolset.SaveAsset` |
| `ListStateTrees` | `AssetToolset.FindAssets` filtered to class `StateTree` |
| `ListStateTreeSchemas` | `ObjectToolset.SearchSubclasses` on `UStateTreeSchema` |
| `GetRootParameters` | `ObjectToolset.GetProperties` on the editor data, key `RootParameters` |
| `GetComponentParameterOverrides` | `ObjectToolset.GetProperties` on the StateTree component |
| `GetComponentStateTreePath` | `ObjectToolset.GetProperties` on the component, key `StateTreeRef` |

### Group C — `ObjectToolset.SetProperties` on `UStateTreeState`

Every one of these is a plain `EditDefaultsOnly` UPROPERTY on `UStateTreeState`, so
`ObjectToolset.SetProperties` reaches it. Exact property keys, from `StateTreeState.h`:

| Removed VibeUE tool | Property key | Declared type |
|---|---|---|
| `RenameState` | `Name` | `FName` |
| `SetStateDescription` | `Description` | `FString` |
| `SetStateTag` | `Tag` | `FGameplayTag` |
| `SetStateType` | `Type` | `EStateTreeStateType` |
| `SetSelectionBehavior` | `SelectionBehavior` | `EStateTreeStateSelectionBehavior` |
| `SetTasksCompletion` | `TasksCompletion` | `EStateTreeTaskCompletionType` |
| `SetLinkedSubtree` | `LinkedSubtree` | `FStateTreeStateLink` |
| `SetLinkedAsset` | `LinkedAsset` | `TObjectPtr<UStateTree>` |
| `SetStateEnabled` | `bEnabled` | `bool` |
| `SetStateWeight` | `Weight` | `float` |

`SetContextActorClass` is the one that needs **two** calls — gotcha (4) above.

### Group D — `ObjectToolset` array operations

The engine's toolset property importer emits real `EPropertyChangeType::ArrayAdd` / `ArrayRemove` /
`ArrayClear` notifications, applied **incrementally**. That is what makes these safe — and why
Rule 2 above matters.

| Removed VibeUE tool | Replacement |
|---|---|
| `AddTransition` | `ObjectToolset.SetProperties` — append one element to `Transitions` (**Rule 1**) |
| `RemoveTransition` | `ObjectToolset.SetProperties` — drop one element from `Transitions` |
| `UpdateTransition` | `ObjectToolset.SetProperties` on `Transitions` with the element's fields changed |
| `RemoveTransitionCondition` | `ObjectToolset.SetProperties` — drop one element from that transition's `Conditions` |
| `SetEnterConditionOperand` | `ObjectToolset.SetProperties` on the enter condition element's `ConditionOperand` |
| `SetTransitionConditionOperand` | `ObjectToolset.SetProperties` on the transition condition element's `ConditionOperand` |

### Group E — in-plugin duplicate

| Removed VibeUE tool | Replacement |
|---|---|
| `AddOrUpdateRootFloatParameter` | `StateTreeService.AddOrUpdateRootParameter` (still a VibeUE tool) |

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
StateTreeToolset  → the StateTree ASSET's structure (states, node lists, compile)  [call_tool]
ObjectToolset     → the StateTree ASSET's state properties and transitions         [call_tool]
StateTreeService  → asset creation, bindings, root parameters, theme colors         [execute_python]
BlueprintService  → the STT Blueprint CONTENT (variables, function graphs, wiring)  [execute_python]
```

Load the `blueprints` skill (via the engine `AgentSkillToolset` — `GetSkills`, see the `vibeue`
skill) before writing any code that touches an `STT_*` Blueprint's internals.

If the request mentions timers, delayed completion, event callbacks, or screenshots of Blueprint graphs,
also load the `blueprint-graphs` skill:

```
call_tool(toolset="ToolsetRegistry.AgentSkillToolset", tool="GetSkills", args={"skills": ["blueprint-graphs"]})
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
| **Subtree / Root State** | Top-level state (usually named "Root") — created with Epic `add_sub_tree`; children come from `add_child_state` |
| **State** | A node in the tree; can have tasks, enter conditions, transitions, and child states |
| **Task** | Logic that runs while a state is active. Can be a C++ struct (`FStateTreeDelayTask`) or a **Blueprint asset** (`STT_MyTask`) |
| **Blueprint Task** | An `STT_*` Blueprint asset extending `StateTreeTaskBlueprintBase` — edited with `BlueprintService` |
| **Evaluator** | Global computation that runs every tick; provides data to all states |
| **Global Task** | Task that runs as long as the StateTree is active |
| **Transition** | Rule that moves execution from one state to another |
| **Compile** | Converts editor data to runtime format — **required after any structural change** (Epic `compile`, then `AssetToolset.SaveAsset`) |

## State Paths

VibeUE's surviving tools address states with `/`-separated paths starting from the subtree name:

```
Root                → top-level subtree
Root/Walking        → child of Root named Walking
Root/Walking/Idle   → child of Walking named Idle
```

Epic's tools address the **state itself** — the objects `get_root_states`, `get_all_states` and
`get_children` return — so resolve a path to a state object first and keep it for the rest of the
batch. Run `describe_toolset` for the exact argument names.

## ⚠️ Common Mistakes

| WRONG | CORRECT |
|-------|---------|
| `create_state_tree(...)` returns False and you retry other paths/folders | The path is rarely the problem — the **schema** is. Default `StateTreeComponentSchema` requires the GameplayStateTree plugin. List what this project actually has with `ObjectToolset.SearchSubclasses` on `UStateTreeSchema` and pass one of those explicitly |
| Creating StateTrees via `AssetTools` + `unreal.StateTreeFactory` | Fails from Python (`Cannot nativize 'StateTreeFactory'…` / returns None) — use `StateTreeService.create_state_tree()`. Epic has no asset-creation tool at all, by design |
| `unreal.StateTreeToolset.get_all_states(st)` | Epic's toolset is **`call_tool` only** — there is no `unreal.StateTreeToolset` Python class |
| `call_tool` for `bind_task_property_to_context` | VibeUE's survivors run inside `execute_python`: `unreal.StateTreeService.bind_task_property_to_context(...)` |
| `info = get_state_tree_info(path)` then `info.all_states` / `info.transitions` / `info.context_actor_class` | Removed with the tool. Structure → Epic `get_all_states` / `get_children`; node values → Epic `get_node_instance_data`; state fields and `Transitions` → `ObjectToolset.GetProperties` on the state; context actor class → `GetProperties` on the editor data's `Schema`, then on the schema object |

## Workflow

One build, two call styles, using the three toolset names from "Who owns what in 5.9":

```python
ST    = "state_tree_toolset.state_tree.StateTreeToolset"
OBJ   = "EditorToolset.ObjectToolset"
ASSET = "EditorToolset.AssetToolset"
```

**Run `describe_toolset` once for each of the three before the first write** — the argument names
below are shown as `{...}` on purpose, because this skill does not guess Epic's parameter spellings.

**Step 1 — create the asset (VibeUE, `execute_python`).** Epic has no asset creation.

```python
import unreal

created = unreal.StateTreeService.create_state_tree("/Game/AI/MyBehavior")
if not created:
    # Most common cause: the schema doesn't exist in this project. The default
    # "StateTreeComponentSchema" comes from the GameplayStateTree plugin — list what this
    # project really has with ObjectToolset.SearchSubclasses on UStateTreeSchema, then:
    #   create_state_tree("/Game/AI/MyBehavior", "<one of those>")
    pass
```

**Steps 2-6 — structure, node values, transitions, compile, save (Epic, `call_tool`).**

```python
# 2. Hierarchy — a root subtree, then children of it
call_tool(tool_name="add_sub_tree",     toolset_name=ST,  arguments={...})   # "Root"
call_tool(tool_name="add_child_state",  toolset_name=ST,  arguments={...})   # "Idle" under Root
call_tool(tool_name="add_child_state",  toolset_name=ST,  arguments={...})   # "Walking"
call_tool(tool_name="add_child_state",  toolset_name=ST,  arguments={...})   # "Attacking"

# 3. Tasks — struct tasks, or add_blueprint_task for an STT_* Blueprint
call_tool(tool_name="add_task",            toolset_name=ST, arguments={...})  # FStateTreeDelayTask
call_tool(tool_name="add_blueprint_task",  toolset_name=ST, arguments={...})  # STT_Rotate

# 4. Task property values — the returned keys of get_node_instance_data ARE the property names
call_tool(tool_name="get_node_instance_data", toolset_name=ST, arguments={...})
call_tool(tool_name="set_node_instance_data", toolset_name=ST, arguments={...})

# 5. Transitions — append ONE element to the state's Transitions, trigger + type + target
#    state together in the SAME write (Rule 1). Never rewrite the array (Rule 2).
call_tool(tool_name="SetProperties", toolset_name=OBJ, arguments={...})

# 6. Compile, then save — Epic's writes dirty the package but do not compile (gotcha 6)
call_tool(tool_name="compile",   toolset_name=ST,    arguments={...})
call_tool(tool_name="SaveAsset", toolset_name=ASSET, arguments={...})
```

**Step 7 — parameters and bindings (VibeUE, `execute_python`).**

```python
unreal.StateTreeService.add_or_update_root_parameter(
    "/Game/AI/MyBehavior", "IdlingTime", "Float", "2.0")

unreal.StateTreeService.bind_task_property_to_root_parameter(
    "/Game/AI/MyBehavior", "Root/Idle", "FStateTreeDelayTask", "Duration", "IdlingTime")

# 8. Show the user the state you just edited (the asset must already be open in an editor tab)
unreal.StateTreeService.set_state_expanded("/Game/AI/MyBehavior", "Root", True)
unreal.StateTreeService.select_state("/Game/AI/MyBehavior", "Root/Walking")
```

A batch of Epic edits is **not** one undo step (gotcha 1) — wrap it in your own
`unreal.ScopedEditorTransaction` if the user should be able to undo the build in one go.

## Sub-docs available

This skill's larger reference material has been split into sibling files. Load them via the engine
`AgentSkillToolset` (`GetSkills` with `["state-trees/<name>"]`, see the `vibeue` skill) when you need the detail:

- **`api-reference`** — the real 5.9 inventory: all 45 surviving `StateTreeService` tools with their signatures, Epic's `StateTreeToolset` tools grouped by job, the `ObjectToolset` property-key table for `UStateTreeState`, and the removed → replacement table.
- **`api-bindings`** — the 24 `bind_*` / `unbind_*` tools and the workflows around them: evaluators & global tasks, the full `OnDelegate` transition workflow, the context actor class two-call dance, bindings from context / root parameters / global tasks / evaluators, reading bindings back (engine-native `StateTreeEditorData.get_bindings`), assigning a StateTree to a `StateTreeComponent` (use `StateTreeRef` not `StateTree`), theme colors and expand/collapse.
- **`event-payloads`** — Sending a StateTree event with a struct payload from a Blueprint: the `Make <FMyStruct>` → `Make Instanced Struct` → `Make State Tree Event` → `Send State Tree Event` chain, `build_graph` pattern, pin names to verify, and common mistakes.
- **`changelog`** — **historical record only.** It documents VibeUE's Phase 1 & 2 development, much of it about tools the `5-9` branch removed. Read it for history; this file is the authority on what exists.
- **`blueprint-tasks`** — Creating & editing `STT_*` StateTree Blueprint Tasks: discovery workflow for the parent class, creating via the engine `BlueprintFactory`, correct event functions (`ReceiveLatentTick`, `ReceiveLatentEnterState`, `ReceiveExitState`, `ReceiveStateCompleted`), finding the registered `_C` task type name, and which Epic `add_blueprint_*` tool to use for each `STT_*` node kind.

## COMMON_MISTAKES

### ⚠️ "Color" Means Theme Color, Not Materials

When a user asks to rename, change, or list "colors" on a StateTree, they mean **theme colors** —
the editor-only color labels in the StateTree's global color table. Do NOT load the materials skill
or look for material parameters. Use `get_theme_colors`, `set_state_theme_color`, and `rename_theme_color`.

### ⚠️ Blueprint Nodes — Use the `add_blueprint_*` Tool, Not the Struct One

Epic ships a dedicated Blueprint variant for every node kind. For an `STT_*` / `STE_*` / `STC_*`
Blueprint node use `add_blueprint_task`, `add_blueprint_global_task`, `add_blueprint_evaluator`,
`add_blueprint_enter_condition` or `add_blueprint_consideration` — **not** the struct variant
(`add_task`, `add_global_task`, …), which takes a `UScriptStruct`. Never pass
`"StateTreeBlueprintTaskWrapper"` anywhere: that is the internal wrapper struct, not the Blueprint
class you want. Run `describe_toolset` for what each `add_blueprint_*` expects.

### ⚠️ Blueprint Task Names in VibeUE's Binding Tools

Blueprint tasks are stored internally as `StateTreeBlueprintTaskWrapper`. VibeUE's surviving tools
that take a `task_struct_name` — `bind_task_property_to_context` and the rest of the `bind_*` /
`unbind_*` family, `set_task_enabled`, `set_task_considered_for_completion` — accept **any** of
these forms, and they all resolve to the same node:

- `"STT_Rotate_C"` — the Blueprint generated class name
- `"STT_Rotate"` — the Blueprint name without `_C` suffix
- `"STT Rotate"` — the display name shown in the editor
- `"StateTreeBlueprintTaskWrapper"` — the raw struct type

To read a node's display text, use Epic's `get_node_description`.

### ⚠️ Binding Fails When Context Actor Class Is Not Set

`bind_task_property_to_context` returns `False` when the StateTree has no context actor class.
The error message will say "Context 'Actor' not found — this StateTree has NO context actor class set."

`set_context_actor_class` is gone. Setting it is now the **two-call** `ObjectToolset` dance from
gotcha (4):

1. `ObjectToolset.GetProperties` on the **editor data** for `Schema` → take the schema subobject's
   `refPath`. This read is also the diagnosis: it tells you whether a context actor class is set.
2. `ObjectToolset.SetProperties` on **that schema object** with `ContextActorClass`.

A nested `Schema.ContextActorClass` key on the editor data silently does not work — the schema's
`PostEditChangeChainProperty` never fires and `ContextDataDescs` never rebuilds, so bindings keep
failing.

```python
# Only AFTER the schema object actually carries a ContextActorClass does this succeed
unreal.StateTreeService.bind_task_property_to_context(
    st_path, "Root", "STT_Rotate_C", "Cube", "Actor", "")
```

### ⚠️ Forgetting to Compile

Every structural change (add state, add task, add transition) requires recompilation, and Epic's
writes only mark the package dirty. Always Epic `compile`, then `AssetToolset.SaveAsset`.

```python
# WRONG — structure written, never compiled, then saved
call_tool(tool_name="add_child_state", toolset_name=ST,    arguments={...})
call_tool(tool_name="SaveAsset",       toolset_name=ASSET, arguments={...})  # saves an uncompiled tree

# CORRECT
call_tool(tool_name="add_child_state", toolset_name=ST,    arguments={...})
call_tool(tool_name="compile",         toolset_name=ST,    arguments={...})  # check the result first
call_tool(tool_name="SaveAsset",       toolset_name=ASSET, arguments={...})
```

### ⚠️ Root Subtree vs Child State — Two Different Tools

A tree needs a root subtree before it can have children, and the two jobs are separate Epic tools:

- `add_sub_tree` — a new **top-level subtree** (what "Root" is). Same split for `move_sub_tree` /
  `remove_sub_tree`.
- `add_child_state` — a child **of an existing state**. Same split for `move_child_state` /
  `remove_child_state`.

Reaching for `add_child_state` on an empty tree, or `add_sub_tree` when the user asked for a child
of Root, are the two ways to get this wrong.

### ⚠️ A GotoState Transition Needs Its Target in the Same Write

This is Rule 1 above, and it is the single easiest way to corrupt a transition. Appending a
transition element whose `State.ID` is invalid does not just leave the target empty — the engine
overwrites your `Trigger` with `OnStateCompleted` and retargets the transition at the root state.
Pass trigger, type and target state in one `ObjectToolset.SetProperties`.

### ⚠️ NEVER Use remove + add to "Move" a State

**This is destructive and can silently drop the original state's identity and editor data.**

When asked to move a state under a different parent, use Epic's `move_child_state` (or
`move_sub_tree` for a top-level subtree). These reparent the existing `UStateTreeState` in-place,
preserving its children, tasks, transitions, bindings and per-state metadata.

`remove_child_state` followed by `add_child_state` creates a *different* state object. Any data
attached to the original can be lost or detached from the new copy.

```python
# WRONG — destroys the original state object and recreates a lookalike
call_tool(tool_name="remove_child_state", toolset_name=ST, arguments={...})
call_tool(tool_name="add_child_state",    toolset_name=ST, arguments={...})

# CORRECT — reparent in-place
call_tool(tool_name="move_child_state",   toolset_name=ST, arguments={...})
```

If the move fails, stop and inspect the tree. Do NOT fall back to remove+add as a workaround.

### ⚠️ NEVER Use remove + add to "Update" a Transition

**This is destructive and silently deletes all conditions on the transition.**

To change a transition's target, trigger or type, do what `UpdateTransition` used to do: one
`ObjectToolset.SetProperties` on `Transitions` **with that element's fields changed**. The element
keeps its `ID`, so its conditions and every binding keyed on that ID survive.

Dropping the element and appending a new one gives you a fresh transition with **no conditions** —
any `StateTreeObjectIsValidCondition` you had wired up is permanently lost — and a brand-new `ID`.

```python
# WRONG — remove the element, append a new one: conditions and bindings gone
call_tool(tool_name="SetProperties", toolset_name=OBJ, arguments={...})  # drop Transitions[0]
call_tool(tool_name="SetProperties", toolset_name=OBJ, arguments={...})  # append a replacement

# CORRECT — change the existing element's fields in place
call_tool(tool_name="SetProperties", toolset_name=OBJ, arguments={...})  # Transitions[0].State = ...
```

And never rewrite the whole array to do it — that is Rule 2, and it orphans every binding on the
state. To **reorder**, use VibeUE's `move_transition`.

### ⚠️ Check the Existing Transitions Before Appending One

VibeUE's `add_transition` used to refuse a duplicate (same trigger, type and target) and return
`False`. Nothing on the engine path promises you that, so a blind retry after a failed compile just
leaves you with two transitions.

Read the state's `Transitions` with `ObjectToolset.GetProperties` first, decide, then write once. If
an element is wrong, change *that element's* fields rather than appending another one.

### ⚠️ Use `StateTreeRef` Not `StateTree` on StateTreeComponent

`StateTreeComponent` has two related properties. The editor Details panel reads `StateTreeRef`.
Setting `StateTree` silently succeeds but the value does not appear in the editor.

`BlueprintService.set_component_property` was removed; the component template is reached through
the engine. Resolve it, then write the property with `ObjectToolset.SetProperties`:

```
# 1. The Blueprint's CDO, then its components (GetComponents switches to SubobjectDataSubsystem
#    for a Blueprint actor, so the SCS template really is enumerated).
call_tool GetDefaultObject  {"blueprint": {"refPath": "<BP path>"}}   # BlueprintToolsets.BlueprintToolset
call_tool GetComponents     {"actor": {"refPath": "<the CDO refPath>"}}   # EditorToolset.ActorToolset

# 2. Write StateTreeRef -- NOT StateTree -- on that component.
#    Run GetPropertySchema first: StateTreeRef is an FStateTreeReference struct, not a bare path.
call_tool GetPropertySchema {"instance": {"refPath": "<component refPath>"}, "property": "StateTreeRef"}
call_tool SetProperties     {"instance": {"refPath": "<component refPath>"}, "values": "{...}"}
```

The rule is unchanged: write `StateTreeRef`, never `StateTree`. Writing `StateTree` still
"succeeds" and still leaves the Details panel showing None.

### ⚠️ Task Struct Names Include "F" Prefix

Epic's `add_task` takes the task's `UScriptStruct`, and the struct name carries its `F`. Nothing in
the engine surface enumerates node structs, which is exactly why VibeUE kept the discovery tools —
read the name from `get_available_task_types()` instead of typing it from memory.

```python
# VibeUE (execute_python) — the authoritative list of what add_task will accept
types = unreal.StateTreeService.get_available_task_types()
# → "FStateTreeDelayTask", not "StateTreeDelayTask"; also "STT_Rotate_C" for Blueprint tasks
```

`get_available_condition_types`, `get_available_consideration_types` and
`get_available_evaluator_types` do the same for the other node kinds.

### ⚠️ Never Guess Node Property Names or Value Formats

A write with a wrong property name or value format does not do what you meant. The property names
are not something to guess — **the keys `get_node_instance_data` returns _are_ the property names**:

1. Epic `get_node_instance_data` on the node → the exact keys and their current values.
2. Epic `set_node_instance_data` with those keys.
3. Read it back with `get_node_instance_data` before compiling.

```python
# WRONG — inventing a key ("Color" when the real property is "TextColor") and never checking
call_tool(tool_name="set_node_instance_data", toolset_name=ST, arguments={...})

# CORRECT — read the keys first, write, read back
call_tool(tool_name="get_node_instance_data", toolset_name=ST, arguments={...})
call_tool(tool_name="set_node_instance_data", toolset_name=ST, arguments={...})
call_tool(tool_name="get_node_instance_data", toolset_name=ST, arguments={...})
```

Pick the right pair for what you are addressing: `get/set_node_instance_data` for the instance
struct (the values in the details panel — this is the default), `get/set_node_template_data` for the
node's template struct, `get/set_node_execution_runtime_data` for execution-time data.
`get_node_instance_object` returns the UObject form for Blueprint-based nodes.

### ⚠️ Condition Properties That Require Bindings (e.g. "Object")

Conditions like `StateTreeObjectIsValidCondition` have properties that **must be bound**
to context data or event payload — setting a string value won't work. Use
`bind_transition_condition_property_to_context`, `bind_enter_condition_property_to_context`, or
`bind_transition_condition_property_to_event_payload` rather than writing a value
(`set_transition_condition_property_value`, or Epic's `set_node_instance_data` on an enter
condition). All the `bind_*` tools below are VibeUE survivors.

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
# C++ field:     bIsBound  → Python: p.is_bound
# C++ field:     bEnabled  → Python: enabled
# C++ field:     bExpanded → Python: expanded

for p in unreal.StateTreeService.get_transition_condition_property_names(
        path, "Root", 0, "StateTreeObjectIsValidCondition"):
    print(p.name, p.is_bound, p.bound_to)   # NOT p.is_bound or p.bIsBound
```

### ⚠️ Use `ReceiveLatentTick`, Not `ReceiveTick` on StateTree Tasks

`ReceiveTick` is **deprecated** on `StateTreeTaskBlueprintBase`. Using it causes compile errors:
> `Cannot override 'StateTreeTaskBlueprintBase::ReceiveTick' — declared with a different signature`

Events are `BlueprintToolsets.BlueprintToolset.AddEvent` now (`add_event_node` was removed). The
name is what matters:

```
# WRONG — deprecated, will fail to compile
call_tool AddEvent {"blueprint": {"refPath": "/Game/StateTree/STT_MyTask"}, "eventName": "ReceiveTick"}

# CORRECT — the latent Tick, no return value
call_tool AddEvent {"blueprint": {"refPath": "/Game/StateTree/STT_MyTask"}, "eventName": "ReceiveLatentTick"}

# WRONG — deprecated Enter State
call_tool AddEvent {"blueprint": {"refPath": "/Game/StateTree/STT_MyTask"}, "eventName": "ReceiveEnterState"}

# CORRECT — the latent Enter State
call_tool AddEvent {"blueprint": {"refPath": "/Game/StateTree/STT_MyTask"}, "eventName": "ReceiveLatentEnterState"}
```

`ListCompatibleEventFunctions` on the same toolset lists what the Blueprint will actually accept.

## Sample scripts (run via `execute_python`)

- **`scripts/build_state_tree.txt`** — the full mixed build: VibeUE `create_state_tree`, Epic
  `add_sub_tree` / `add_child_state` / `add_task`, a transition appended in one
  `ObjectToolset.SetProperties` (Rule 1), Epic `compile` + `AssetToolset.SaveAsset`, then a VibeUE
  root parameter and binding. Only the VibeUE parts run under `execute_python`; the Epic parts are
  `call_tool`.

## Additional gotchas

- A `ConstructorHelpers::FClassFinder` on a Blueprint that owns a StateTree force-loads the tree before the StateTree module registers; `IsReadyToRun()` then stays false all session and every instance is brain-dead while looking healthy. Use a `TSoftClassPtr` plus `LoadSynchronous()` at BeginPlay instead.

## Reference pages

Open these with the paths below; UE rewrites them to absolute paths when it serves this skill.

- [api-bindings](api-bindings.md) - The 24 surviving binding tools and the workflows around them - evaluators and global tasks, transitions and the OnDelegate workflow, the two-call c...
- [api-reference](api-reference.md) - The real 5.9 inventory - all 45 surviving VibeUE tools with signatures, Epic's StateTreeToolset tools by job, the ObjectToolset keys on UStateTreeS...
- [blueprint-tasks](blueprint-tasks.md) - Create and edit STT_* StateTree Blueprint Tasks - discover parent class, create via the engine BlueprintFactory, ReceiveLatentTick / ReceiveLatentEnte...
- [changelog](changelog.md) - HISTORICAL record of VibeUE StateTree development - the 5-9 branch removed 53 of the tools described there; this file is the authority on what exists...
- [event-payloads](event-payloads.md) - Send a StateTree event with a struct payload from a Blueprint (Make Struct -> Make Instanced Struct -> Make State Tree Event -> Send State Tree Event)
- [scripts/build_state_tree.txt](scripts/build_state_tree.txt) - runnable sample

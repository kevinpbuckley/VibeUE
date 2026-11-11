---
name: api-reference
description: The real 5.9 StateTree inventory - all 45 surviving VibeUE StateTreeService tools with signatures, Epic's StateTreeToolset tools grouped by job, the ObjectToolset property keys on UStateTreeState, and the removed-to-replacement table
---

This sub-doc is the inventory. `SKILL.md` carries the ownership table, the two transition rules and
the gotchas — read those first. For the binding workflows behind the `bind_*` signatures here, see
`api-bindings.md`.

**Two call styles, and they are not interchangeable:**

| Who | `toolset_name` | How | Names |
|---|---|---|---|
| Epic `StateTreeToolset` (a Python toolset) | `state_tree_toolset.state_tree.StateTreeToolset` | `call_tool` only | snake_case — `add_child_state`, `compile` |
| Epic `ObjectToolset` | `EditorToolset.ObjectToolset` | `call_tool` only | PascalCase — `GetProperties`, `SetProperties` |
| Epic `AssetToolset` | `EditorToolset.AssetToolset` | `call_tool` only | PascalCase — `FindAssets`, `SaveAsset` |
| VibeUE's 45 survivors | — | `unreal.StateTreeService.<snake_case>()` in `execute_python` | `create_state_tree`, `bind_task_property_to_context` |

There is no `unreal.StateTreeToolset`. The tool names here are verified; **parameter names come from
`describe_toolset`**, never from guesswork. `SKILL.md` has the rule for deriving any other toolset's
registered name (C++ `<Module>.<ClassName>`, Python `<package>.<module>.<ClassName>`).

# Part 1 — VibeUE's 45 surviving tools

Every signature below is the real one. Defaults are shown; a `match_index` of `-1` means "the last
matching node".

## Asset creation (1)

Epic deliberately ships no asset creation — its own source says there is no K2 path for creating a
StateTree with a schema in head (`UStateTreeFactory::SetSchemaClass` is not a `UFUNCTION`), so
clients are expected to use the Content Browser. VibeUE's tool is the scriptable path.

```text
unreal.StateTreeService.create_state_tree(asset_path, schema_class_name="StateTreeComponentSchema") -> bool
```

If it returns `False` the schema is usually the problem, not the path. List what this project really
has with `ObjectToolset.SearchSubclasses` on `UStateTreeSchema` and pass one of those.

## Node-type discovery (4)

Epic's adders take a `UScriptStruct`, and **nothing in the engine surface enumerates `UScriptStruct`
node types** — `ObjectToolset.SearchSubclasses` is `UClass`-only. These four are how you find out
what `add_task` / `add_enter_condition` / `add_consideration` / `add_evaluator` will accept.

```text
unreal.StateTreeService.get_available_task_types()          -> [str]  # "FStateTreeDelayTask", "STT_Rotate_C", ...
unreal.StateTreeService.get_available_condition_types()     -> [str]
unreal.StateTreeService.get_available_consideration_types() -> [str]  # Constant, FloatInput, EnumInput, + custom
unreal.StateTreeService.get_available_evaluator_types()     -> [str]
```

## Theme colors (3)

`FStateTreeEditorColor::ExportTextItem` returns false unless `PPF_Copy|PPF_Duplicate`, so Epic's
property reads hand back empty strings for colors; and a state's `ColorRef` is only a GUID
reference, unusable without `get_theme_colors` to resolve it.

```text
unreal.StateTreeService.get_theme_colors(asset_path) -> [FStateTreeThemeColorInfo]
unreal.StateTreeService.set_state_theme_color(asset_path, state_path, color_name, color)  # creates the entry if needed
unreal.StateTreeService.rename_theme_color(asset_path, old_color_name, new_color_name)    # preserves the ColorRef UUID
```

`FStateTreeThemeColorInfo`: `.display_name`, `.color` (`FLinearColor`), `.used_by_states`
(list of state paths) — **not** `.referencing_state_paths`.

## Root parameters (3)

`FInstancedPropertyBag`'s schema is C++-mutable only, so these cannot go through a property write.

```python
unreal.StateTreeService.add_or_update_root_parameter(asset_path, name, type, default_value="")
unreal.StateTreeService.remove_root_parameter(asset_path, name)
unreal.StateTreeService.rename_root_parameter(asset_path, old_name, new_name)
```

`type` is one of `"Bool"`, `"Int32"`, `"Int64"`, `"Float"`, `"Double"`, `"Name"`, `"String"`,
`"Text"`. To **read** parameters back, use `ObjectToolset.GetProperties` on the editor data with key
`RootParameters`.

## Per-instance parameter override on a placed actor (1)

Needs property-bag descriptor GUIDs nothing else exposes. The type is resolved from the linked
StateTree asset, so there is no type argument.

```python
unreal.StateTreeService.set_component_parameter_override(actor_name_or_label, parameter_name, value)
```

Save the level afterwards to persist it.

## Editor view (2)

```python
unreal.StateTreeService.select_state(asset_path, state_path)              # asset must already be open in a tab
unreal.StateTreeService.set_state_expanded(asset_path, state_path, expanded)
```

`select_state` survives because the engine's `SelectAssetEditorObjects` raises for
non-Blueprint/Widget editors. `set_state_expanded` survives because `bExpanded` is a bare
`UPROPERTY(meta = (ExcludeFromHash))` — no `CPF_Edit`, no `CPF_BlueprintVisible` — so the toolset
refuses it for read *and* write, and it is invisible even to `ListProperties`.

## Task flags the engine cannot path-address (2)

Both live inside an `FInstancedStruct` or behind an `Instanced` object reference.
`ToolsetPropertyPath` refuses to cross an object reference and `FInstancedStruct` has no reflected
fields, so there is no path to them at all.

```python
unreal.StateTreeService.set_task_enabled(asset_path, state_path, task_struct_name, task_match_index, enabled)
unreal.StateTreeService.set_task_considered_for_completion(asset_path, state_path, task_struct_name, task_match_index, considered_for_completion)
```

## Transitions and transition conditions (6)

`move_transition` survives because no toolset anywhere emits `EPropertyChangeType::ArrayMove` — see
Rule 2 in `SKILL.md`. The condition tools survive for the same `FInstancedStruct` reason as the task
flags; `add_transition_condition` additionally needs `InitializeAs` to allocate
`Instance`/`InstanceObject` with the right outer and mint the node ID.

```text
unreal.StateTreeService.move_transition(asset_path, state_path, from_index, to_index)

unreal.StateTreeService.add_transition_condition(asset_path, state_path, transition_index, condition_struct_name)

unreal.StateTreeService.get_transition_condition_property_names(
    asset_path, state_path, transition_index, condition_struct_name,
    condition_match_index=-1) -> [FStateTreePropertyInfo]

unreal.StateTreeService.set_transition_condition_property_value(
    asset_path, state_path, transition_index, condition_struct_name,
    property_path, value, condition_match_index=-1)

unreal.StateTreeService.get_transition_event_payload_property_names(
    asset_path, state_path, transition_index) -> [FStateTreePropertyInfo]
```

`bind_transition_to_delegate` (below) is the sixth.

`FStateTreePropertyInfo`: `.name` (case-sensitive C++ name), `.type`, `.current_value` (UE text
format), `.is_bound` (only the exact bound path is marked), `.bound_to`
(e.g. `"RootParameter:Parameters.IdlingTime"`; a trailing `" (output)"` means this property *writes*
the named source).

## The 24 `bind_*` / `unbind_*` tools

Their engine counterpart is `UStateTreeEditorData::SetBinding` / `ClearBinding`, which are
`PythonCallable` **native** APIs, not toolset tools — so they are not a toolset duplicate and they
all stay. Workflows and worked examples live in `api-bindings.md`.

**State task as target** — `(asset_path, state_path, task_struct_name, task_property_path, …, task_match_index=-1)`

| Tool | Trailing arguments |
|---|---|
| `bind_task_property_to_root_parameter` | `parameter_path` |
| `bind_task_property_to_context` | `context_name="Actor"`, `context_property_path=""` |
| `bind_task_property_to_global_task_property` | `global_task_struct_name`, `global_task_property_path`, then `global_task_match_index=-1` |
| `bind_task_property_to_evaluator_property` | `evaluator_struct_name`, `evaluator_property_path`, then `evaluator_match_index=-1` |
| `unbind_task_property` | — |

**Enter condition as target** — `(asset_path, state_path, condition_struct_name, condition_property_path, …, condition_match_index=-1)`

| Tool | Trailing arguments |
|---|---|
| `bind_enter_condition_property_to_root_parameter` | `parameter_path` |
| `bind_enter_condition_property_to_context` | `context_name="Actor"`, `context_property_path=""` |
| `bind_enter_condition_property_to_global_task_property` | `global_task_struct_name`, `global_task_property_path`, then `global_task_match_index=-1` |
| `bind_enter_condition_property_to_evaluator_property` | `evaluator_struct_name`, `evaluator_property_path`, then `evaluator_match_index=-1` |
| `unbind_enter_condition_property` | — |

**Transition condition as target** — `(asset_path, state_path, transition_index, condition_struct_name, condition_property_path, …, condition_match_index=-1)`

| Tool | Trailing arguments |
|---|---|
| `bind_transition_condition_property_to_context` | `context_name="Actor"`, `context_property_path=""` |
| `bind_transition_condition_property_to_event_payload` | `payload_property_path` (field name inside the payload struct) |
| `bind_transition_condition_property_to_evaluator_property` | `evaluator_struct_name`, `evaluator_property_path`, then `evaluator_match_index=-1` |
| `unbind_transition_condition_property` | — |

**Consideration as target** — `(asset_path, state_path, consideration_struct_name, consideration_property_path, …, match_index=-1)`

| Tool | Trailing arguments |
|---|---|
| `bind_consideration_property_to_root_parameter` | `parameter_path` |
| `bind_consideration_property_to_context` | `context_name="Actor"`, `context_property_path=""` |
| `unbind_consideration_property` | — |

**Evaluator as target** — `(asset_path, evaluator_struct_name, evaluator_property_path, …, match_index=-1)`; evaluators are global, so there is no `state_path`

| Tool | Trailing arguments |
|---|---|
| `bind_evaluator_property_to_root_parameter` | `parameter_path` |
| `bind_evaluator_property_to_context` | `context_name="Actor"`, `context_property_path=""` |
| `unbind_evaluator_property` | — |

**Global task as target** — `(asset_path, task_struct_name, task_property_path, …, match_index=-1)`; also global, no `state_path`

| Tool | Trailing arguments |
|---|---|
| `bind_global_task_property_to_root_parameter` | `parameter_path` |
| `bind_global_task_property_to_context` | `context_name="Actor"`, `context_property_path=""` |
| `unbind_global_task_property` | — |

**Transition → delegate dispatcher**

```python
unreal.StateTreeService.bind_transition_to_delegate(
    asset_path, state_path, transition_index, task_struct_name,
    dispatcher_property_name, task_match_index=-1)
```

Any `*_struct_name` argument accepts a Blueprint node by `_C` class name (`"STT_Rotate_C"`), plain
Blueprint name (`"STT_Rotate"`), editor display name (`"STT Rotate"`), or the raw wrapper struct.

# Part 2 — Epic's `StateTreeToolset` (`call_tool`, snake_case)

`toolset_name="state_tree_toolset.state_tree.StateTreeToolset"`. Enable the **StateTreeToolset**
plugin if `list_toolsets` does not show it. Run `describe_toolset` before relying on an exact
parameter list — what follows are the verified tool *names*, grouped by job.

| Job | Tools |
|---|---|
| Compile / refresh | `compile`, `request_ui_refresh` |
| Read the tree | `get_editor_data`, `get_root_states`, `get_all_states`, `get_children`, `get_node_description` |
| States | `add_sub_tree`, `add_child_state`, `move_sub_tree`, `move_child_state`, `remove_sub_tree`, `remove_child_state` |
| Tasks | `add_task`, `add_blueprint_task`, `move_task`, `remove_task` |
| Enter conditions | `add_enter_condition`, `add_blueprint_enter_condition`, `remove_enter_condition` |
| Considerations | `add_consideration`, `add_blueprint_consideration`, `remove_consideration` |
| Evaluators | `add_evaluator`, `add_blueprint_evaluator`, `remove_evaluator` |
| Global tasks | `add_global_task`, `add_blueprint_global_task`, `remove_global_task` |
| Node data | `get_node_instance_data` / `set_node_instance_data`, `get_node_template_data` / `set_node_template_data`, `get_node_execution_runtime_data` / `set_node_execution_runtime_data`, `get_node_instance_object`, `get_node_execution_runtime_object` |

Three node-data pairs, chosen by what you are addressing:

- **instance** (`get/set_node_instance_data`) — the per-node values shown in the details panel.
  This is the default, and **the keys it returns are the property names**.
- **template** (`get/set_node_template_data`) — the node's template struct
  (`FStateTreeEditorNode.Node`).
- **execution runtime** (`get/set_node_execution_runtime_data`) — execution-time data.

`get_node_instance_object` / `get_node_execution_runtime_object` return the UObject form for
Blueprint-based nodes.

For an `STT_*` / `STE_*` / `STC_*` **Blueprint** node always use the `add_blueprint_*` variant; the
plain variant takes a `UScriptStruct`.

Epic's tools do **not** open undo scopes of their own, and they do **not** compile. Wrap a batch in
your own `unreal.ScopedEditorTransaction` if it should undo as one action, and finish with `compile`
then `AssetToolset.SaveAsset`.

# Part 3 — Epic's `ObjectToolset` / `AssetToolset`

Both are C++ toolsets in the `EditorToolset` module, so:
`toolset_name="EditorToolset.ObjectToolset"` and `toolset_name="EditorToolset.AssetToolset"`.

| Job | Tool |
|---|---|
| Read any state / editor-data property | `ObjectToolset.GetProperties` |
| Write any state property, and all transition array edits | `ObjectToolset.SetProperties` |
| List a property surface | `ObjectToolset.ListProperties` |
| Which schema / node *classes* exist | `ObjectToolset.SearchSubclasses` (`UClass`-only) |
| Find StateTree assets | `AssetToolset.FindAssets`, filtered to class `StateTree` |
| Save (after `compile`) | `AssetToolset.SaveAsset` |

## `ObjectToolset` property keys on `UStateTreeState`

Each is a plain `EditDefaultsOnly` UPROPERTY on `UStateTreeState`, so `SetProperties` reaches it
(and `GetProperties` reads it). Exact keys, from `StateTreeState.h`:

| Property key | Declared type | Replaces the removed tool |
|---|---|---|
| `Name` | `FName` | `RenameState` |
| `Description` | `FString` | `SetStateDescription` |
| `Tag` | `FGameplayTag` | `SetStateTag` |
| `Type` | `EStateTreeStateType` | `SetStateType` |
| `SelectionBehavior` | `EStateTreeStateSelectionBehavior` | `SetSelectionBehavior` |
| `TasksCompletion` | `EStateTreeTaskCompletionType` | `SetTasksCompletion` |
| `LinkedSubtree` | `FStateTreeStateLink` | `SetLinkedSubtree` |
| `LinkedAsset` | `TObjectPtr<UStateTree>` | `SetLinkedAsset` |
| `bEnabled` | `bool` | `SetStateEnabled` |
| `Weight` | `float` | `SetStateWeight` |

Three things to remember when writing these:

- `Weight` **lost its clamp**. `ClampMin=0` is only metadata and the JSON import path does not
  enforce it, so a negative weight sticks. Clamp it yourself.
- `Type` **has side effects, by design** — it resets `Tasks` for a `Group`, forces
  `SelectionBehavior` to `TryEnterState` and clears `Tasks` for `Linked`/`LinkedAsset`, clears
  `LinkedSubtree`/`LinkedAsset` when leaving those types, and flips `Parameters.bFixedLayout`.
- The context actor class is **not** on the state. It takes two calls: `GetProperties` on the editor
  data for `Schema`, then `SetProperties` on **that schema object** with `ContextActorClass`.

Transitions are the state's `Transitions` array, edited with `SetProperties`. Before you touch it,
read Rule 1 (target state in the same write) and Rule 2 (never rewrite the array) in `SKILL.md`.

# Part 4 — Removed → replacement

Everything on the left **no longer exists**: 53 tools were cut on the `5-9` branch, 98 → 45.

## Group A — Epic `StateTreeToolset` (`call_tool`, snake_case)

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

## Group B — Epic `AssetToolset` / `ObjectToolset`

| Removed VibeUE tool | Replacement |
|---|---|
| `SaveStateTree` | `compile` first, then `AssetToolset.SaveAsset` |
| `ListStateTrees` | `AssetToolset.FindAssets` filtered to class `StateTree` |
| `ListStateTreeSchemas` | `ObjectToolset.SearchSubclasses` on `UStateTreeSchema` |
| `GetRootParameters` | `ObjectToolset.GetProperties` on the editor data, key `RootParameters` |
| `GetComponentParameterOverrides` | `ObjectToolset.GetProperties` on the StateTree component |
| `GetComponentStateTreePath` | `ObjectToolset.GetProperties` on the component, key `StateTreeRef` |

## Group C — `ObjectToolset.SetProperties` on `UStateTreeState`

The ten state properties in the key table in Part 3: `RenameState`, `SetStateDescription`,
`SetStateTag`, `SetStateType`, `SetSelectionBehavior`, `SetTasksCompletion`, `SetLinkedSubtree`,
`SetLinkedAsset`, `SetStateEnabled`, `SetStateWeight`. `SetContextActorClass` is the one that needs
**two** calls.

## Group D — `ObjectToolset` array operations

| Removed VibeUE tool | Replacement |
|---|---|
| `AddTransition` | `ObjectToolset.SetProperties` — append one element to `Transitions` (**Rule 1**) |
| `RemoveTransition` | `ObjectToolset.SetProperties` — drop one element from `Transitions` |
| `UpdateTransition` | `ObjectToolset.SetProperties` on `Transitions` with the element's fields changed |
| `RemoveTransitionCondition` | `ObjectToolset.SetProperties` — drop one element from that transition's `Conditions` |
| `SetEnterConditionOperand` | `ObjectToolset.SetProperties` on the enter condition element's `ConditionOperand` |
| `SetTransitionConditionOperand` | `ObjectToolset.SetProperties` on the transition condition element's `ConditionOperand` |

## Group E — in-plugin duplicate

| Removed VibeUE tool | Replacement |
|---|---|
| `AddOrUpdateRootFloatParameter` | `StateTreeService.AddOrUpdateRootParameter` (still a VibeUE tool) |

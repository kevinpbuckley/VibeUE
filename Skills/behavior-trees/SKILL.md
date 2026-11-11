---
name: behavior-trees
display_name: Behavior Trees, Blackboards & EQS
description: Author Behavior Trees, Blackboards and Environment Queries with Unreal 5.9's own AIModuleToolset (composites, tasks, decorators, services, blackboard keys, EQS options and tests), plus the five capabilities the engine lacks that VibeUE still provides (BT graph repair, blackboard key rename and reference sweep, EQS run and EQS graph repair). Use when the user asks to build a Behavior Tree, add BT nodes or decorators, bind blackboard keys, author a blackboard, rename a blackboard key, or repair a BT whose graph shows up empty.
vibeue_classes:
  - BehaviorTreeService
  - BlackboardService
  - EnvQueryService
unreal_classes:
  - UBehaviorTree
  - UBlackboardData
  - UBTNode
  - UBTCompositeNode
  - UBTTaskNode
  - UBTDecorator
keywords:
  - behavior tree
  - blackboard
  - BT task
  - BT decorator
  - BT service
  - blackboard key
  - AI behavior
  - repair behavior tree
  - environment query
  - EQS
---

# Behavior Trees & Blackboards

**The engine owns this domain.** UE 5.9 ships `AIModuleToolset.BehaviorTreeToolset` (25 tools) for
Behavior Tree and Blackboard authoring. Use it via `call_tool`. VibeUE keeps only three things it
cannot do.

Enable the **AIModuleToolset** plugin if `list_toolsets` does not show it.

## Use the engine for all authoring

| Want | `AIModuleToolset.BehaviorTreeToolset` tool |
|---|---|
| Create the BT asset | `CreateBehaviorTree` |
| Create a Blackboard asset | `CreateBlackboardData` |
| Root composite / root decorator | `AddRootComposite`, `AddRootDecorator` |
| Add a composite (Selector, Sequence, Simple Parallel) | `AddComposite` |
| Add a task | `AddTask` |
| Add a decorator to a node | `AddDecorator` |
| Add a service to a composite | `AddService` |
| Read the tree | `GetRootGraphNode`, `GetChildGraphNodes`, `GetSubtree` |
| Read a node's decorators / services | `GetDecoratorGraphNodes`, `GetServiceGraphNodes` |
| Get the runtime object behind a graph node | `GetNodeInstance` |
| Reorder / move nodes and subnodes | `MoveGraphNode`, `MoveSubnode` |
| Remove nodes and subnodes | `RemoveGraphNode`, `RemoveSubnode` |
| Assign / read the tree's blackboard | `SetBlackboard`, `GetBlackboard` |
| Blackboard key CRUD | `AddBlackboardKey`, `RemoveBlackboardKey` |
| List keys / keys a node may use | `GetBlackboardKeyNames`, `GetAllowedBlackboardKeys` |
| Point a node's key selector at a key | `SetNodeBlackboardKey` |

| Want | Other engine toolset |
|---|---|
| Any node property — name, comment, wait time, any field | `EditorToolset.ObjectToolset` → `GetProperties` / `SetProperties` / `ListProperties` on the object from `GetNodeInstance` |
| Which task/decorator/service classes exist | `ObjectToolset.SearchSubclasses`, or `search_symbols` / `get_symbol` |
| Find BT or Blackboard assets | `EditorToolset.AssetToolset` → `FindAssets` |
| **Save** (this is also what compiles the tree) | `AssetToolset.SaveAsset` |
| Validate an asset | `AssetToolset.ValidateAssets` |
| Blackboard key *type*, parent board, instance-synced flag, metadata | `ObjectToolset.SetProperties` on the board / its `FBlackboardEntry` |

> **Saving is the compile step.** Epic's BT tools only mark the package dirty. Nothing in the engine
> surface explicitly compiles a Behavior Tree, so finish an authoring session with
> `AssetToolset.SaveAsset` — that is what turns the editor graph into the runtime tree. VibeUE's old
> `compile_and_save` was a convenience over exactly this and is gone.

## What VibeUE still provides (three tools)

Everything else on `BehaviorTreeService` and `BlackboardService` was cut on the 5-9 branch because
the engine covers it.

### `BehaviorTreeService.repair_graph_from_runtime_tree(asset_path)`

For a BT whose **editor graph is empty or holds only its root while the runtime tree is intact**.
Such an asset opens blank in the Behavior Tree editor, and an ordinary write to it is refused —
committing that graph would overwrite the runtime tree with an empty one. Epic's tools edit the graph
they find, so they cannot recover this; they would persist the damage.

```python
import unreal
err = unreal.BehaviorTreeService.repair_graph_from_runtime_tree("/Game/AI/BT_Zombie")
print(err or "repaired and saved")
```

It is **explicit and never implicit** — nothing else calls it, because rebuilding a production
asset's graph as a side effect of an unrelated edit is the kind of unrequested write this exists to
prevent. It refuses when there is nothing to repair (no runtime tree, or a graph that already has
nodes under its root) and logs the before/after node counts.

### `BlackboardService.rename_blackboard_key(asset_path, old, new)`

Epic has eight rename tools but none for a blackboard key, and a rename has to fix up every BT node
selector bound to the old name. Returns a plain empty array on an unreferenced success; a refusal is
a **single entry prefixed `"ERROR: "`** — a shape a real reference can never take, since a real one
is always `"<btPath>:<node>.<property>"`. It refuses an inherited key, a missing key, and a
collision with an existing name.

### `BlackboardService.find_blackboard_key_references(asset_path, key_name)`

Read-only: every BT node key selector bound to that key, across every Behavior Tree whose blackboard
is this asset *or inherits from it*, formatted `"<btPath>:<node>.<property>"`. This is the question
to ask **before** deciding to rename or remove a key. Same `"ERROR: "` convention on failure.

```python
import unreal
refs = unreal.BlackboardService.find_blackboard_key_references("/Game/AI/BB_Zombie", "TargetActor")
for r in refs:
    print(r)   # /Game/AI/BT_Zombie:Chase.BlackboardKey
```

## Typical flow

1. `CreateBehaviorTree` + `CreateBlackboardData`, then `SetBlackboard` (engine).
2. `AddBlackboardKey` for each key (engine).
3. `AddRootComposite`, then `AddComposite` / `AddTask` / `AddDecorator` / `AddService` (engine).
4. `GetNodeInstance` → `ObjectToolset.SetProperties` for node fields; `SetNodeBlackboardKey` for
   key selectors (engine).
5. `AssetToolset.SaveAsset` — this compiles the tree (engine).
6. If the asset ever opens blank, `repair_graph_from_runtime_tree` (VibeUE).
7. Before renaming or deleting a key, `find_blackboard_key_references`, then
   `rename_blackboard_key` (VibeUE).


## Environment Queries (EQS) — same split

UE 5.9's `AIModuleToolset.EnvironmentQueryToolset` (14 tools) owns EQS authoring:

| Want | Tool |
|---|---|
| Create the query asset | `CreateEnvironmentQuery` |
| Add an option (**takes the generator class**, so there is no separate set-generator step) | `AddOption` |
| Add / remove / move tests | `AddTest`, `RemoveTest`, `MoveTest` |
| Remove / reorder options | `RemoveOption`, `MoveOption` |
| Enable or disable a test | `SetTestEnabled` |
| Read the graph | `GetRootGraphNode`, `GetOptionGraphNodes`, `GetTestGraphNodes` |
| Get the objects behind nodes | `GetOptionInstance`, `GetTestInstance`, `GetGenerator` |
| Any generator/test property (scoring, distance, filters) | `EditorToolset.ObjectToolset` → `SetProperties` on the instance |
| Available generator / test / context classes | `ObjectToolset.SearchSubclasses` or `search_symbols` |
| Find, save, validate | `EditorToolset.AssetToolset` |

VibeUE keeps the two things that surface has no tool for:

### `EnvQueryService.run_query(asset_path, querier_actor, run_mode)`

Epic's EQS toolset is authoring and inspection only — **nothing in the engine runs a query**. This
executes one against a querier actor and returns the ranked items with their scores.

```python
import unreal
res = unreal.EnvQueryService.run_query("/Game/AI/EQS_Cover", "BP_Enemy_1", "AllMatching")
if res.success:
    for item in res.items:
        print(item.description, item.score)
else:
    print(res.error)
```

Run modes are `SingleBestItem`, `SingleRandomItemFromBestFive`, `AllMatching` — an unrecognised mode
is **refused and names the valid set**, never defaulted. It requires a **running PIE session** (it
needs an AI system and a real querier); headless it refuses saying so.

### `EnvQueryService.repair_graph_from_options(asset_path)`

The EQS counterpart of the BT graph repair: rebuilds option and test nodes from `UEnvQuery::Options`
for a sparse graph, lays out, commits and saves. Empty string on success, else the reason.

## Gotchas

- A BT edited only through tool calls is not compiled until it is **saved**. Save before testing.
- `rename_blackboard_key` and `find_blackboard_key_references` return `["ERROR: ..."]` rather than
  raising. Check the first entry before treating the array as references.
- A key that lives on a **parent** blackboard cannot be renamed through the child — the rename is
  refused. Edit the parent asset.
- Running a Behavior Tree is gameplay: start PIE with `EditorAppToolset.StartPIE` and inspect the
  live actor from `execute_python`.

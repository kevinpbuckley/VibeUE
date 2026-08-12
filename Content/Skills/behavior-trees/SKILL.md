---
name: behavior-trees
display_name: Behavior Trees & Blackboards
description: Create, inspect, and edit Behavior Tree and Blackboard assets — composites, tasks, decorators, services, node properties, blackboard key bindings, key CRUD and metadata, tree round-trips and validation (BehaviorTreeService + BlackboardService). Use when the user asks to build a Behavior Tree, add BT nodes or decorators, bind blackboard keys, author a blackboard, or validate/repair a BT asset.
vibeue_classes:
  - BehaviorTreeService
  - BlackboardService
unreal_classes:
  - UBehaviorTree
  - UBlackboardData
  - UBTNode
  - UBTCompositeNode
  - UBTTaskNode
  - UBTDecorator
  - UBTService
keywords:
  - behavior tree
  - behaviour tree
  - blackboard
  - bt
  - btt
  - btd
  - bts
  - composite
  - selector
  - sequence
  - simple parallel
  - decorator
  - service
  - task
  - blackboard key
  - key selector
  - ai
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this
> session, call it with `{action: "load", skill: "ai-and-navigation"}` for UE domain knowledge on
> Behavior Trees — architecture, node semantics, best practices. If no such tool is available, skip
> this line entirely and proceed with this skill alone — do NOT attempt the call.

# Behavior Trees Skill

Two services, reachable from Python (`unreal.BehaviorTreeService.*`, `unreal.BlackboardService.*`)
and as AICallable tools:

```
BlackboardService     → UBlackboardData assets: create, key CRUD, metadata, re-parenting
BehaviorTreeService   → UBehaviorTree assets: structure, sub-nodes, properties, build, validate
```

Discover exact signatures with `discover_python_class('unreal.BehaviorTreeService')` — this skill
teaches the concepts and the gotchas, not the call shapes.

## The golden workflow

```python
import unreal
BT = unreal.BehaviorTreeService
BB = unreal.BlackboardService

# 1. Blackboard first — key bindings resolve against the TREE'S board, so assign it at creation.
BB.create_blackboard("/Game/AI/BB_Guard")
BB.add_blackboard_key("/Game/AI/BB_Guard", "TargetActor", "Object")
BB.set_blackboard_key_object_class("/Game/AI/BB_Guard", "TargetActor", "/Script/Engine.Actor")

# 2. Tree on that board.
BT.create_behavior_tree("/Game/AI/BT_Guard", "/Game/AI/BB_Guard")

# 3. Structure. Every mutator returns the new node's PATH (or "ERROR: ...").
sel = BT.add_node("/Game/AI/BT_Guard", "Root", "BTComposite_Selector")          # Root/Selector[0]
seq = BT.add_node("/Game/AI/BT_Guard", sel, "BTComposite_Sequence")
wait = BT.add_node("/Game/AI/BT_Guard", seq, "BTTask_Wait")

# 4. Properties and bindings.
BT.set_node_property_value("/Game/AI/BT_Guard", wait, "WaitTime", '(DefaultValue=3.5,Key="")')
dec = BT.add_decorator("/Game/AI/BT_Guard", seq, "BTDecorator_Blackboard")
BT.set_node_blackboard_key("/Game/AI/BT_Guard", dec, "BlackboardKey", "TargetActor")

# 5. Verify.
print(BT.validate_tree("/Game/AI/BT_Guard"))   # [] == clean
print(BT.get_tree("/Game/AI/BT_Guard"))        # whole tree as JSON
```

Every mutator saves the asset itself — there is no separate "commit" step to forget.
`compile_and_save` exists for re-running layout + save on a tree edited some other way.

## Node paths — the addressing scheme

- `Root/Selector[0]/Sequence[1]/Wait[0]` — segments are node TITLES with an index among
  same-named siblings. `Root` is a keyword (the graph root), not a title.
- Sub-nodes: `<owner path>/@decorator[n]` / `<owner path>/@service[n]` — by slot, in order.
- **Paths are positional, not identities.** Inserting/removing a sibling renumbers later ones;
  renaming a node changes its segment. Hold the node's `guid` (from GetTree/GetNodeInfo) across
  edits, and re-read GetTree after a rename. `set_node_name` refuses names the grammar cannot
  express (`/`, leading `@`, trailing `[n]`) — on both the direct route and via the `NodeName`
  property.

## Things that are not obvious and will bite you

- **WaitTime and friends are structs.** In UE 5.8 many numeric BT properties are
  `FValueOrBBKey_*`: pass `'(DefaultValue=3.5,Key="")'`, not `'3.5'`. Read the current value
  first (`get_node_property_value`) and mirror its shape.
- **Blackboard key selectors are not values.** Always `set_node_blackboard_key`, never
  `set_node_property_value`, for anything `get_node_property_names` flags
  `bIsBlackboardKeySelector`. The service validates the key's TYPE against the selector's filter
  up front — a mistyped binding would otherwise resolve, read back fine, and be silently cleared
  by the engine on the next save.
- **SimpleParallel has two fixed slots**, not a child list: slot 0 = main task (tasks only),
  slot 1 = background branch. The engine refills an empty background slot with a Wait task on
  every save, so the background branch can only ever be REPLACED (`add_node(parallel, cls, 1)`),
  never removed or moved away.
- **Root-level decorators go on the top composite** (the node under Root), not on `Root` itself —
  `add_decorator(asset, "Root/Selector", ...)` is the correct call and the refusal message on
  `Root` says so.
- **`set_node_property_value` verifies after the save.** `ValueAfterWrite` is re-read from the
  asset, not echoed; if the engine rewrote the value at save time the call reports failure and
  tells you what is actually on disk. Trust it.
- **Injected nodes (from RunBehavior subtrees) are read-only everywhere.** Edit the subtree asset.
- **Comments**: `set_node_comment` writes the graph comment bubble — annotation only, survives
  GetTree→BuildTree round-trips. Use it to record WHY a subtree exists.

## Round-trips, repair, safety

- `get_tree` → edit the JSON → `build_tree(target, json, bReplaceExisting)` replays it through
  the ordinary mutators. One save per node/property — fine for tens of nodes, not a bulk-import
  path for thousands. With `bReplaceExisting=true` the result's `PreReplaceSnapshot` holds the
  target's previous tree; if the build fails midway, feed that snapshot back to `build_tree` to
  restore.
- Composite (logic-operator) decorators cannot be recreated by BuildTree (editor-only sub-graphs;
  reported per-node) — author those in the BT editor.
- Writes are refused while PIE runs, while an editor is open on the asset, and when the commit
  would destroy or crash on the runtime tree (sparse graph / instance-less root). Refusals name
  the mechanism and the fix; read them.
- `repair_graph_from_runtime_tree` rebuilds a sparse editor graph from the runtime tree —
  explicit, never implicit. `validate_tree` is the read-only diagnostic sweep (orphans, broken
  key bindings at any nesting depth, empty composites, misplaced sub-nodes).

## Blackboards beyond key CRUD

- `set_blackboard_key_instance_synced`, `set_blackboard_key_metadata` (category + description),
  `set_blackboard_parent` (cycle- and shadowing-checked) — all post-creation editable.
- `find_blackboard_key_references(board, key)` lists every BT selector bound to a key, formatted
  `<btPath>:<node>.<property>` — run it BEFORE `remove_blackboard_key` or
  `rename_blackboard_key`; rename returns the same list after the fact (those bindings broke).
- `SelfActor` on a parent-less board is engine-persistent and cannot be removed.
- Blueprint node classes (`BTT_*`, `BTD_*`, `BTS_*`) are created with the **blueprints** skill
  (parent class `BTTask_BlueprintBase` etc.); `get_available_node_types` lists them once they
  exist, and `add_node` resolves them by short name, `_C` name, or full object path.

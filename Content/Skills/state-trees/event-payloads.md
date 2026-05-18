---
name: event-payloads
description: Send a StateTree event with a struct payload from a Blueprint (Make Struct -> Make Instanced Struct -> Make State Tree Event -> Send State Tree Event)
---

This sub-doc continues from skill.md → "Sending a StateTree Event with a Struct Payload from Blueprint".

## Sending a StateTree Event with a Struct Payload from Blueprint

When an actor Blueprint needs to send a StateTree event that carries data (e.g. a target pawn,
position, or any custom struct), use this three-node chain:

```
Make <FMyStruct>  →  Make Instanced Struct  →  Make State Tree Event  →  Send State Tree Event
```

**Always load `blueprint-graphs`** before writing this code — the node types (`make_struct`,
`instanced_struct`) are documented there.

### Why Instanced Struct?

`Make State Tree Event.Payload` expects `FInstancedStruct`, not a raw struct. `Make Instanced Struct`
wraps any struct into `FInstancedStruct`. The struct type must be set at node creation time so the
`Value` wildcard pin resolves correctly — **do not try to connect `Value` before the struct type
is configured**.

### Pattern (using `build_graph`)

```python
import unreal

bp_path = "/Game/StateTree/BP_Cube.BP_Cube"
graph = "EventGraph"

# Read existing node IDs first (Set TargetPawn, Get StateTree, Make State Tree Event, etc.)
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, graph)
set_pawn_id    = next(n.node_id for n in nodes if n.node_title == "Set TargetPawn" and ...)
make_event_id  = next(n.node_id for n in nodes if n.node_title == "Make State Tree Event")
send_event_id  = next(n.node_id for n in nodes if "Send State Tree Event" in n.node_title)

result = unreal.BlueprintService.build_graph(
    bp_path, graph,
    [
        {"ref": "MkPayload", "type": "make_struct",     "params": {"struct": "FMyPayload"}},
        {"ref": "MkInst",    "type": "instanced_struct","params": {"struct": "FMyPayload"}},
    ],
    [
        # Rewire execution: SetPawn.then → MkPayload → MkInst → SendEvent
        # (disconnect old SetPawn.then → SendEvent first if needed)
        {"from_": "MkPayload.MyPayload",  "to": "MkInst.Value"},
        {"from_": "MkInst.ReturnValue",   "to": f"{make_event_id}.Payload"},
    ],
    [],
    True, True
)
```

### Pin names to verify

| Node | Key pins |
|------|----------|
| `Make <FMyPayload>` (`make_struct`) | Output: struct type name (check with `get_node_pins()`) |
| `Make Instanced Struct` (`instanced_struct`) | Input: `Value`; Output: `ReturnValue` |
| `Make State Tree Event` | Inputs: `Tag` (FGameplayTag), `Payload` (FInstancedStruct), `Origin` (FName) |
| `Send State Tree Event` | Inputs: `execute`, `self` (StateTreeComponent), `Event` (FStateTreeEvent) |

### ⚠️ Common Mistakes

- Connecting `Cast.AsPawn → MakeInst.Value.TargetPawn` directly — **fails**. You must go through
  `Make <FMyPayload>` first to populate the struct fields, then feed the struct into `MakeInst.Value`.
- Forgetting to disconnect the old `SetPawn.then → SendEvent.execute` wire before inserting the
  new nodes in between.
- Not passing the `struct` param to `instanced_struct` — the `Value` pin stays a wildcard and
  connections will fail at compile time.

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

### Pattern (using the Graph DSL)

`BlueprintService.build_graph` and `get_nodes_in_graph` were removed. Graph authoring is
`BlueprintToolsets.BlueprintDslToolset`, which reads and writes a whole scope as DSL text rather
than taking a node/connection list. The workflow:

```
# 0. Once per session: learn the grammar, and this node type's pins.
call_tool Docs   {}
call_tool Schema {"assetPath": "/Game/StateTree/BP_Cube.BP_Cube", "nodeType": "Make Instanced Struct"}

# 1. Read the graph you are about to change, and KEEP the Revision.
call_tool Read   {"assetPath": "/Game/StateTree/BP_Cube.BP_Cube", "scope": "EventGraph"}

# 2. Add the two nodes and rewire, as exact replacements against the text you just read.
call_tool Edit   {"assetPath": "/Game/StateTree/BP_Cube.BP_Cube", "scope": "EventGraph",
                  "revision": "<Revision from Read>", "edits": [ ... ]}
```

Use `Write` instead of `Edit` for a complete document or a large structural change; `Write` is
upsert, so an omitted top-level entry stays put (and warns), and an entry is deleted with a
tombstone such as ``(event `Foo` :!delete)``. A stale `Revision` is refused -- `Read` again. Both
are transactional: a failure leaves the asset unchanged.

The ordering constraint above still holds, and the DSL does not relax it: the struct type has to be
set on `Make Instanced Struct` in the same write that connects its `Value` pin, or the wildcard has
not resolved yet. `Schema` is how you confirm the pin set a given type/property combination
produces -- it reports which properties `determine` the pins.

### Pin names to verify

| Node | Key pins |
|------|----------|
| `Make <FMyPayload>` | Output: struct type name (confirm with `BlueprintDslToolset.Schema`) |
| `Make Instanced Struct` | Input: `Value`; Output: `ReturnValue` |
| `Make State Tree Event` | Inputs: `Tag` (FGameplayTag), `Payload` (FInstancedStruct), `Origin` (FName) |
| `Send State Tree Event` | Inputs: `execute`, `self` (StateTreeComponent), `Event` (FStateTreeEvent) |

### ⚠️ Common Mistakes

- Connecting `Cast.AsPawn → MakeInst.Value.TargetPawn` directly — **fails**. You must go through
  `Make <FMyPayload>` first to populate the struct fields, then feed the struct into `MakeInst.Value`.
- Forgetting to disconnect the old `SetPawn.then → SendEvent.execute` wire before inserting the
  new nodes in between.
- Not passing the `struct` param to `instanced_struct` — the `Value` pin stays a wildcard and
  connections will fail at compile time.

### Reading the payload fields on the StateTree side

Both payload tools are **VibeUE survivors** — `unreal.StateTreeService` calls inside
`execute_python`. Inspect the fields instead of guessing the path, then bind:

```python
payload_props = unreal.StateTreeService.get_transition_event_payload_property_names(st_path, "Root", 0)
for p in payload_props:
    print(f"{p.name}: {p.type} = {p.current_value!r}")

unreal.StateTreeService.bind_transition_condition_property_to_event_payload(
    st_path, "Root", 0, "StateTreeObjectIsValidCondition", "Object", "TargetPawn")
```

## Tag-only events, and sending from Python (live-verified 2026-07-03)

- `FGameplayTag` **cannot be constructed from Python** (TagName is read-only; the constructor
  routes to MakeLiteralGameplayTag which takes a tag, not a name). To send a tagged StateTree
  event from a script, author a one-shot Blueprint chain instead: custom event ->
  `make_struct` (`/Script/StateTreeModule.StateTreeEvent`, full path required) -> set the `Tag`
  pin default to `(TagName="My.Tag")` -> `SendStateTreeEvent` on the StateTree component — then
  trigger it with `controller_or_actor.call_method("MyCustomEvent")`.
- An `OnEvent` transition's tag must be a **registered** gameplay tag before writing it to the
  transition's `RequiredEvent` sticks — otherwise the compile keeps failing with
  "On Event Transition requires at least tag or payload". Register via the engine
  `GameplayTagsToolset.AddTag` (`tagName`, `tagSource="DefaultGameplayTags.ini"`).
- `update_transition` is gone. Changing a transition's trigger, tag or payload is now one
  `ObjectToolset.SetProperties` (`toolset_name="EditorToolset.ObjectToolset"`, via `call_tool`)
  that edits **that element's fields in place** inside the state's
  `Transitions` array. Do not drop-and-re-add the element (it mints a new ID and loses the
  element's conditions) and never rewrite the whole array (it regenerates every ID and orphans
  every binding on the state) — see the two transition rules in `SKILL.md`.

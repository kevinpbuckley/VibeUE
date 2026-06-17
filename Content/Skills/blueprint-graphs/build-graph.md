---
name: build-graph
description: Batch Blueprint graph builder (build_graph) - when to use, node types, connection format, examples, round-trip export/rebuild, auto-layout, Make Struct/Instanced Struct, error handling
---

This sub-doc continues from skill.md → "Batch Graph Builder (`build_graph`)".

## Batch Graph Builder (`build_graph`)

### When to Use

Use `build_graph` when you need to create **3+ nodes with connections** in a single call. It is
significantly faster and less error-prone than creating nodes one-at-a-time for complex graphs.

Use the individual `add_*_node` + `connect_nodes` methods when:
- You need to create 1-2 nodes
- You need to inspect pins between node creations
- The exact function name is uncertain (use `discover_nodes` first)

### Node Types

| Type | Required Params | Description |
|------|----------------|-------------|
| `function_call` | `class`, `function` | Calls a UFunction (e.g. `KismetSystemLibrary::PrintString`) |
| `spawner_key` | `key` | Creates node from a spawner key (FUNC, EVENT, NODE prefix) |
| `variable_get` | `variable` | Gets a Blueprint variable |
| `variable_set` | `variable` | Sets a Blueprint variable |
| `event` | `event` | Overridable event (e.g. `ReceiveBeginPlay`) |
| `custom_event` | `name` | Custom event node |
| `branch` | *(none)* | If/Then/Else branch |
| `cast` | `target_class` | Dynamic cast |
| `print_string` | *(none)* | PrintString shorthand |
| `input_action` | `action` | Enhanced Input Action (asset path) |
| `math` | `operation`, `operand_type` | Math op (Add/Subtract/Multiply/Divide/Clamp/Abs) |
| `comparison` | `operation`, `operand_type` | Comparison (Greater/Less/Equal/NotEqual/GreaterEqual/LessEqual) |
| `delegate_bind` | `delegate`, (optional: `component`) | Bind a multicast delegate |
| `create_event` | `function` | Create Event node |
| `validated_get` | `variable` | Validated Get (with exec pins) |
| `member_get` | `member`, `class` | Get member from another class |
| `create_delegate` | `function` | Create Delegate node |
| `make_struct` | `struct` | Make Struct node (`K2Node_MakeStruct`) for any struct type (engine or user-defined) |
| `instanced_struct` | `struct` | Make Instanced Struct node — wraps a struct into `FInstancedStruct` |

> **Standard Macro nodes (ForEachLoop, ForLoop, WhileLoop, DoOnce, Gate, IsValid, FlipFlop, …) are NOT placeable here.** They are `K2Node_MacroInstance` with no spawner key, so neither `spawner_key` nor `function_call` will create them. Place them separately with `add_macro_instance_node(bp, graph, "ForEachLoop", x, y)`, then wire them via the normal connection format. See the macro section in [SKILL.md](SKILL.md).

### Connection Format

Connections use `"RefOrGUID.PinName"` format: `{"from_": "A.then", "to": "B.execute"}`

**⚠️ Key name is `from_` (with underscore)** because `from` is a Python reserved keyword.
UE Python maps the C++ UPROPERTY `From` to `from_`.

The ref part can be either:
- A **local ref** from the `Nodes` array (e.g. `"MkInst"`)
- An **existing node GUID** already in the graph (32-char hex string from `get_nodes_in_graph()`)

This allows `build_graph` to wire new nodes to existing nodes in a single call:

```python
# Mix local refs with existing GUIDs
existing_id = "EB9221E84DFFF609028F9DAA6267B654"  # from get_nodes_in_graph()
connections = [
    {"from_": f"{existing_id}.OutputPin", "to": "NewNode.InputPin"},  # existing → new
    {"from_": "NewNode.OutputPin", "to": "OtherNew.InputPin"},        # new → new
]
```

Pin aliases supported:
- `execute` / `exec` → first exec input pin
- `then` / `output` → first exec output pin
- `value` / `result` → first non-exec output pin
- `True` → `then`, `False` → `else` (Branch node)

### Example: BeginPlay → PrintString

```python
import unreal

bp_path = "/Game/BP_MyActor"

result = unreal.BlueprintService.build_graph(
    bp_path,
    "EventGraph",
    # Nodes
    [
        {"ref": "BP", "type": "event", "params": {"event": "ReceiveBeginPlay"}},
        {"ref": "Print", "type": "print_string", "params": {}},
    ],
    # Connections
    [
        {"from_": "BP.then", "to": "Print.execute"},
    ],
    # Pin defaults
    [
        {"node_ref": "Print", "pin_name": "InString", "value": "Hello World!"},
    ],
    True,  # auto-layout
    True   # compile after
)

print(f"Success: {result.b_success}")
print(f"Nodes: {result.nodes_created}/{result.nodes_created + result.nodes_failed}")
print(f"Connections: {result.connections_made}/{result.connections_made + result.connections_failed}")
if result.errors:
    for e in result.errors:
        print(f"  ERROR: {e}")
```

### Example: Branch with Math

```python
result = unreal.BlueprintService.build_graph(
    bp_path,
    "EventGraph",
    [
        {"ref": "BP", "type": "event", "params": {"event": "ReceiveBeginPlay"}},
        {"ref": "GetHP", "type": "variable_get", "params": {"variable": "Health"}},
        {"ref": "Cmp", "type": "comparison", "params": {"operation": "Less", "operand_type": "Double"}},
        {"ref": "Branch", "type": "branch", "params": {}},
        {"ref": "PrintLow", "type": "print_string", "params": {}},
        {"ref": "PrintOK", "type": "print_string", "params": {}},
    ],
    [
        {"from_": "BP.then", "to": "Branch.execute"},
        {"from_": "GetHP.Health", "to": "Cmp.A"},
        {"from_": "Cmp.ReturnValue", "to": "Branch.Condition"},
        {"from_": "Branch.then", "to": "PrintLow.execute"},
        {"from_": "Branch.else", "to": "PrintOK.execute"},
    ],
    [
        {"node_ref": "Cmp", "pin_name": "B", "value": "25.0"},
        {"node_ref": "PrintLow", "pin_name": "InString", "value": "Health is low!"},
        {"node_ref": "PrintOK", "pin_name": "InString", "value": "Health OK"},
    ],
    True, True
)
```

### StateTreeDelegate Broadcast — Replace Finish Task with Broadcast Delegate

When a StateTree task Blueprint has a `StateTreeDelegate` variable (e.g. `FinishRotatingDispatcher`) and the user wants the custom event timer callback to **broadcast** that delegate instead of calling `Finish Task`, the graph must:

1. Disconnect `CustomEvent.then → Finish Task.execute`
2. Delete the `Finish Task` node
3. Discover the `Broadcast Delegate` spawner key (search `"Broadcast"`)
4. Create a `Broadcast Delegate` node + a variable GET for the delegate
5. Connect `CustomEvent.then → Broadcast.execute` and `GET.FinishRotatingDispatcher → Broadcast.Dispatcher`

```python
import unreal

bp_path = "/Game/StateTree/Tasks/STT_Rotate"
graph = "EventGraph"

# Step 1: Find node GUIDs
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, graph)
custom_node = next((n for n in nodes if "CustomEvent" in n.node_type or "Custom Event" in n.node_title), None)
finish_node = next((n for n in nodes if "Finish Task" in n.node_title), None)
assert custom_node and finish_node, "Nodes not found"

# Step 2: Disconnect CustomEvent.then (correct: 4 args)
unreal.BlueprintService.disconnect_pin(bp_path, graph, custom_node.node_id, "then")

# Step 3: Delete Finish Task
unreal.BlueprintService.delete_node(bp_path, graph, finish_node.node_id)

# Step 4: Discover Broadcast Delegate node — search "Broadcast" NOT "Dispatcher"
matches = unreal.BlueprintService.discover_nodes(bp_path, "Broadcast", "", 10)
for m in matches:
    print(m.display_name, m.spawner_key)  # display_name, NOT node_title

broadcast_node = next((m for m in matches if "Broadcast" in m.display_name), None)
assert broadcast_node, "Broadcast Delegate node not found"

# Step 5: Build replacement — Broadcast + GET variable
result = unreal.BlueprintService.build_graph(
    bp_path, graph,
    [
        {"ref": "Broadcast", "type": "spawner_key", "params": {"key": broadcast_node.spawner_key}},
        {"ref": "GetDisp", "type": "variable_get", "params": {"variable": "FinishRotatingDispatcher"}},
    ],
    [
        {"from_": f"{custom_node.node_id}.then", "to": "Broadcast.execute"},
        {"from_": "GetDisp.FinishRotatingDispatcher", "to": "Broadcast.Dispatcher"},
    ],
    [], True, True
)
print(f"Success: {result.b_success}, errors: {result.errors}")
```

**Target (self)** on the Broadcast node connects to `self` by default when the Blueprint is the correct class (`StateTreeTaskBlueprintBase`). You do not need to wire it manually.

### Subscribe to an Event Dispatcher on Another Blueprint (Bind Event)

The inverse of broadcast: a StateTree task Blueprint wants to **subscribe** to an event dispatcher (multicast delegate) declared on another Blueprint exposed via an Input variable (e.g. `Cube : BP_Cube_C` with dispatcher `FinishedLooking`), then call `Finish Task` when it fires.

**Preferred — `add_delegate_bind_on_variable` (one call, no class string).**
Mirrors `add_function_call_on_variable`: it reads the variable's type, finds the delegate on that class, creates the bind node + variable Get, and wires the Target (self) pin for you.

```python
import unreal

bp = "/Game/StateTree/Tasks/STT_LookInRandomDirection"
g  = "EventGraph"

# Existing node GUIDs (from get_nodes_in_graph)
look_at_id = "<Look At node GUID>"

# One-shot: derives BP_Cube_C from the Cube variable's type, creates bind + Get,
# wires Get -> Target. Returns the bind node GUID.
bind_id = unreal.BlueprintService.add_delegate_bind_on_variable(
    bp, g, "Cube", "FinishedLooking", 960.0, 0.0)

# Add the Custom Event + FinishTask call and wire the remaining pins.
custom_id = unreal.BlueprintService.add_custom_event_node(bp, g, "OnFinishedLooking", 960.0, 240.0)
finish_id = unreal.BlueprintService.add_function_call_node(
    bp, g, "StateTreeTaskBlueprintBase", "FinishTask", 1480.0, 240.0)

# 1) upstream exec -> bind
unreal.BlueprintService.connect_nodes(bp, g, look_at_id, "then",       bind_id,   "execute")
# 2) custom event's delegate output -> bind's Delegate pin
unreal.BlueprintService.connect_nodes(bp, g, custom_id,  "OutputDelegate", bind_id, "Delegate")
# 3) custom event fires -> FinishTask
unreal.BlueprintService.connect_nodes(bp, g, custom_id,  "then",       finish_id, "execute")

unreal.BlueprintService.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_asset(bp)
```

**Pin names on `K2Node_AddDelegate` (the bind node):**
- exec in: `execute`
- exec out: `then`
- Target: `self` (object input — already wired for you by `add_delegate_bind_on_variable`)
- Delegate: `Delegate` (input — wire a Custom Event's `OutputDelegate` here)

**Lower-level alternative — `add_delegate_bind_node`** (when you don't have a variable to derive the class from, e.g. you want to bind in code without a member variable). `target_class` resolution accepts any of:
- `"Self"` / `""` — the current Blueprint
- Native class with or without prefix: `"Actor"`, `"AActor"`, `"UButton"`
- Blueprint asset path: `"/Game/StateTree/BP_Cube"` (or `.BP_Cube_C`)
- Short Blueprint name with or without `_C`: `"BP_Cube"`, `"BP_Cube_C"`

You also need to compose the variable Get and wire Target yourself when using this primitive.

### Round-Trip: Export → Modify → Rebuild

Use `get_graph_definition` to capture an existing graph, modify the definition, then
rebuild with `build_graph`:

```python
import unreal

bp_path = "/Game/BP_MyActor"
graph = "EventGraph"

# Export current graph
nodes, connections, defaults, error = unreal.BlueprintService.get_graph_definition(
    bp_path, graph)

# Inspect exported nodes
for n in nodes:
    print(f"  {n.ref}: {n.type} {n.params}")

# Modify (add a new node, change a connection, etc.)
# Then rebuild in a different graph or blueprint
```

### Auto-Layout (`auto_layout_graph` / `auto_layout_selected_nodes`)

Both methods use the same layered (Sugiyama-style) algorithm:
- **Columns by dependency depth** — layer = longest path over the combined exec **and** data edge graph, so inputs sit to the left and flow rightward into the exec/Set nodes that consume them (a deep `Get → math → Clamp → Set` chain steps across columns instead of stacking in one).
- **Cycle-safe** — back-edges (a loop body wired back to its loop, recursion, a Reset re-entry) are detected via DFS and excluded from column assignment, so a cycle can never inflate graph width. The back-edge is still drawn as a wire. DFS is seeded from event entry points so the dropped edge is the genuine loop-back.
- **Crossing reduction** — median ordering sweeps; fan-out siblings (a branch's then/else, a sequence's outputs, a loop's body/completed) keep their output-pin order top-to-bottom.
- **Straight backbones** — Y aligns each node to the median center of its neighbors, weighted toward **exec** neighbors, so the execution spine stays horizontal while data feeds in from the side. A branch sits at the midpoint of its then/else targets.
- **Independent components** get separate non-overlapping vertical bands; rows are spaced by node size (pin count).
- **Idempotent** — running it twice moves nothing. Column width 420 px.

**`auto_layout_graph`** — repositions **every** node in the graph:

```python
unreal.BlueprintService.auto_layout_graph(bp_path, "EventGraph")
```

**`auto_layout_selected_nodes`** — repositions **only the listed nodes**; everything else is untouched.
Adjacency (exec + data edges) is computed within the selection only, so the layout is self-contained.
The origin is anchored to the top-left corner of the selection's current bounding box.

```python
# Layout whatever is currently selected in the Blueprint Editor
selected = unreal.BlueprintService.get_selected_nodes(bp_path)
ids = [n.node_id for n in selected]
unreal.BlueprintService.auto_layout_selected_nodes(bp_path, "EventGraph", ids)

# Layout a hand-picked subset by GUID
unreal.BlueprintService.auto_layout_selected_nodes(
    bp_path, "EventGraph",
    ["GUID-A", "GUID-B", "GUID-C"])
```

### Make Struct and Make Instanced Struct

Use `make_struct` to create a `K2Node_MakeStruct` for any struct type (engine or user-defined).
Use `instanced_struct` to wrap a struct into `FInstancedStruct` — required when a pin expects `FInstancedStruct` (e.g. `Make State Tree Event.Payload`).

**⚠️ The `Value` pin on `Make Instanced Struct` is a wildcard.** It resolves to the struct's fields only after the struct type is set via `struct` param. Do **not** try to split or connect it before the node type is resolved — connecting by field name (e.g. `Value.TargetPawn`) will fail.

**Correct pattern — build_graph example:**

```python
result = unreal.BlueprintService.build_graph(
    bp_path,
    "EventGraph",
    [
        # 1. Make the struct with its fields populated
        {"ref": "MkPayload", "type": "make_struct", "params": {"struct": "FStartChasingPayload"}},
        # 2. Wrap it in FInstancedStruct
        {"ref": "MkInst", "type": "instanced_struct", "params": {"struct": "FStartChasingPayload"}},
    ],
    [
        # Wire the struct output into the instanced struct Value pin
        {"from_": "MkPayload.StartChasingPayload", "to": "MkInst.Value"},
        # Wire MkInst output into whatever consumes FInstancedStruct
        {"from_": "MkInst.ReturnValue", "to": "MkState.Payload"},
    ],
    [],
    True, True
)
```

**Pin names for `make_struct`:** The output pin name matches the struct type name exactly as Unreal exposes it (e.g. `StartChasingPayload` for `FStartChasingPayload`). If uncertain, create the node, then call `get_node_pins()` to read the actual output pin name before connecting.

**Pin names for `instanced_struct`:** Input is `Value`, output is `ReturnValue`.

### Error Handling

`build_graph` returns `FBuildGraphResult` with detailed audit:
- `b_success` — `True` only if zero node failures AND zero compile errors
- `nodes_created` / `nodes_failed` — individual node creation results
- `connections_made` / `connections_failed` — wiring results
- `defaults_set` / `defaults_failed` — pin default results
- `ref_to_node_id` — maps your local refs to engine GUIDs
- `errors` — critical failures (node not found, class not found, compile errors)
- `warnings` — non-fatal issues (pin mismatch, connection rejected)

Always check `result.errors` and `result.warnings` after a `build_graph` call.

---
name: blueprint-graphs
display_name: Blueprint Graph Editing
description: Add, connect, and configure nodes in Blueprint event graphs and function graphs
vibeue_classes:
  - BlueprintService
unreal_classes:
  - EditorAssetLibrary
keywords:
  - node
  - graph
  - connect
  - wire
  - pin
  - event
  - function call
  - timer
  - delay
  - custom event
  - branch
  - math
  - execute
related_skills:
  - blueprints
---

# Blueprint Graph Editing Skill

> **Also load `blueprints` skill** when creating new blueprints, adding variables/components, or overriding functions.
> This skill covers **node-level graph editing** — adding nodes, wiring pins, setting values, and layout.

## Critical Rules

### ⚠️ Method Name Gotchas

| WRONG | CORRECT |
|-------|---------|
| `list_nodes()` | `get_nodes_in_graph()` |
| `add_node()` | `add_function_call_node()` or `add_event_node()` etc. |
| `disconnect_nodes()` | `disconnect_pin()` |
| `get_node_connections()` | `get_connections()` |

### ⚠️ Property Name Gotchas

| WRONG | CORRECT |
|-------|---------|
| `node.node_name` | `node.node_title` |
| `node.node_position_x` | `node.pos_x` |
| `node.node_position_y` | `node.pos_y` |
| `pin.is_linked` | `pin.is_connected` |
| `pin.current_value` | `pin.default_value` |
| `pin.sub_pins` | *(does not exist)* |

### ⚠️ Branch Node Pin Names

Use **`then`** and **`else`**, NOT `true`/`false`:

```python
# WRONG
connect_nodes(path, func, branch_id, "true", target_id, "execute")

# CORRECT
connect_nodes(path, func, branch_id, "then", target_id, "execute")
connect_nodes(path, func, branch_id, "else", target_id, "execute")
```

### ⚠️ UE5.7 Uses Doubles for Math

| WRONG | CORRECT |
|-------|---------|
| `Greater_FloatFloat` | `Greater_DoubleDouble` |
| `Add_FloatFloat` | `Add_DoubleDouble` |

### ⚠️ Compile Before Using Variables in Nodes

```python
unreal.BlueprintService.add_variable(path, "Health", "float", "100.0")
unreal.BlueprintService.compile_blueprint(path)  # REQUIRED before adding nodes
unreal.BlueprintService.add_get_variable_node(path, func, "Health", x, y)
```

### ⚠️ Node IDs Are GUID Strings, Not Small Integers

Do **not** assume Blueprint nodes have numeric IDs like `0` or `1`.
`get_nodes_in_graph()` returns **GUID strings** in `node.node_id`.

```python
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, graph)
for node in nodes:
    print(node.node_id, node.node_title)
```

For function graphs, find nodes by `node_type` or `node_title`.
For event-style overrides, find nodes by the **display title Unreal shows in the graph**, not by the raw override function name.

Examples for StateTree task blueprints:

| Override created | Typical graph title |
|---|---|
| `ReceiveLatentEnterState` | `Event EnterState` |
| `ReceiveLatentTick` | `Event Tick` |
| `ReceiveStateCompleted` | `Event StateCompleted` |

### ⚠️ Delay vs Set Timer by Event

When a user asks for a "timer" or "timed delay" in a Blueprint, **use `Set Timer by Event`**, NOT `Delay`:

| Node | Behavior | Use When |
|------|----------|----------|
| `Delay` | **Latent action** — blocks execution flow on that path | Simple linear sequences where nothing else runs during the wait |
| `Set Timer by Event` | **Non-blocking** — fires a delegate callback after the duration | The Blueprint must keep ticking/running during the wait (e.g. State Tree tasks that rotate while waiting, animation tasks, any gameplay that shouldn't freeze) |

The pattern for `Set Timer by Event` requires a **Custom Event**:

Important: after `add_custom_event_node(...)`, immediately re-read the graph and re-find that callback by the returned `node_id`. Do **not** assume the title will be exactly the event name you passed in. Unreal can display custom event titles as multi-line labels such as `OnTimerFinished` + `Custom Event`.

Do **not** silently substitute a `Create Event` / `Create Delegate` node plus a separate function graph when the user asked for a `Custom Event` node or when the target graph should visibly contain the callback in `EventGraph`. A `Create Event` delegate can compile and still be the wrong implementation for the requested graph shape.

```python
import unreal

bp_path = "/Game/MyBlueprint"
graph = "EventGraph"

# 1. Add Set Timer by Event node
timer_id = unreal.BlueprintService.add_function_call_node(
    bp_path, graph, "KismetSystemLibrary", "K2_SetTimerDelegate", 300, 0)

# 2. Set the Time pin
unreal.BlueprintService.set_node_pin_value(bp_path, graph, timer_id, "Time", "1.0")

# 3. Add a Custom Event node — this is the callback
custom_event_id = unreal.BlueprintService.add_custom_event_node(
    bp_path, graph, "OnTimerFinished", 300, 300)

# 4. Connect the Custom Event's delegate output to the timer's Delegate pin
unreal.BlueprintService.connect_nodes(
    bp_path, graph, custom_event_id, "OutputDelegate",
    timer_id, "Delegate")

# 5. Wire: EnterState → Set Timer by Event
unreal.BlueprintService.connect_nodes(
    bp_path, graph, enter_state_id, "then", timer_id, "execute")

# 6. Wire: Custom Event → Finish Task (or whatever follows)
unreal.BlueprintService.connect_nodes(
    bp_path, graph, custom_event_id, "then", finish_id, "execute")
```

### ⚠️ StateTree Task Timer Workflow: Always Discover Real Titles and Pins First

For `STT_*` Blueprint tasks, do **not** guess event titles or pin names. The stable pattern is:

1. `override_function(bp, "ReceiveLatentEnterState")`
2. `get_nodes_in_graph(bp, "EventGraph")` and locate the event by title `Event EnterState`
3. After `add_custom_event_node(...)`, call `get_nodes_in_graph()` again and re-find the callback by the returned GUID
4. Treat the callback title as display-only evidence; custom event titles can be multi-line and should not be your primary lookup key
5. `get_node_pins()` on every newly created node
6. Wire using the **actual pin names returned by the graph**

Current UE 5.7 / VibeUE graph details for this workflow:

| Node | Pin to use |
|---|---|
| `Event EnterState` | `then` |
| `Custom Event` | `OutputDelegate`, `then` |
| `Set Timer by Event` | `execute`, `Delegate`, `Time`, `bLooping`, `bMaxOncePerFrame` |
| `Finish Task` | `execute`, `bSucceeded` |

Success for this workflow also requires the callback node in `EventGraph` to be a real custom event node, typically `K2Node_CustomEvent` in a fresh node listing. `K2Node_CreateDelegate` is a different node type and should not be treated as equivalent when the requested outcome is a visible custom event callback in the graph.

```python
import unreal

bp_path = "/Game/StateTree/STT_Rotate"
graph = "EventGraph"

unreal.BlueprintService.override_function(bp_path, "ReceiveLatentEnterState")

timer_id = unreal.BlueprintService.add_function_call_node(
    bp_path, graph, "KismetSystemLibrary", "K2_SetTimerDelegate", 520, 0)
custom_id = unreal.BlueprintService.add_custom_event_node(
    bp_path, graph, "OnTimerFinished", 0, 420)
finish_id = unreal.BlueprintService.add_function_call_node(
    bp_path, graph, "StateTreeTaskBlueprintBase", "FinishTask", 520, 420)

nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, graph)
enter_node = next((n for n in nodes if n.node_title == "Event EnterState"), None)
custom_node = next((n for n in nodes if n.node_id == custom_id), None)
assert enter_node, "Event EnterState not found"
assert custom_node, f"Custom event {custom_id} not found after create"

custom_pins = unreal.BlueprintService.get_node_pins(bp_path, graph, custom_node.node_id)
print("CUSTOM EVENT TITLE:", custom_node.node_title)
print("CUSTOM EVENT PINS:", [p.pin_name for p in custom_pins])

unreal.BlueprintService.set_node_pin_value(bp_path, graph, timer_id, "Time", "1.0")
unreal.BlueprintService.set_node_pin_value(bp_path, graph, timer_id, "bLooping", "false")
unreal.BlueprintService.set_node_pin_value(bp_path, graph, finish_id, "bSucceeded", "true")

assert unreal.BlueprintService.connect_nodes(bp_path, graph, enter_node.node_id, "then", timer_id, "execute")
assert unreal.BlueprintService.connect_nodes(bp_path, graph, custom_node.node_id, "OutputDelegate", timer_id, "Delegate")
assert unreal.BlueprintService.connect_nodes(bp_path, graph, custom_node.node_id, "then", finish_id, "execute")
```

Use `add_create_event_node()` only when you need a **Create Event / Create Delegate** node. For `Set Timer by Event`, a `Custom Event` node is the simpler callback source and matches the Blueprint editor workflow shown in screenshots.

If a previous attempt already inserted a `Create Event` node for this timer callback, treat that as the wrong graph shape when the request is for a custom event. Remove the wrong node, remove any stale callback function graph that only existed to support that delegate node, then create the real `Custom Event` node and verify it by node ID and node type.

### ⚠️ Verification Is Mandatory Before Claiming Success

After any graph edit, verify all three layers:

1. **Connections**: call `get_connections()` and confirm the exact expected wiring.
2. **Pins**: if a connection fails, call `get_node_pins()` and use the real pin names.
3. **Compile**: inspect `compile_blueprint(...).success`, `num_errors`, and `errors`.

For any node you claim you created, also re-read the graph with `get_nodes_in_graph()` and confirm that node actually exists in the graph after the edit. A returned node ID from a create call is not enough.

For `Custom Event` timer callbacks, verify both of these before wiring:

1. The returned GUID appears in a fresh `get_nodes_in_graph()` result.
2. `get_node_pins()` on that exact node shows the pins you intend to use, typically `OutputDelegate` and `then` for the callback path.

Also verify that the node type is the expected custom event form rather than `K2Node_CreateDelegate`.

```python
result = unreal.BlueprintService.compile_blueprint(bp_path)
assert result.success, result.errors

nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, graph)
for node in nodes:
    print(f"NODE {node.node_id} {node.node_title}")

connections = unreal.BlueprintService.get_connections(bp_path, graph)
for conn in connections:
    print(f"{conn.source_node_title}.{conn.source_pin_name} -> {conn.target_node_title}.{conn.target_pin_name}")

unreal.EditorAssetLibrary.save_asset(bp_path)
```

Never print `MODIFIED` or describe the graph as complete until the expected connections are present and compile succeeds.
Never describe a node as present until it appears in a fresh `get_nodes_in_graph()` result.

### Required Success Gate For Graph Edits

If you edit a graph, your success check must answer all of these from live asset data:

1. Which required node titles are present?
2. Which exact connection lines prove the intended wiring exists?
3. Did compile succeed with zero relevant errors?

If you cannot answer those three questions from tool output, the task is not complete yet.

**Common mistake**: Using `Delay` when the user says "timer". `Delay` is a latent action that
pauses the execution chain. `Set Timer by Event` is non-blocking and fires a separate event —
this is critical for State Tree tasks, animation blueprints, and any actor that must keep
ticking during the wait period.

### Accessing Members of Another Blueprint (`add_member_get_node`)

Use `add_member_get_node` to read a property or component that belongs to another class
(not the current Blueprint). This creates a getter node with a **Target** input pin.

```
Target (input) — the object reference (e.g. your BP_Cube variable output)
MemberName (output) — the property value (e.g. the CubeMesh component)
```

```python
# Get the CubeMesh component from a "Cube" variable of type BP_Cube
mesh_id = unreal.BlueprintService.add_member_get_node(
    bp_path, graph, "BP_Cube_C", "CubeMesh", 400, 0)

# Connect: Cube (from validated get) -> Target of the member getter
unreal.BlueprintService.connect_nodes(bp_path, graph, val_get_id, "Cube", mesh_id, "self")
# Output pin name matches the member name: "CubeMesh"
unreal.BlueprintService.connect_nodes(bp_path, graph, mesh_id, "CubeMesh", next_id, "Target")
```

The `TargetClass` must be the generated class name (`BP_Cube_C`, not `BP_Cube`). The function
resolves it via the same 3-step fallback as `create_blueprint`.

### Validated Get Nodes (`add_validated_get_node`)

Use `add_validated_get_node` to create a **Validated Get** — a variable getter with execution
pins that only continues on the valid path if the object reference is non-null.

Pin names produced:

| Pin | Name | Direction |
|-----|------|-----------|
| Execution in | `"execute"` | input |
| Is Valid (object non-null) | `"then"` | output exec |
| Is Not Valid (object null) | `"else"` | output exec |
| Variable data | variable name e.g. `"MyObject"` | output data |

```python
import unreal

bp_path = "/Game/BP_MyActor"
graph = "EventGraph"

# Compile first so the variable type is resolved
unreal.BlueprintService.compile_blueprint(bp_path)

# Add BeginPlay + validated get + some function call
begin_id = unreal.BlueprintService.add_event_node(bp_path, graph, "ReceiveBeginPlay", 0, 0)
val_get_id = unreal.BlueprintService.add_validated_get_node(bp_path, graph, "MyObject", 300, 0)
call_id = unreal.BlueprintService.add_function_call_node(bp_path, graph, "MyObject", "SomeFunction", 700, 0)

# Execution flow: BeginPlay -> ValidatedGet (Is Valid path) -> SomeFunction
unreal.BlueprintService.connect_nodes(bp_path, graph, begin_id, "then", val_get_id, "execute")
unreal.BlueprintService.connect_nodes(bp_path, graph, val_get_id, "then", call_id, "execute")

# Data flow: MyObject output -> function Target
unreal.BlueprintService.connect_nodes(bp_path, graph, val_get_id, "MyObject", call_id, "self")

unreal.BlueprintService.compile_blueprint(bp_path)
unreal.EditorAssetLibrary.save_asset(bp_path)
```

> Only Object/Actor reference variables support Validated Get (`ValidatedObject` variation).
> Primitive types (int, float, bool) produce a Branch-style impure get instead.

---

## Workflows

### Override a Parent Function (BlueprintImplementableEvent / BlueprintNativeEvent)

Use `list_overridable_functions` to discover what can be overridden, then `override_function`
to create the graph. After that, use `get_nodes_in_graph` + `set_node_pin_value` to wire in
return values.

```python
import unreal

bp_path = "/Game/StateTree/STT_Rotate"

# Step 1: See all overridable functions and which are already overridden
funcs = unreal.BlueprintService.list_overridable_functions(bp_path)
for f in funcs:
    status = "OVERRIDDEN" if f.already_overridden else "available"
    print(f"{f.function_name} ({f.owner_class}) [{status}] -> {f.return_type}")
# ⚠️ UE Python strips `b` prefix: use `already_overridden` and `is_native_event` (not `b_already_overridden`)

# Step 2: Create the override graph (idempotent — safe to call even if already exists)
unreal.BlueprintService.override_function(bp_path, "GetStaticDescription")

# Step 3: Inspect the new graph — find the result node
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, "GetStaticDescription")
result_node = next((n for n in nodes if "Result" in n.node_type), None)

# Step 4: Set the return value pin directly (for FText/FString returns use the pin name)
if result_node:
    unreal.BlueprintService.set_node_pin_value(bp_path, "GetStaticDescription", result_node.node_id, "ReturnValue", "Rotate Cube")

# Step 5: Compile and save
unreal.BlueprintService.compile_blueprint(bp_path)
unreal.EditorAssetLibrary.save_asset(bp_path)
```

#### ⚠️ Event-style vs Function-style overrides

`override_function` automatically picks the right approach based on `is_event_style`:

| `is_event_style` | Mechanism | Where to find the node after |
|---|---|---|
| `True` (void/latent: EnterState, Tick, StateCompleted…) | Event node added to **EventGraph** | `get_nodes_in_graph(bp, "EventGraph")` |
| `False` (returns a value: GetDescription…) | **Function graph** with entry + result | `get_nodes_in_graph(bp, function_name)` |

Never call `add_event_node` manually for override functions — `override_function` handles it.

For event-style overrides, the visible node title is usually the friendly event name Unreal shows in the graph, such as `Event EnterState`, not the raw function name like `ReceiveLatentEnterState`.

#### ⚠️ `list_overridable_functions` vs `list_functions`

| Method | What it returns |
|--------|----------------|
| `list_functions` | Functions **defined in this blueprint** (user-created + already overridden) |
| `list_overridable_functions` | Functions from **parent class** that are override-able, with `already_overridden` and `is_event_style` flags |

Always call `list_overridable_functions` when the user asks "what functions can I override?" or
"override the X function". Never guess function names — they are case-sensitive.

### Create Function with Logic

```python
import unreal

bp_path = "/Game/BP_Player"

# Create function with parameters
unreal.BlueprintService.create_function(bp_path, "TakeDamage", is_pure=False)
unreal.BlueprintService.add_function_input(bp_path, "TakeDamage", "Amount", "float", "0.0")
unreal.BlueprintService.add_function_output(bp_path, "TakeDamage", "NewHealth", "float")
unreal.BlueprintService.compile_blueprint(bp_path)

# Add nodes (entry=0, result=1)
get_health = unreal.BlueprintService.add_get_variable_node(bp_path, "TakeDamage", "Health", -400, -100)
subtract = unreal.BlueprintService.add_math_node(bp_path, "TakeDamage", "Subtract", "Float", -200, 0)
set_health = unreal.BlueprintService.add_set_variable_node(bp_path, "TakeDamage", "Health", 200, 0)

# Connect nodes
unreal.BlueprintService.connect_nodes(bp_path, "TakeDamage", 0, "then", set_health, "execute")
unreal.BlueprintService.compile_blueprint(bp_path)
unreal.EditorAssetLibrary.save_asset(bp_path)
```

### Add Enhanced Input Action Node

Use `add_input_action_node()` to create an Enhanced Input Action event node in a Blueprint:

```python
import unreal

bp_path = "/Game/BP_ThirdPersonCharacter"
ia_path = "/Game/Input/IA_Ragdoll"

# Create the Enhanced Input Action node
node_id = unreal.BlueprintService.add_input_action_node(bp_path, "EventGraph", ia_path, -800, 2500)

if node_id:
    # Connect to other nodes - Output pins are: Started, Ongoing, Triggered, Completed, Canceled
    set_physics = unreal.BlueprintService.add_function_call_node(bp_path, "EventGraph", "PrimitiveComponent", "SetSimulatePhysics", -400, 2500)
    unreal.BlueprintService.connect_nodes(bp_path, "EventGraph", node_id, "Started", set_physics, "execute")
    
    unreal.BlueprintService.compile_blueprint(bp_path)
    unreal.EditorAssetLibrary.save_asset(bp_path)
```

**Important**: The Input Action asset must exist first. Create it with `InputService.create_action()` if needed.

---

## Node Layout Best Practices

### Layout Constants

```python
GRID_H = 200   # Horizontal spacing
GRID_V = 150   # Vertical spacing
DATA_ROW = -150  # Data getters above execution
EXEC_ROW = 0     # Main execution row
```

### Execution Flow (Left to Right)

```python
# Entry (0,0) → Branch (200,0) → SetVar (400,0) → Return (800,0)
```

### Data Flow (Above Execution)

```python
# Getters at Y=-150, math at Y=-75, execution at Y=0
get_health = add_get_variable_node(bp_path, func, "Health", 200, -150)
subtract = add_math_node(bp_path, func, "Subtract", "Float", 200, -75)
branch = add_branch_node(bp_path, func, 200, 0)
```

### Branch Layout (True/False Paths)

```python
# True path: Y=0 (same row)
# False path: Y=150 (offset down)
set_armor = add_set_variable_node(bp_path, func, "Armor", 400, 0)    # True
set_health = add_set_variable_node(bp_path, func, "Health", 400, 150)  # False
```

### Reposition Entry/Result (CRITICAL)

Entry and Result nodes are stacked at (0,0) by default:

```python
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, func_name)
for node in nodes:
    if "FunctionEntry" in node.node_type:
        unreal.BlueprintService.set_node_position(bp_path, func_name, node.node_id, 0, 0)
    elif "FunctionResult" in node.node_type:
        unreal.BlueprintService.set_node_position(bp_path, func_name, node.node_id, 800, 0)
```

---

## Common Function Call Classes

For `add_function_call_node(path, graph, class, func, x, y)`:

- **KismetMathLibrary** — Math (Add_DoubleDouble, Multiply_DoubleDouble, Sin, Sqrt)
- **KismetSystemLibrary** — System (PrintString, Delay, K2_SetTimerDelegate)
- **KismetStringLibrary** — String (Concat_StrStr, MakeLiteralString, Contains)
- **GameplayStatics** — Game (GetPlayerController, SpawnActor)
- **Actor** — Actor (GetActorLocation, SetActorLocation)
- **PrimitiveComponent** — Physics (SetSimulatePhysics)
- **SceneComponent** — Transform (AddRelativeRotation, SetRelativeLocation)

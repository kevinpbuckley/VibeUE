---
name: workflows
description: Step-by-step Blueprint graph workflows - overriding parent functions, creating functions with logic, adding Enhanced Input Action nodes
---

This sub-doc continues from skill.md → "Workflows".

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

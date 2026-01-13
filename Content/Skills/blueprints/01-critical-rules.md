# Blueprint Critical Rules

---

## ⚠️ CRITICAL: Method Name Gotchas

The AI frequently guesses wrong method names. **ALWAYS use `discover_python_class` before guessing!**

### Common Wrong Method Guesses

| WRONG (AttributeError) | CORRECT |
|------------------------|---------|
| `add_function(path, name)` | `create_function(path, name)` |
| `set_node_property(...)` | `set_node_pin_value(path, graph, node_id, pin, value)` |
| `list_nodes(path, graph)` | `get_nodes_in_graph(path, graph)` |
| `list_graph_nodes(path, graph)` | `get_nodes_in_graph(path, graph)` |
| `get_blueprint_properties(path)` | `get_property(path, prop_name)` |
| `unreal.get_default_object(class)` | BLOCKED - use `get_property()` instead |

### Pattern: "create_" vs "add_"
- `create_function(path, name)` ✓ - Creates a new function
- `create_blueprint(name, parent, folder)` ✓ - Creates a new blueprint
- BUT: `add_variable(path, name, type, default)` ✓ - Adds variable to existing BP
- BUT: `add_function_parameter(...)` ✓ - Adds param to existing function

### Pattern: "list_" vs "get_"
- `list_variables(path)` ✓
- `list_functions(path)` ✓  
- `list_components(path)` ✓
- BUT: `get_nodes_in_graph(path, graph)` ✓ (NOT `list_nodes`)

---

## ⚠️ CRITICAL: Property Name Gotchas

The AI frequently guesses wrong property names. Use the correct names from auto-discovery:

### Common Wrong Guesses

| WRONG | CORRECT |
|-------|---------|
| `info.inputs` | `info.input_parameters` |
| `node.node_name` | `node.node_title` |
| `pin.is_linked` | `pin.is_connected` |
| `pin.direction` | `pin.is_input` (bool: True=input, False=output) |
| `var.name` | `var.variable_name` |
| `param.pin_name` | `param.parameter_name` |

---

## ⚠️ CRITICAL: Branch Node Pin Names

Branch nodes (`K2Node_IfThenElse`) use **`then`** and **`else`** for output pins, NOT `true`/`false`:

```python
# WRONG - will silently fail to connect
connect_nodes(path, func, branch_id, "true", target_id, "execute")
connect_nodes(path, func, branch_id, "false", target_id, "execute")

# CORRECT - use "then" and "else"
connect_nodes(path, func, branch_id, "then", target_id, "execute")   # True path
connect_nodes(path, func, branch_id, "else", target_id, "execute")   # False path
```

**Always verify connections after wiring:**
```python
pins = unreal.BlueprintService.get_node_pins(path, func, branch_id)
for pin in pins:
    print(f"Pin '{pin.pin_name}': Connected={pin.is_connected}")
```

---

## ⚠️ CRITICAL: UE5.7 Uses Doubles for Math Functions

In UE 5.7, most math functions use **Double** not Float:

| WRONG (returns empty ID) | CORRECT |
|--------------------------|---------|
| `Greater_FloatFloat` | `Greater_DoubleDouble` |
| `Less_FloatFloat` | `Less_DoubleDouble` |
| `Add_FloatFloat` | `Add_DoubleDouble` |
| `Subtract_FloatFloat` | `Subtract_DoubleDouble` |
| `Multiply_FloatFloat` | `Multiply_DoubleDouble` |

```python
# WRONG in UE5.7 - returns empty string ID
id = add_function_call_node(path, func, "KismetMathLibrary", "Greater_FloatFloat", x, y)

# CORRECT for UE5.7
id = add_function_call_node(path, func, "KismetMathLibrary", "Greater_DoubleDouble", x, y)
```

---

## ⚠️ CRITICAL: Validate Connections After Wiring

`connect_nodes()` can return True but fail silently. **ALWAYS verify:**

```python
# After connecting nodes
success = unreal.BlueprintService.connect_nodes(path, func, src_id, "then", tgt_id, "execute")

# VERIFY the connection actually worked
pins = unreal.BlueprintService.get_node_pins(path, func, src_id)
then_pin = next((p for p in pins if p.pin_name == "then"), None)
if then_pin and not then_pin.is_connected:
    print("WARNING: Connection failed silently!")
```

---

## ⚠️ CRITICAL: No Generic `add_node` Method

There is NO `add_node(path, graph, "NodeType", x, y)` method.

**Use specific methods:**
- `add_branch_node(path, graph, x, y)`
- `add_math_node(path, graph, operation, type, x, y)`
- `add_comparison_node(path, graph, comparison, type, x, y)`
- `add_get_variable_node(path, graph, var_name, x, y)`
- `add_set_variable_node(path, graph, var_name, x, y)`
- `add_print_string_node(path, graph, x, y)`
- `add_function_call_node(path, graph, class, func, x, y)`

---

## ⚠️ CRITICAL: Method Name Word Order

```python
# CORRECT
add_get_variable_node(...)
add_set_variable_node(...)

# WRONG - AttributeError!
add_variable_get_node(...)
add_variable_set_node(...)
```

---

## ⚠️ CRITICAL: Compile Before Using Variables in Nodes

```python
# Add variable
unreal.BlueprintService.add_variable(path, "Health", "float", "100.0")

# MUST compile before adding Get/Set nodes
unreal.BlueprintService.compile_blueprint(path)

# NOW you can add variable nodes
unreal.BlueprintService.add_get_variable_node(path, func, "Health", x, y)
```

---

## ⚠️ CRITICAL: Check Before Creating

Always check if something exists before creating:

```python
# Check blueprint exists
existing = unreal.AssetDiscoveryService.find_asset_by_path("/Game/BP_Player")
if not existing:
    unreal.BlueprintService.create_blueprint("Player", "Character", "/Game/")

# Check variable exists (use .variable_name NOT .name)
vars = unreal.BlueprintService.list_variables(path)
if not any(v.variable_name == "Health" for v in vars):
    unreal.BlueprintService.add_variable(path, "Health", "float", "100.0")
```

---

## ⚠️ CRITICAL: Entry and Result Nodes Are STACKED by Default

When you create a function, Entry and Result nodes are created at the **same position (0,0)**.

**ALWAYS reposition them after creating a function:**

```python
# After creating function and adding logic, reposition Entry/Result
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, func_name)

for node in nodes:
    if "FunctionEntry" in node.node_type:
        # Entry stays at left (0, 0)
        unreal.BlueprintService.set_node_position(bp_path, func_name, node.node_id, 0, 0)
    elif "FunctionResult" in node.node_type:
        # Result goes to far right (800+)
        unreal.BlueprintService.set_node_position(bp_path, func_name, node.node_id, 800, 0)
```

**See `03-node-layout.md` for complete layout best practices.**

---

## ⚠️ Node ID 0 and 1 are Reserved

- **Entry Node** = ID 0 (function start)
- **Result Node** = ID 1 (function return)

```python
entry_node = 0  # No need to create
result_node = 1  # No need to create
# Custom nodes start at ID 2+
```

---

## ⚠️ Use Safe `next()` with Default

```python
# WRONG - crashes if no match
entry = next(n for n in nodes if n.node_type == "K2Node_FunctionEntry")

# CORRECT - returns None if no match
entry = next((n for n in nodes if n.node_type == "K2Node_FunctionEntry"), None)
if entry is None:
    print("Entry node not found!")
```

---

## ⚠️ Save After Making Changes

```python
unreal.BlueprintService.compile_blueprint(path)
unreal.EditorAssetLibrary.save_asset(path)
```

---

## ⚠️ Use Full Asset Paths

```python
# WRONG
path = "BP_Player"

# CORRECT
path = "/Game/Blueprints/BP_Player"
```

---

## Common Function Call Classes

For `add_function_call_node(path, graph, class, func, x, y)`:

- `KismetMathLibrary` - Math (Add, Multiply, Sin, etc.)
- `KismetSystemLibrary` - System (PrintString, Delay)
- `KismetStringLibrary` - String ops
- `KismetArrayLibrary` - Array ops
- `GameplayStatics` - Game (GetPlayerController, SpawnActor)
- `Actor` - Actor (GetActorLocation, SetActorLocation)

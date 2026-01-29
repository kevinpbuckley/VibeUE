---
name: blueprints
display_name: Blueprint System
description: Create and modify Blueprint assets, variables, functions, components, and node graphs
vibeue_classes:
  - BlueprintService
  - AssetDiscoveryService
unreal_classes:
  - EditorAssetLibrary
keywords:
  - blueprint
  - function
  - variable
  - node
  - graph
  - component
---

# Blueprint System Skill

## Critical Rules

### ⚠️ `create_blueprint()` Signature

```python
# CORRECT - Three separate arguments: name, parent, folder
unreal.BlueprintService.create_blueprint("BP_MyActor", "Actor", "/Game/Blueprints")

# WRONG - Path as first argument
unreal.BlueprintService.create_blueprint("/Game/Blueprints/BP_MyActor", "Actor")
```

**Returns**: Full asset path like `/Game/Blueprints/BP_MyActor.BP_MyActor`

### ⚠️ Method Name Gotchas

| WRONG | CORRECT |
|-------|---------|
| `add_function()` | `create_function()` |
| `list_nodes()` | `get_nodes_in_graph()` |
| `get_component_info(path, name)` | `get_component_info(type)` - takes ONLY type! |

### ⚠️ Property Name Gotchas

| WRONG | CORRECT |
|-------|---------|
| `info.inputs` | `info.input_parameters` |
| `node.node_name` | `node.node_title` |
| `pin.is_linked` | `pin.is_connected` |
| `var.name` | `var.variable_name` |

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

### ⚠️ Entry/Result Node IDs

- **Entry Node** = ID 0
- **Result Node** = ID 1
- Custom nodes start at ID 2+

---

## Workflows

### Create Blueprint with Variables

```python
import unreal

existing = unreal.AssetDiscoveryService.find_asset_by_path("/Game/BP_Player")
if not existing:
    path = unreal.BlueprintService.create_blueprint("Player", "Character", "/Game/")
    unreal.BlueprintService.add_variable(path, "Health", "float", "100.0")
    unreal.BlueprintService.add_variable(path, "IsAlive", "bool", "true")
    unreal.BlueprintService.compile_blueprint(path)
    unreal.EditorAssetLibrary.save_asset(path)
```

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

### Inspect Blueprint

```python
import unreal

bp_path = "/Game/BP_Player"

info = unreal.BlueprintService.get_blueprint_info(bp_path)
print(f"Parent: {info.parent_class}")

vars = unreal.BlueprintService.list_variables(bp_path)
for v in vars:
    print(f"Variable: {v.variable_name} ({v.variable_type})")

nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, "TakeDamage")
for node in nodes:
    print(f"Node {node.node_id}: {node.node_type}")
```

### Add Component with Properties

```python
import unreal

bp_path = "/Game/BP_Enemy"

unreal.BlueprintService.add_component(bp_path, "StaticMeshComponent", "BodyMesh")
unreal.BlueprintService.add_component(bp_path, "PointLightComponent", "Glow", "BodyMesh")  # Child
unreal.BlueprintService.compile_blueprint(bp_path)

unreal.BlueprintService.set_component_property(bp_path, "BodyMesh", "bVisible", "true")
unreal.BlueprintService.set_component_property(bp_path, "Glow", "Intensity", "5000.0")
unreal.EditorAssetLibrary.save_asset(bp_path)
```

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

- **KismetMathLibrary** - Math (Add, Multiply, Sin, Sqrt)
- **KismetSystemLibrary** - System (PrintString, Delay)
- **GameplayStatics** - Game (GetPlayerController, SpawnActor)
- **Actor** - Actor (GetActorLocation, SetActorLocation)

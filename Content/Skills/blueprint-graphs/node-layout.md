---
name: node-layout
description: Node layout best practices for Blueprint graphs - constants, execution flow, data flow, branch layout, repositioning Entry/Result nodes
---

This sub-doc continues from skill.md → "Node Layout Best Practices".

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

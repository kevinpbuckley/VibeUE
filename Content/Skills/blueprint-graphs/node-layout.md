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

Position is supplied per node. With `build_graph`, set `auto_layout=False` and place nodes with
explicit pixel coordinates in a follow-up `set_node_position` pass (build_graph node descriptors
don't carry coordinates); the intent is getters above the exec row:

```python
# Getters at Y=-150, math at Y=-75, execution at Y=0
# build_graph node types: variable_get @ (200,-150), math @ (200,-75), branch @ (200,0)
# Create them in one batch, then set_node_position each to the coordinates above.
```

### Branch Layout (True/False Paths)

```python
# True path: Y=0 (same row)
# False path: Y=150 (offset down)
# Two variable_set nodes (build_graph) — Armor @ (400,0) "True", Health @ (400,150) "False" —
# placed via set_node_position after creation.
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

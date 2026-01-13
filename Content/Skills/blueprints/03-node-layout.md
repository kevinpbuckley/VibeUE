# Blueprint Node Layout Best Practices

When creating nodes in Blueprint graphs, follow these layout conventions to produce clean, readable graphs.

---

## ⚠️ CRITICAL: Entry and Result Node Positions

**Entry node** and **Result node** are auto-created but often stacked at (0,0).

**YOU MUST reposition them using `set_node_position`:**
- **Entry node**: Left side at X=0, Y=0 (beginning of function)
- **Result node**: Right side at X=800+, Y=0 (end of function)

```python
import unreal

bp_path = "/Game/BP_Player"
func_name = "ApplyDamage"

# Get existing nodes to find Entry and Result
nodes = unreal.BlueprintService.get_nodes_in_graph(bp_path, func_name)

entry_id = None
result_id = None
for node in nodes:
    if "FunctionEntry" in node.node_type:
        entry_id = node.node_id
    elif "FunctionResult" in node.node_type:
        result_id = node.node_id

# Reposition Entry to left (0, 0) - usually already here but verify
if entry_id:
    unreal.BlueprintService.set_node_position(bp_path, func_name, entry_id, 0, 0)

# Reposition Result to right end (800, 0) - CRITICAL!
# The Result node is often stacked on Entry - move it to the end
if result_id:
    unreal.BlueprintService.set_node_position(bp_path, func_name, result_id, 800, 0)
```

**Why this matters:** Without repositioning, Entry and Result overlap, making the function unreadable.

---

## Grid Constants

Use these standard spacing values for consistent layouts:

```python
# Horizontal spacing between nodes
GRID_H = 200  # Standard horizontal gap

# Vertical spacing between branches/rows
GRID_V = 150  # Standard vertical gap

# Node widths (approximate)
NODE_WIDTH = 150  # Typical node width
WIDE_NODE = 250   # Nodes with many pins (Branch, Select, etc.)

# Entry/Result positions
ENTRY_X = 0       # Entry node at far left
RESULT_X = 800    # Result node at far right (adjust based on function complexity)
```

---

## Execution Flow Layout (Left to Right)

Arrange execution flow horizontally, left to right. Each step in the flow should be at increasing X positions:

```python
# Execution flow goes left to right
#   Entry → Branch → SetVar → Return
#   X=0     X=200    X=400    X=800

ENTRY_X = 0      # Entry node (auto-created, usually here)
COL_1 = 200      # First custom nodes
COL_2 = 400      # Second column
COL_3 = 600      # Third column
RESULT_X = 800   # Result node (MUST be repositioned to end!)

# All on same Y for main execution path
main_y = 0
```

Example:
```python
import unreal

bp_path = "/Game/BP_Player"
func_name = "ApplyDamage"

# Layout constants
GRID_H = 200
GRID_V = 150
START_X = 200  # Entry node is at 0, start custom nodes after
RESULT_X = 800 # Result node at the end

# Main execution row (Y=0)
branch_id = unreal.BlueprintService.add_branch_node(bp_path, func_name, START_X, 0)
set_var_id = unreal.BlueprintService.add_set_variable_node(bp_path, func_name, "Health", START_X + GRID_H, 0)
```

---

## Data Flow Layout (Above Main Row)

Place data nodes (getters, math operations) ABOVE the execution flow, feeding down into it:

```python
# Data flow feeds down into execution:
#
#   GetHealth  GetAmount     ← Y = -150 (data row)
#       ↓          ↓
#   [  Subtract  ]            ← Y = -75 (math row, if needed)
#       ↓
#   Entry → Branch → SetVar   ← Y = 0 (execution row)

# Data getters above execution
data_y = -150

# Math operations between data and execution
math_y = -75

# Main execution
exec_y = 0
```

Example:
```python
# Data row (Y = -GRID_V)
get_health = unreal.BlueprintService.add_get_variable_node(bp_path, func_name, "Health", 200, -150)
get_amount = unreal.BlueprintService.add_get_variable_node(bp_path, func_name, "DamageAmount", 400, -150)

# Math row (Y = -GRID_V/2)
subtract = unreal.BlueprintService.add_math_node(bp_path, func_name, "Subtract", "Float", 300, -75)

# Execution row (Y = 0)
branch = unreal.BlueprintService.add_branch_node(bp_path, func_name, 200, 0)
set_health = unreal.BlueprintService.add_set_variable_node(bp_path, func_name, "Health", 400, 0)
```

---

## Branch Layouts (True/False Paths)

When a Branch node splits execution, offset the True and False paths vertically:

```python
# Branch splits execution vertically:
#
#   Entry → Branch → (True Path)   Y = 0
#              ↓
#           (False Path)           Y = GRID_V

branch_x = 200
true_path_y = 0
false_path_y = 150  # GRID_V below

# True branch continues right at same Y
# False branch continues right at offset Y
```

Example:
```python
# Branch at (200, 0)
branch = unreal.BlueprintService.add_branch_node(bp_path, func_name, 200, 0)

# True path: subtract from armor (Y = 0)
set_armor = unreal.BlueprintService.add_set_variable_node(bp_path, func_name, "Armor", 400, 0)

# False path: subtract from health (Y = 150)  
set_health = unreal.BlueprintService.add_set_variable_node(bp_path, func_name, "Health", 400, 150)
```

---

## Complete Function Layout Example

Here's a complete example showing proper layout for a damage function:

```python
import unreal

bp_path = "/Game/Blueprints/BP_Player_Test"
func_name = "ApplyDamage"

# Layout constants
GRID_H = 200
GRID_V = 150
DATA_ROW = -150
MATH_ROW = -75
EXEC_ROW = 0
TRUE_BRANCH = 0
FALSE_BRANCH = 150

# Column positions (left to right flow)
COL_1 = 200   # First nodes after entry
COL_2 = 400   # Second column
COL_3 = 600   # Third column
COL_4 = 800   # Fourth column (typically return)

# ============================================
# ROW: Data getters (Y = -150)
# ============================================
get_damage = unreal.BlueprintService.add_get_variable_node(
    bp_path, func_name, "DamageAmount", COL_1, DATA_ROW)
get_armor = unreal.BlueprintService.add_get_variable_node(
    bp_path, func_name, "Armor", COL_2, DATA_ROW)
get_health = unreal.BlueprintService.add_get_variable_node(
    bp_path, func_name, "Health", COL_3, DATA_ROW)

# ============================================
# ROW: Math operations (Y = -75)
# ============================================
compare = unreal.BlueprintService.add_comparison_node(
    bp_path, func_name, "Greater", "Float", COL_1, MATH_ROW)
subtract = unreal.BlueprintService.add_math_node(
    bp_path, func_name, "Subtract", "Float", COL_2, MATH_ROW)

# ============================================
# ROW: Main execution (Y = 0)
# ============================================
# Entry node is at (0, 0) - ID 0
branch = unreal.BlueprintService.add_branch_node(
    bp_path, func_name, COL_1, EXEC_ROW)

# ============================================
# True path (Y = 0, continuing right)
# ============================================
set_armor = unreal.BlueprintService.add_set_variable_node(
    bp_path, func_name, "Armor", COL_2, TRUE_BRANCH)

# ============================================
# False path (Y = 150, offset down)
# ============================================
set_health = unreal.BlueprintService.add_set_variable_node(
    bp_path, func_name, "Health", COL_2, FALSE_BRANCH)

# ============================================
# Merge point / Return (rightmost column)
# Result node is ID 1
# ============================================
```

---

## Layout Checklist

When adding nodes to a function:

1. **Start with execution flow at Y=0**
   - Entry (0, 0) → First action (200, 0) → Next (400, 0) → Return

2. **Place data nodes above (negative Y)**
   - Variable getters at Y = -150
   - Math nodes at Y = -75 (between data and execution)

3. **Branch true/false paths vertically**
   - True path stays at current Y
   - False path offsets down by 150

4. **Maintain column alignment**
   - Use consistent X positions (200, 400, 600, 800)
   - Nodes in the same "step" share the same X

5. **Keep related nodes grouped**
   - Getter and its consumer should be vertically aligned
   - Math node positioned between its inputs and output user

---

## Visual Reference

```
Y = -150  [Get Health]  [Get Armor]  [Get Damage]     ← Data Row
              ↓              ↓            ↓
Y = -75                   [Subtract]  [Compare > 0]   ← Math Row
                              ↓            ↓
Y = 0     [Entry] ———→ [Branch] ———→ [Set Armor] ——→ [Return]   ← Exec Row (True)
                          ↓
Y = 150               [Set Health] ———————————————→ [Return]    ← False Branch

X =  0       200          400           600           800
         (COL_1)       (COL_2)       (COL_3)       (COL_4)
```

---

## Avoid These Layout Problems

### ❌ Random Positions
```python
# BAD: Nodes scattered without pattern
node1 = add_branch_node(path, func, 100, 50)
node2 = add_get_variable_node(path, func, 300, -20)
node3 = add_set_variable_node(path, func, 250, 180)
```

### ❌ All Nodes Same Position
```python
# BAD: Nodes stacked on top of each other
node1 = add_branch_node(path, func, 0, 0)
node2 = add_get_variable_node(path, func, 0, 0)
node3 = add_set_variable_node(path, func, 0, 0)
```

### ❌ Data Below Execution
```python
# BAD: Data flows up instead of down
exec_node = add_branch_node(path, func, 200, -100)
data_node = add_get_variable_node(path, func, 200, 100)  # Below!
```

### ✅ Correct Layout
```python
# GOOD: Data above, execution below, left-to-right flow
data_node = add_get_variable_node(path, func, 200, -150)  # Data row
exec_node = add_branch_node(path, func, 200, 0)           # Exec row
```
